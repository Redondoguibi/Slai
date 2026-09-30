"""Direct x86-64 encoding and PE32+ linking, using only the standard library.

RIP-relative references throughout; Windows x64 ABI, stack slots for SIR values.
The operating-system loader resolves DLL imports. No assembler or linker runs.
"""
from __future__ import annotations

import struct

from .frontend import SlaiError, Token
from .semantic import Value


RAX, RCX, RDX, RSP, RBP, R8, R9, R10, R11 = 0, 1, 2, 4, 5, 8, 9, 10, 11
ARGUMENTS = (RCX, RDX, R8, R9)


def align(n, boundary):
    return (n + boundary - 1) & -boundary


class Encoder:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []
        self.strings = {}
        self.blobs = {}
        self.rdata = bytearray()
        self.frames = []
        self.frame = 0
        self.serial = 0

    def emit(self, *data):
        for item in data:
            self.code.extend(bytes.fromhex(item) if isinstance(item, str) else item)

    def label(self, name):
        if name in self.labels:
            raise ValueError(f"Duplicate native label: {name}")
        self.labels[name] = len(self.code)

    def fresh(self):
        self.serial += 1
        return f"native{self.serial}"

    def ref(self, prefix, label):
        self.emit(prefix)
        self.fixups.append((len(self.code), label))
        self.emit(b"\0" * 4)

    def rex(self, r=0, b=0, w=True):
        self.emit(bytes([0x40 | (8 if w else 0) | ((r >> 3) << 2) | (b >> 3)]))

    def immediate(self, reg, value):
        self.rex(b=reg)
        self.emit(bytes([0xB8 + (reg & 7)]), struct.pack("<Q", int(value) & ((1 << 64) - 1)))

    def move(self, dst, src):
        self.rex(r=src, b=dst)
        self.emit(bytes([0x89, 0xC0 | ((src & 7) << 3) | (dst & 7)]))

    def memory(self, opcode, reg, base, displacement):
        self.rex(r=reg, b=base)
        self.emit(bytes([opcode, 0x80 | ((reg & 7) << 3) | (base & 7)]))
        if base & 7 == 4:
            self.emit("24")
        self.emit(struct.pack("<i", displacement))

    def load(self, reg, base, displacement=0):
        self.memory(0x8B, reg, base, displacement)

    def store(self, base, displacement, reg):
        self.memory(0x89, reg, base, displacement)

    def address(self, reg, label):
        self.rex(r=reg)
        self.ref(bytes([0x8D, ((reg & 7) << 3) | 5]), label)

    def load_value(self, reg, value):
        if value.slot < 0:
            self.rex(r=reg)
            self.ref(bytes([0x8B, ((reg & 7) << 3) | 5]), f"global{-value.slot}")
        else:
            self.load(reg, RBP, -8 * value.slot)

    def save(self, value, reg=RAX):
        if value.slot < 0:
            self.rex(r=reg)
            self.ref(bytes([0x89, ((reg & 7) << 3) | 5]), f"global{-value.slot}")
        else:
            self.store(RBP, -8 * value.slot, reg)

    def binary(self, opcode, dst, src):
        self.rex(r=src, b=dst)
        self.emit(bytes([opcode, 0xC0 | ((src & 7) << 3) | (dst & 7)]))

    def compare(self, a, b):
        self.binary(0x39, a, b)

    def test(self, reg):
        self.binary(0x85, reg, reg)

    def jump(self, label, condition=None):
        prefix = bytes([0x0F, condition]) if condition is not None else b"\xe9"
        self.ref(prefix, label)

    def call(self, label):
        self.ref("e8", label)

    def external(self, name):
        self.ref("ff15", f"import:{name}")

    def string(self, value):
        if value not in self.strings:
            label = f"str{len(self.strings)}"
            self.strings[value] = (label, len(self.rdata))
            self.rdata.extend(value.encode("utf-8") + b"\0")
        return self.strings[value][0]

    def blob(self, data):
        if data not in self.blobs:
            self.rdata.extend(b"\0" * (align(len(self.rdata), 8) - len(self.rdata)))
            self.blobs[data] = (f"blob{len(self.blobs)}", len(self.rdata))
            self.rdata.extend(data or b"\0")
        return self.blobs[data][0]

    def start(self, name, slots, arguments=0):
        # Outgoing argument space never overlaps locals; includes ABI shadow space.
        frame = align(slots * 8 + max(32, arguments * 8), 16)
        if frame > 4000:
            raise SlaiError(Token("", "", "<backend>", 1, 1),
                            "Frame maior que 4000 bytes; divida a função (stack probing ainda não implementado).", "ELIMIT")
        self.label(name)
        begin = len(self.code)
        self.emit("55 48 89 e5 48 81 ec", struct.pack("<I", frame))
        self.frame = frame
        self.frames.append([begin, None, frame])

    def epilogue(self):
        self.emit("48 81 c4", struct.pack("<I", self.frame), "5d c3")

    def end(self):
        self.frames[-1][1] = len(self.code)

    def arguments(self, values):
        for i, value in enumerate(values):
            reg = ARGUMENTS[i] if i < 4 else RAX
            self.load_value(reg, value)
            if i >= 4:
                self.store(RSP, 32 + (i - 4) * 8, reg)

    def panic_if(self, condition, message):
        safe = self.fresh()
        inverse = {0x84: 0x85, 0x85: 0x84, 0x8C: 0x8D, 0x8D: 0x8C,
                   0x83: 0x82, 0x82: 0x83, 0x8E: 0x8F, 0x8F: 0x8E}[condition]
        self.jump(safe, inverse)
        self.address(RCX, self.string(message))
        self.call("rt_panic")
        self.label(safe)

    def function(self, fn):
        maximum = max([len(ins.args[1]) for ins in fn.instructions if ins.op in ("call", "spawn")] + [4])
        self.start(fn.name, fn.slots, maximum)
        error_exit = self.fresh()
        for i, parameter in enumerate(fn.parameters):
            if i < 4:
                self.save(parameter, ARGUMENTS[i])
            else:
                self.load(RAX, RBP, 48 + (i - 4) * 8)
                self.save(parameter)
        for ins in fn.instructions:
            op, out, args = ins.op, ins.out, ins.args
            if op == "const":
                if out.type == "string":
                    self.address(RAX, self.string(args[0]))
                else:
                    self.immediate(RAX, args[0])
            elif op == "copy":
                self.load_value(RAX, args[0])
            elif op in ("neg", "not", "bool"):
                self.load_value(RAX, args[0])
                if op == "neg":
                    self.emit("48 f7 d8")
                else:
                    self.test(RAX)
                    self.emit("0f 94 c0" if op == "not" else "0f 95 c0", "48 0f b6 c0")
            elif op == "binary":
                operator, a, b = args
                self.load_value(RAX, a)
                self.load_value(R10, b)
                if operator == "+":
                    self.binary(0x01, RAX, R10)
                elif operator == "-":
                    self.binary(0x29, RAX, R10)
                elif operator == "*":
                    self.emit("49 0f af c2")
                else:
                    self.test(R10)
                    self.panic_if(0x84, "Slai runtime: divisao por zero")
                    safe = self.fresh()
                    self.immediate(R11, -(1 << 63))
                    self.compare(RAX, R11)
                    self.jump(safe, 0x85)
                    self.immediate(R11, -1)
                    self.compare(R10, R11)
                    self.panic_if(0x84, "Slai runtime: overflow na divisao int64")
                    self.label(safe)
                    self.emit("48 99 49 f7 fa")  # cqo; idiv r10
                    if operator == "%":
                        self.move(RAX, RDX)
            elif op == "compare":
                operator, a, b = args
                if a.type == "string":
                    self.arguments([a, b])
                    self.external("strcmp")
                    self.emit("48 63 c0")  # sign-extend C int return
                    self.immediate(R10, 0)
                else:
                    self.load_value(RAX, a)
                    self.load_value(R10, b)
                self.compare(RAX, R10)
                cc = {"==": 0x94, "!=": 0x95, "<": 0x9C, "<=": 0x9E, ">": 0x9F, ">=": 0x9D}[operator]
                self.emit(bytes([0x0F, cc, 0xC0]), "48 0f b6 c0")
            elif op == "label":
                self.label(args[0])
            elif op == "jump":
                self.jump(args[0])
            elif op == "branch":
                value, target, when_true = args
                self.load_value(RAX, value)
                self.test(RAX)
                self.jump(target, 0x85 if when_true else 0x84)
            elif op == "call":
                self.arguments(args[1])
                self.call(args[0])
            elif op == "spawn":
                self.arguments(args[1])
                self.call("spawn_" + args[0])
            elif op == "await":
                self.arguments(args)
                self.call("rt_await")
            elif op == "sleep":
                self.arguments(args)
                self.external("Sleep")
            elif op == "assert":
                self.load_value(RAX, args[0])
                self.test(RAX)
                passed = self.fresh()
                self.jump(passed, 0x85)
                self.address(RCX, self.string(args[1]))
                self.call("rt_error_set")
                self.label(passed)
            elif op == "error_set":
                self.arguments(args)
                self.call("rt_error_set")
            elif op == "error_take":
                self.call("rt_error_take")
            elif op == "ia_apply":
                self.call(f"ia_apply_{args[0]}")
            elif op == "error_check":
                self.call("rt_error_get")
                self.test(RAX)
                self.jump(args[0] or error_exit, 0x85)
            elif op == "error_jump":
                self.jump(args[0] or error_exit)
            elif op == "alloc":
                self.immediate(RCX, args[0])
                self.call("rt_alloc")
            elif op == "stack_alloc":
                self.memory(0x8D, RAX, RBP, -8 * args[0])
            elif op == "load_field":
                self.load_value(RAX, args[0])
                self.load(RAX, RAX, args[1])
            elif op == "store_field":
                self.load_value(RAX, args[0])
                self.load_value(R10, args[2])
                self.store(RAX, args[1], R10)
            elif op.startswith("list_"):
                self.arguments(args)
                self.call("rt_" + op)
            elif op == "log":
                value = args[0]
                if value.type == "bool":
                    self.load_value(RAX, value)
                    self.address(RDX, self.string("false"))
                    self.address(R10, self.string("true"))
                    self.test(RAX)
                    self.emit("49 0f 45 d2")  # cmovne rdx,r10
                    self.address(RCX, self.string("%s\n"))
                else:
                    self.load_value(RDX, value)
                    self.address(RCX, self.string("%s\n" if value.type == "string" else "%I64d\n"))
                self.external("printf")
            elif op == "return":
                self.load_value(RAX, args[0])
                self.epilogue()
            else:
                raise ValueError(f"Unknown SIR opcode: {op}")
            if out:
                self.save(out)
        self.label(error_exit)
        self.immediate(RAX, 0)
        self.epilogue()
        self.end()

    def runtime(self):
        self.start("entry", 0)
        self.external("TlsAlloc")
        self.immediate(R10, 0xFFFFFFFF)
        self.compare(RAX, R10)
        self.panic_if(0x84, "Slai runtime: TLS indisponivel")
        self.address(R10, "error_tls")
        self.store(R10, 0, RAX)
        self.immediate(RCX, 65001)
        self.external("SetConsoleOutputCP")
        self.call("main")
        self.call("rt_error_get")
        self.test(RAX)
        success = self.fresh()
        self.jump(success, 0x84)
        self.move(RCX, RAX)
        self.call("rt_panic")
        self.label(success)
        self.immediate(RCX, 0)
        self.external("fflush")
        self.immediate(RCX, 0)
        self.external("ExitProcess")
        self.emit("0f 0b")
        self.end()

        self.start("rt_error_get", 0)
        self.address(RCX, "error_tls")
        self.load(RCX, RCX)
        self.external("TlsGetValue")
        self.epilogue()
        self.end()

        self.start("rt_error_set", 0)
        self.move(RDX, RCX)
        self.address(RCX, "error_tls")
        self.load(RCX, RCX)
        self.external("TlsSetValue")
        self.epilogue()
        self.end()

        self.start("rt_error_take", 1)
        self.call("rt_error_get")
        self.store(RBP, -8, RAX)
        self.immediate(RCX, 0)
        self.call("rt_error_set")
        self.load(RAX, RBP, -8)
        self.epilogue()
        self.end()

        self.start("rt_await", 1)
        self.store(RBP, -8, RCX)
        self.load(RCX, RCX)
        self.test(RCX)
        ready = self.fresh()
        self.jump(ready, 0x84)
        self.immediate(RDX, 0xFFFFFFFF)
        self.external("WaitForSingleObject")
        self.test(RAX)
        self.panic_if(0x85, "Slai runtime: falha ao aguardar task")
        self.load(R10, RBP, -8)
        self.load(RCX, R10)
        self.external("CloseHandle")
        self.load(R10, RBP, -8)
        self.immediate(RAX, 0)
        self.store(R10, 0, RAX)
        self.label(ready)
        self.load(R10, RBP, -8)
        self.load(RCX, R10, 16)
        self.call("rt_error_set")
        self.load(R10, RBP, -8)
        self.load(RAX, R10, 8)
        self.epilogue()
        self.end()

        self.start("rt_panic", 0)
        self.move(RDX, RCX)
        self.address(RCX, self.string("%s\n"))
        self.external("printf")
        self.immediate(RCX, 0)
        self.external("fflush")
        self.immediate(RCX, 1)
        self.external("ExitProcess")
        self.emit("0f 0b")
        self.end()

        self.start("rt_alloc", 0)
        self.external("malloc")
        self.test(RAX)
        self.panic_if(0x84, "Slai runtime: memoria insuficiente")
        self.epilogue()
        self.end()

        # List descriptor: length, capacity, data pointer. Stable across realloc.
        self.start("rt_list_new", 1)
        self.immediate(RCX, 24)
        self.call("rt_alloc")
        self.store(RBP, -8, RAX)
        self.immediate(R10, 0)
        self.store(RAX, 0, R10)
        self.immediate(R10, 4)
        self.store(RAX, 8, R10)
        self.immediate(RCX, 32)
        self.call("rt_alloc")
        self.load(R10, RBP, -8)
        self.store(R10, 16, RAX)
        self.move(RAX, R10)
        self.epilogue()
        self.end()

        self.start("rt_list_push", 3)
        self.store(RBP, -8, RCX)
        self.store(RBP, -16, RDX)
        self.load(RAX, RCX, 0)
        self.load(R10, RCX, 8)
        self.compare(RAX, R10)
        room = self.fresh()
        self.jump(room, 0x8C)
        self.immediate(R11, (1 << 60) - 1)
        self.compare(R10, R11)
        self.panic_if(0x83, "Slai runtime: lista excedeu limite de memoria")
        self.binary(0x01, R10, R10)
        self.store(RBP, -24, R10)
        self.move(RDX, R10)
        self.emit("48 c1 e2 03")  # capacity * 8
        self.load(RCX, RCX, 16)
        self.external("realloc")
        self.test(RAX)
        self.panic_if(0x84, "Slai runtime: memoria insuficiente")
        self.load(RCX, RBP, -8)
        self.store(RCX, 16, RAX)
        self.load(R10, RBP, -24)
        self.store(RCX, 8, R10)
        self.label(room)
        self.load(RCX, RBP, -8)
        self.load(RAX, RCX, 0)
        self.load(R10, RCX, 16)
        self.load(RDX, RBP, -16)
        self.emit("49 89 14 c2")  # mov [r10+rax*8],rdx
        self.emit("48 ff c0")
        self.store(RCX, 0, RAX)
        self.epilogue()
        self.end()

        for name, write in (("rt_list_get", False), ("rt_list_set", True)):
            self.start(name, 0)
            self.test(RDX)
            self.panic_if(0x8C, "Slai runtime: indice de lista negativo")
            self.load(RAX, RCX, 0)
            self.compare(RDX, RAX)
            self.panic_if(0x83, "Slai runtime: indice fora da lista")
            self.load(R10, RCX, 16)
            self.emit("4d 89 04 d2" if write else "49 8b 04 d2")
            self.epilogue()
            self.end()

    def tasks(self, compiler):
        targets = {ins.args[0] for fn in compiler.functions for ins in fn.instructions if ins.op == "spawn"}
        for fn in compiler.functions:
            if fn.name not in targets:
                continue
            count = len(fn.parameters)
            self.start("spawn_" + fn.name, count + 1, max(6, count))
            for i in range(count):
                if i < 4:
                    self.store(RBP, -8 * (i + 1), ARGUMENTS[i])
                else:
                    self.load(RAX, RBP, 48 + (i - 4) * 8)
                    self.store(RBP, -8 * (i + 1), RAX)
            self.immediate(RCX, 24 + count * 8)
            self.call("rt_alloc")
            self.store(RBP, -8 * (count + 1), RAX)
            self.immediate(R10, 0)
            for offset in (0, 8, 16):
                self.store(RAX, offset, R10)
            for i in range(count):
                self.load(R10, RBP, -8 * (i + 1))
                self.store(RAX, 24 + i * 8, R10)
            self.move(R9, RAX)
            self.immediate(RCX, 0)
            self.immediate(RDX, 0)
            self.address(R8, "thread_" + fn.name)
            self.immediate(RAX, 0)
            self.store(RSP, 32, RAX)
            self.store(RSP, 40, RAX)
            self.external("CreateThread")
            self.test(RAX)
            self.panic_if(0x84, "Slai runtime: falha ao criar thread")
            self.load(R10, RBP, -8 * (count + 1))
            self.store(R10, 0, RAX)
            self.move(RAX, R10)
            self.epilogue()
            self.end()

            self.start("thread_" + fn.name, 1, max(4, count))
            self.store(RBP, -8, RCX)
            self.move(R10, RCX)
            for i in range(count):
                reg = ARGUMENTS[i] if i < 4 else RAX
                self.load(reg, R10, 24 + i * 8)
                if i >= 4:
                    self.store(RSP, 32 + (i - 4) * 8, reg)
            self.call(fn.name)
            self.load(R10, RBP, -8)
            self.store(R10, 8, RAX)
            self.call("rt_error_get")
            self.load(R10, RBP, -8)
            self.store(R10, 16, RAX)
            self.immediate(RAX, 0)
            self.epilogue()
            self.end()


IMPORTS = {
    "KERNEL32.dll": ["ExitProcess", "SetConsoleOutputCP", "TlsAlloc", "TlsGetValue", "TlsSetValue",
                     "CreateThread", "WaitForSingleObject", "CloseHandle", "Sleep",
                     "CreateFileW", "ReadFile", "WriteFile", "SetFilePointerEx", "SetEndOfFile", "FlushFileBuffers"],
    "msvcrt.dll": ["printf", "fflush", "malloc", "realloc", "strcmp", "memcmp", "free"],
}


def import_table(rva):
    data = bytearray(20 * (len(IMPORTS) + 1))
    symbols = {}
    for index, (dll, names) in enumerate(IMPORTS.items()):
        dll_offset = len(data)
        data.extend(dll.encode("ascii") + b"\0")
        hints = []
        for name in names:
            data.extend(b"\0" * (align(len(data), 2) - len(data)))
            hints.append(rva + len(data))
            data.extend(b"\0\0" + name.encode("ascii") + b"\0")
        data.extend(b"\0" * (align(len(data), 8) - len(data)))
        lookup = len(data)
        for hint in hints + [0]:
            data.extend(struct.pack("<Q", hint))
        iat = len(data)
        for name, hint in zip(names, hints):
            symbols[f"import:{name}"] = rva + len(data)
            data.extend(struct.pack("<Q", hint))
        data.extend(b"\0" * 8)
        struct.pack_into("<IIIII", data, index * 20, rva + lookup, 0, 0, rva + dll_offset, rva + iat)
    return data, symbols, 20 * (len(IMPORTS) + 1)


def build_pe(compiler):
    encoder = Encoder()
    encoder.runtime()
    for function in compiler.functions:
        encoder.function(function)
    encoder.tasks(compiler)
    from .native_ia import emit_runtime
    emit_runtime(encoder, compiler)
    # UNWIND_INFO: push rbp; mov rbp,rsp; sub rsp,imm32. No frame register:
    # RSP stays fixed in the body, and UWOP_ALLOC_LARGE describes the allocation.
    unwind_offsets = []
    for begin, end, frame in encoder.frames:
        encoder.rdata.extend(b"\0" * (align(len(encoder.rdata), 4) - len(encoder.rdata)))
        unwind_offsets.append(len(encoder.rdata))
        encoder.rdata.extend(bytes([1, 11, 3, 0, 11, 1]))
        encoder.rdata.extend(struct.pack("<H", frame // 8))
        encoder.rdata.extend(bytes([1, 0x50, 0, 0]))
    text_rva = 0x1000
    rdata_rva = align(text_rva + len(encoder.code), 0x1000)
    idata_rva = align(rdata_rva + len(encoder.rdata), 0x1000)
    imports, addresses, import_size = import_table(idata_rva)
    data_rva = align(idata_rva + len(imports), 0x1000)
    globals_data = bytearray(8 + compiler.globals * 8)
    addresses["error_tls"] = data_rva + compiler.globals * 8
    pdata_rva = align(data_rva + len(globals_data), 0x1000)
    pdata = bytearray()
    for (begin, end, _), unwind in zip(encoder.frames, unwind_offsets):
        pdata.extend(struct.pack("<III", text_rva + begin, text_rva + end, rdata_rva + unwind))
    addresses.update({name: text_rva + off for name, off in encoder.labels.items()})
    addresses.update({label: rdata_rva + off for label, off in encoder.strings.values()})
    addresses.update({label: rdata_rva + off for label, off in encoder.blobs.values()})
    addresses.update({f"global{i + 1}": data_rva + i * 8 for i in range(compiler.globals)})
    for offset, target in encoder.fixups:
        struct.pack_into("<i", encoder.code, offset, addresses[target] - (text_rva + offset + 4))
    sections = [(b".text", text_rva, encoder.code, 0x60000020),
                (b".rdata", rdata_rva, encoder.rdata, 0x40000040),
                (b".idata", idata_rva, imports, 0xC0000040),
                (b".data", data_rva, globals_data, 0xC0000040),
                (b".pdata", pdata_rva, pdata, 0x40000040)]
    header_size = align(0x80 + 4 + 20 + 240 + 40 * len(sections), 0x200)
    header = bytearray(header_size)
    header[:2] = b"MZ"
    struct.pack_into("<I", header, 0x3C, 0x80)
    header[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", header, 0x84, 0x8664, len(sections), 0, 0, 0, 240, 0x23)
    opt = 0x98
    struct.pack_into("<HBBIIIII", header, opt, 0x20B, 0, 1,
                     align(len(encoder.code), 0x200),
                     sum(align(len(s[2]), 0x200) for s in sections[1:]), 0,
                     addresses["entry"], text_rva)
    struct.pack_into("<QII", header, opt + 24, 0x140000000, 0x1000, 0x200)
    struct.pack_into("<HHHHHH", header, opt + 40, 6, 0, 0, 1, 6, 0)
    struct.pack_into("<III", header, opt + 56, align(pdata_rva + len(pdata), 0x1000), header_size, 0)
    struct.pack_into("<HH", header, opt + 68, 3, 0x100)  # console, NX compatible
    struct.pack_into("<QQQQII", header, opt + 72, 8 * 1024 * 1024, 0x1000, 0x100000, 0x1000, 0, 16)
    struct.pack_into("<II", header, opt + 112 + 8, idata_rva, import_size)
    struct.pack_into("<II", header, opt + 112 + 3 * 8, pdata_rva, len(pdata))
    output = header
    for i, (name, rva, contents, flags) in enumerate(sections):
        raw_size = align(len(contents), 0x200)
        struct.pack_into("<8sIIIIIIHHI", output, opt + 240 + i * 40,
                         name, len(contents), rva, raw_size, len(output), 0, 0, 0, 0, flags)
        output.extend(contents)
        output.extend(b"\0" * (raw_size - len(contents)))
    return bytes(output)

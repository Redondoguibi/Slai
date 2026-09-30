"""Conservative SIR optimizations, with explicit control-flow and escape analysis."""
from .semantic import Instruction, Value, signed


def values(args):
    for item in args:
        if isinstance(item, Value):
            yield item
        elif isinstance(item, tuple):
            yield from values(item)


def rewrite(args, mapping):
    return tuple(mapping.get(item.slot, item) if isinstance(item, Value)
                 else rewrite(item, mapping) if isinstance(item, tuple) else item for item in args)


def successors(code):
    labels = {ins.args[0]: i for i, ins in enumerate(code) if ins.op == "label"}
    edges = []
    for i, ins in enumerate(code):
        following = {i + 1} if i + 1 < len(code) else set()
        if ins.op == "jump":
            following = {labels[ins.args[0]]}
        elif ins.op == "branch":
            following.add(labels[ins.args[1]])
        elif ins.op == "error_check" and ins.args[0]:
            following.add(labels[ins.args[0]])
        elif ins.op == "error_jump":
            following = {labels[ins.args[0]]} if ins.args[0] else set()
        elif ins.op == "return":
            following = set()
        edges.append(following)
    return edges


def constants(fn):
    known = {}
    output = []
    for ins in fn.instructions:
        if ins.op in ("label", "jump", "branch", "error_check", "error_jump", "return"):
            if ins.op == "branch" and ins.args[0].slot in known:
                value = known[ins.args[0].slot]
                if bool(value) == ins.args[2]:
                    ins = Instruction("jump", args=(ins.args[1],))
                else:
                    known.clear()
                    continue
            known.clear()
        args = list(values(ins.args))
        if ins.out:
            folded, value = False, None
            if ins.op == "const":
                folded, value = True, ins.args[0]
            elif args and all(v.slot in known for v in args):
                inputs = [known[v.slot] for v in args]
                if ins.op == "copy":
                    folded, value = True, inputs[0]
                elif ins.op in ("neg", "not", "bool"):
                    folded = True
                    value = signed(-inputs[0]) if ins.op == "neg" else (not inputs[0] if ins.op == "not" else bool(inputs[0]))
                elif ins.op in ("binary", "compare"):
                    operator, x, y = ins.args[0], *inputs
                    if ins.op == "compare":
                        value = {"==": lambda: x == y, "!=": lambda: x != y, "<": lambda: x < y,
                                 "<=": lambda: x <= y, ">": lambda: x > y, ">=": lambda: x >= y}[operator]()
                        folded = True
                    elif operator not in ("/", "%") or (y != 0 and not (x == -(1 << 63) and y == -1)):
                        quotient = (abs(x) // abs(y)) * (-1 if (x < 0) != (y < 0) else 1) if operator in ("/", "%") else 0
                        value = signed({"+": lambda: x + y, "-": lambda: x - y, "*": lambda: x * y,
                                        "/": lambda: quotient, "%": lambda: x - quotient * y}[operator]())
                        folded = True
            known.pop(ins.out.slot, None)
            if folded:
                ins = Instruction("const", ins.out, (value,))
                # Globals can change concurrently in spawned tasks. Only
                # propagate local snapshots, never shared global storage.
                if ins.out.slot > 0:
                    known[ins.out.slot] = value
        output.append(ins)
    fn.instructions = output


def eliminate(fn):
    code = fn.instructions
    if not code:
        return
    edges = successors(code)
    reached, pending = set(), [0]
    while pending:
        index = pending.pop()
        if index not in reached:
            reached.add(index)
            pending.extend(edges[index])
    fn.instructions = code = [ins for i, ins in enumerate(code) if i in reached]
    edges = successors(code)
    live = [set() for _ in code]
    changed = True
    while changed:
        changed = False
        for i in range(len(code) - 1, -1, -1):
            ins = code[i]
            after = set().union(*(live[j] for j in edges[i]))
            uses = {v.slot for v in values(ins.args) if v.slot > 0}
            before = (after - {ins.out.slot} if ins.out else after) | uses
            if before != live[i]:
                live[i] = before
                changed = True
    pure = {"const", "copy", "neg", "not", "bool", "compare"}
    kept = []
    for i, ins in enumerate(code):
        safe = ins.op in pure or (ins.op == "binary" and ins.args[0] not in ("/", "%"))
        if safe and ins.out and ins.out.slot > 0:
            after = set().union(*(live[j] for j in edges[i]))
            if ins.out.slot not in after:
                continue
        kept.append(ins)
    fn.instructions = kept


def inline(compiler):
    allowed = {"const", "copy", "neg", "not", "bool", "binary", "compare", "return"}
    candidates = {}
    for fn in compiler.functions:
        if len(fn.instructions) > 24 or not fn.instructions or fn.instructions[-1].op != "return":
            continue
        if any(ins.op not in allowed or any(v.slot < 0 for v in values(ins.args)) or (ins.out and ins.out.slot < 0)
               for ins in fn.instructions):
            continue
        if sum(ins.op == "return" for ins in fn.instructions) == 1:
            candidates[fn.name] = fn
    for fn in compiler.functions:
        result = []
        for ins in fn.instructions:
            target = candidates.get(ins.args[0]) if ins.op == "call" else None
            if target is None or target is fn or fn.slots + target.slots > 350:
                result.append(ins)
                continue
            mapping = {}
            for parameter, argument in zip(target.parameters, ins.args[1]):
                mapping[parameter.slot] = fn.temp(parameter.type)
                result.append(Instruction("copy", mapping[parameter.slot], (argument,)))
            for original in target.instructions:
                if original.out and original.out.slot not in mapping:
                    mapping[original.out.slot] = fn.temp(original.out.type)
                args = rewrite(original.args, mapping)
                if original.op == "return":
                    result.append(Instruction("copy", ins.out, args))
                else:
                    result.append(Instruction(original.op, mapping.get(original.out.slot) if original.out else None, args))
        fn.instructions = result


def compact(fn):
    slots = {}
    for value in fn.parameters:
        slots.setdefault(value.slot, value.type)
    for ins in fn.instructions:
        for value in list(values(ins.args)) + ([ins.out] if ins.out else []):
            if value.slot > 0:
                slots.setdefault(value.slot, value.type)
    mapping = {old: Value(i + 1, type) for i, (old, type) in enumerate(slots.items())}
    fn.parameters = [mapping[p.slot] for p in fn.parameters]
    for ins in fn.instructions:
        if ins.out and ins.out.slot > 0:
            ins.out = mapping[ins.out.slot]
        ins.args = rewrite(ins.args, mapping)
    fn.slots = len(mapping)


def stack_allocate(fn):
    # Track allocation aliases through copies. Any store of a pointer, return,
    # or call conservatively escapes; field loads/stores through the pointer do not.
    allocations = {ins.out.slot: ins for ins in fn.instructions if ins.op == "alloc"}
    aliases = {slot: {slot} for slot in allocations}
    changed = True
    while changed:
        changed = False
        for ins in fn.instructions:
            if ins.op == "copy":
                source = aliases.get(ins.args[0].slot, set())
                previous = aliases.setdefault(ins.out.slot, set())
                if not source <= previous:
                    previous.update(source)
                    changed = True
    escaped = set()
    for ins in fn.instructions:
        args = list(values(ins.args))
        if ins.op == "copy":
            risky = args if ins.out.slot < 0 else []
        elif ins.op in ("load_field", "store_field"):
            risky = args[1:]  # storing a reference in another record escapes
        elif ins.op in ("alloc", "const"):
            risky = []
        else:
            risky = args
        for value in risky:
            escaped |= aliases.get(value.slot, set())
    for slot, ins in allocations.items():
        size = ins.args[0]
        if slot not in escaped and (fn.slots * 8 + size) < 3000:
            fn.slots += (size + 7) // 8
            ins.op, ins.args = "stack_alloc", (fn.slots,)


def optimize_program(compiler):
    inline(compiler)
    for fn in compiler.functions:
        # Fixed iterations expose copy/fold/dead chains without excessive passes.
        for _ in range(3):
            constants(fn)
            eliminate(fn)
        compact(fn)
        stack_allocate(fn)

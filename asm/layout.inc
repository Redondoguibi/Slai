; Shared image layout. These are addresses in generated applications, not in
; the compiler. All final PE bytes are written by the Assembly PE writer.
IMAGE_BASE = 140000000h
TEXT_BASE = IMAGE_BASE + 1000h
IMPORT_BASE = IMAGE_BASE + 400000h
DATA_BASE = IMAGE_BASE + 401000h
CONST_BASE = IMAGE_BASE + 500000h
PDATA_BASE = IMAGE_BASE + 600000h

IMalloc = IMPORT_BASE + 100h
IRealloc = IMalloc + 8
IPrintf = IMalloc + 16
IFFlush = IMalloc + 24
IStrcmp = IMalloc + 32
IFree = IMalloc + 40
IStrlen = IMalloc + 48
IWfopen = IMalloc + 56
IFseek = IMalloc + 64
IFtell = IMalloc + 72
IFread = IMalloc + 80
IFwrite = IMalloc + 88
IFclose = IMalloc + 96
IMemcmp = IMalloc + 104
IExit = IMPORT_BASE + 200h
IConsole = IExit + 8
ITlsAlloc = IExit + 16
ITlsGet = IExit + 24
ITlsSet = IExit + 32
ICreateThread = IExit + 40
IWait = IExit + 48
IClose = IExit + 56
ISleep = IExit + 64
ITempName = IExit + 72
IMove = IExit + 80
IDelete = IExit + 88

macro api address {
    mov rax,address
    call qword [rax]
}
; All internal helpers obey the Windows x64 ABI and preserve nonvolatile GPRs.
macro procx name {
name:
    push rbp
    mov rbp,rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,200
}
macro endpx {
    add rsp,200
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
}

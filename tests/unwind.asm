; Independent ABI test: ask Windows to unwind every emitted nonleaf function.
format PE64 console
entry verify
include '../tools/fasm/INCLUDE/WIN64A.INC'
include '../asm/layout.inc'
section '.text' code readable executable
procx verify
    call [GetCommandLineW]
    mov rcx,rax
    lea rdx,[argc]
    call [CommandLineToArgvW]
    cmp dword [argc],2
    jne .fail
    mov rcx,[rax+8]
    xor edx,edx
    mov r8d,1
    call [LoadLibraryExW]
    test rax,rax
    jz .fail
    mov rbx,rax
    mov eax,[rbx+3Ch]
    lea rax,[rbx+rax]
    mov esi,[rax+24+112+24]
    add rsi,rbx
    mov edi,[rax+24+112+28]
    xor edx,edx
    mov eax,edi
    mov ecx,12
    div ecx
    mov edi,eax
    test edi,edi
    jz .fail
    xor r12d,r12d
.function:
    cmp r12,rdi
    jae .success
    mov r13d,[rsi+8]
    add r13,rbx
    movzx r15d,byte [r13+2]
    xor r14d,r14d
    lea r10,[r13+4]
.size:
    test r15d,r15d
    jz .context
    movzx eax,byte [r10+1]
    and eax,15
    test eax,eax
    jz .pushsize
    cmp eax,1
    jne .fail
    movzx eax,word [r10+2]
    lea r14,[r14+rax*8]
    add r10,4
    sub r15d,2
    jmp .size
.pushsize:
    add r14,8
    add r10,2
    dec r15d
    jmp .size
.context:
    lea rcx,[context]
    xor edx,edx
    mov r8d,1232
    call [memset]
    mov dword [context+48],100003h
    lea r11,[fake_stack+4104]
    mov rax,1122334455667788h
    mov [r11],rax
    sub r11,r14
    mov qword [context+152],r11
    movzx r15d,byte [r13+2]
    lea r10,[r13+4]
.stack:
    test r15d,r15d
    jz .unwind
    movzx eax,byte [r10+1]
    mov edx,eax
    and eax,15
    test eax,eax
    jz .saved
    movzx eax,word [r10+2]
    lea r11,[r11+rax*8]
    add r10,4
    sub r15d,2
    jmp .stack
.saved:
    shr edx,4
    add edx,100h
    mov [r11],rdx
    add r11,8
    add r10,2
    dec r15d
    jmp .stack
.unwind:
    mov r8d,[rsi]
    add r8,rbx
    movzx eax,byte [r13+1]
    add r8,rax
    mov qword [context+248],r8
    xor ecx,ecx
    mov rdx,rbx
    mov r9,rsi
    lea rax,[context]
    mov [rsp+32],rax
    lea rax,[handler]
    mov [rsp+40],rax
    lea rax,[unw_frame]
    mov [rsp+48],rax
    mov qword [rsp+56],0
    call [RtlVirtualUnwind]
    mov rax,1122334455667788h
    cmp qword [context+248],rax
    jne .fail
    lea rax,[fake_stack+4112]
    cmp qword [context+152],rax
    jne .fail
    cmp qword [context+160],105h
    jne .fail
    add rsi,12
    inc r12d
    jmp .function
.success:
    lea rcx,[fmt_ok]
    mov rdx,r12
    call [printf]
    xor ecx,ecx
    call [ExitProcess]
.fail:
    lea rcx,[fmt_fail]
    mov rdx,r12
    call [printf]
    mov ecx,1
    call [ExitProcess]
endpx
section '.data' data readable writeable
align 16
context rb 1232
fake_stack rb 8192
handler dq 0
unw_frame dq 0
argc dd 0
fmt_ok db '%I64d functions unwound by Windows',10,0
fmt_fail db 'Unwind validation failed at function %I64d',10,0
section '.idata' import data readable writeable
library kernel,'KERNEL32.DLL',ntdll,'ntdll.dll',shell,'SHELL32.DLL',crt,'msvcrt.dll'
import kernel,GetCommandLineW,'GetCommandLineW',LoadLibraryExW,'LoadLibraryExW',ExitProcess,'ExitProcess'
import ntdll,RtlVirtualUnwind,'RtlVirtualUnwind'
import shell,CommandLineToArgvW,'CommandLineToArgvW'
import crt,printf,'printf',memset,'memset'


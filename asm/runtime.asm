format binary
use64
include 'layout.inc'
org TEXT_BASE
; Export table consumed by the compiler at assembly time.
dq rt_entry,rt_alloc,rt_log_int,rt_log_str,rt_log_bool,rt_compare
dq rt_list_new,rt_list_push,rt_list_get,rt_list_set
dq rt_error_get,rt_error_set,rt_error_take,rt_panic,rt_div,rt_mod
dq rt_spawn,rt_await,rt_sleep,rt_assert
dq rt_apply
dq runtime_pdata,runtime_pdata_end-runtime_pdata

procx rt_entry
    api ITlsAlloc
    mov r10,DATA_BASE+8
    mov [r10],rax
    mov ecx,65001
    api IConsole
    mov rax,DATA_BASE
    call qword [rax]
    call rt_error_get
    test rax,rax
    jz .ok
    mov rcx,rax
    call rt_panic
.ok:
    xor ecx,ecx
    api IFFlush
    xor ecx,ecx
    api IExit
    ud2
endpx

procx rt_panic
    mov rdx,rcx
    lea rcx,[fmt_str]
    api IPrintf
    xor ecx,ecx
    api IFFlush
    mov ecx,1
    api IExit
    ud2
endpx

procx rt_alloc
    api IMalloc
    test rax,rax
    jnz .ok
    lea rcx,[msg_memory]
    call rt_panic
.ok:
endpx

procx rt_log_int
    mov rdx,rcx
    lea rcx,[fmt_int]
    api IPrintf
endpx
procx rt_log_str
    mov rdx,rcx
    lea rcx,[fmt_str]
    api IPrintf
endpx
procx rt_log_bool
    test rcx,rcx
    lea rcx,[str_false]
    lea rax,[str_true]
    cmovnz rcx,rax
    call rt_log_str
endpx
procx rt_compare
    api IStrcmp
    movsxd rax,eax
endpx

procx rt_error_get
    mov rcx,DATA_BASE+8
    mov ecx,[rcx]
    api ITlsGet
endpx
procx rt_error_set
    mov rdx,rcx
    mov rcx,DATA_BASE+8
    mov ecx,[rcx]
    api ITlsSet
endpx
procx rt_error_take
    call rt_error_get
    mov rbx,rax
    xor ecx,ecx
    call rt_error_set
    mov rax,rbx
endpx

procx rt_div
    mov rax,rcx
    mov r10,rdx
    test r10,r10
    jz .zero
    mov r11,8000000000000000h
    cmp rax,r11
    jne .divide
    cmp r10,-1
    je .overflow
.divide:
    cqo
    idiv r10
endpx
.zero:
    lea rcx,[msg_zero]
    call rt_panic
.overflow:
    lea rcx,[msg_overflow]
    call rt_panic
procx rt_mod
    call rt_div
    mov rax,rdx
endpx

procx rt_list_new
    mov ecx,24
    call rt_alloc
    mov rbx,rax
    mov qword [rbx],0
    mov qword [rbx+8],4
    mov ecx,32
    call rt_alloc
    mov [rbx+16],rax
    mov rax,rbx
endpx
procx rt_list_push
    mov rbx,rcx
    mov rsi,rdx
    mov rax,[rbx]
    cmp rax,[rbx+8]
    jb .room
    mov rdi,[rbx+8]
    shl rdi,1
    mov rdx,rdi
    shl rdx,3
    jc .full
    mov rcx,[rbx+16]
    api IRealloc
    test rax,rax
    jz .full
    mov [rbx+16],rax
    mov [rbx+8],rdi
.room:
    mov rax,[rbx]
    mov r10,[rbx+16]
    mov [r10+rax*8],rsi
    inc qword [rbx]
    xor eax,eax
endpx
.full:
    lea rcx,[msg_memory]
    call rt_panic
procx rt_list_get
    test rdx,rdx
    js .negative
    cmp rdx,[rcx]
    jae .outside
    mov rax,[rcx+16]
    mov rax,[rax+rdx*8]
endpx
.negative:
    lea rcx,[msg_negative]
    call rt_panic
.outside:
    lea rcx,[msg_index]
    call rt_panic
procx rt_list_set
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    call rt_list_get
    mov rax,[rbx+16]
    mov [rax+rsi*8],rdi
endpx

; spawn(fn_address, argument_array, count). Each task owns a copied argument
; array and an independent Windows TLS error value. Result survives await.
procx rt_spawn
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    lea rcx,[rdi*8+40]
    call rt_alloc
    mov r12,rax
    mov [r12+24],rbx
    mov [r12+32],rdi
    xor eax,eax
.copy:
    cmp rax,rdi
    jae .start
    mov r10,[rsi+rax*8]
    mov [r12+rax*8+40],r10
    inc rax
    jmp .copy
.start:
    xor ecx,ecx
    xor edx,edx
    lea r8,[rt_worker]
    mov r9,r12
    mov qword [rsp+32],0
    mov qword [rsp+40],0
    api ICreateThread
    test rax,rax
    jz .failed
    mov [r12],rax
    mov rax,r12
endpx
.failed:
    lea rcx,[msg_thread]
    call rt_panic
procx rt_worker
    mov rbx,rcx
    ; Slai functions use the Windows ABI, up to 16 arguments.
    mov rax,[rbx+32]
    cmp rax,16
    ja .failed
    mov rsi,4
.stack:
    cmp rsi,rax
    jae .regs
    mov r10,[rbx+rsi*8+40]
    mov [rsp+rsi*8],r10
    inc rsi
    jmp .stack
.regs:
    xor ecx,ecx
    xor edx,edx
    xor r8d,r8d
    xor r9d,r9d
    test rax,rax
    jz .call
    mov rcx,[rbx+40]
    cmp rax,1
    je .call
    mov rdx,[rbx+48]
    cmp rax,2
    je .call
    mov r8,[rbx+56]
    cmp rax,3
    je .call
    mov r9,[rbx+64]
.call:
    call qword [rbx+24]
    mov [rbx+8],rax
    call rt_error_get
    mov [rbx+16],rax
    xor eax,eax
endpx
.failed:
    lea rcx,[msg_thread]
    call rt_panic
procx rt_await
    mov rbx,rcx
    mov rcx,[rbx]
    test rcx,rcx
    jz .ready
    mov edx,-1
    api IWait
    test eax,eax
    jnz .failed
    mov rcx,[rbx]
    api IClose
    mov qword [rbx],0
.ready:
    mov rcx,[rbx+16]
    call rt_error_set
    mov rax,[rbx+8]
endpx
.failed:
    lea rcx,[msg_thread]
    call rt_panic
procx rt_sleep
    api ISleep
endpx
procx rt_assert
    test rcx,rcx
    jnz .ok
    mov rcx,rdx
    call rt_error_set
.ok:
    xor eax,eax
endpx

HOST_APPLY=0
APPLY_ALLOC equ rt_alloc
APPLY_ERROR equ rt_error_set
include 'runtime_apply.inc'
fmt_int db '%I64d',10,0
fmt_str db '%s',10,0
str_true db 'true',0
str_false db 'false',0
msg_memory db 'Slai runtime: memoria insuficiente',0
msg_zero db 'Slai runtime: divisao por zero',0
msg_overflow db 'Slai runtime: overflow na divisao int64',0
msg_negative db 'Slai runtime: indice de lista negativo',0
msg_index db 'Slai runtime: indice fora da lista',0
msg_thread db 'Slai runtime: falha de task',0

align 4
runtime_unwind:
    ; procx: push rbp, mov rbp,rsp, push 7 nonvolatiles, sub rsp,200.
    db 1,22,10,0
    db 22,1
    dw 25
    db 15,0F0h,13,0E0h,11,0D0h,9,0C0h,7,070h,6,060h,5,030h,1,050h
runtime_pdata:
macro runtime_function first,last {
    dd first-IMAGE_BASE,last-IMAGE_BASE,runtime_unwind-IMAGE_BASE
}
runtime_function rt_entry,rt_panic
runtime_function rt_panic,rt_alloc
runtime_function rt_alloc,rt_log_int
runtime_function rt_log_int,rt_log_str
runtime_function rt_log_str,rt_log_bool
runtime_function rt_log_bool,rt_compare
runtime_function rt_compare,rt_error_get
runtime_function rt_error_get,rt_error_set
runtime_function rt_error_set,rt_error_take
runtime_function rt_error_take,rt_div
runtime_function rt_div,rt_mod
runtime_function rt_mod,rt_list_new
runtime_function rt_list_new,rt_list_push
runtime_function rt_list_push,rt_list_get
runtime_function rt_list_get,rt_list_set
runtime_function rt_list_set,rt_spawn
runtime_function rt_spawn,rt_worker
runtime_function rt_worker,rt_await
runtime_function rt_await,rt_sleep
runtime_function rt_sleep,rt_assert
runtime_function rt_assert,rt_match_file
runtime_function rt_match_file,rt_stage_file
runtime_function rt_stage_file,rt_apply
runtime_function rt_apply,apply_read
runtime_pdata_end:

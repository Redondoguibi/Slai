procx alloc
    mov rdx,rcx
    mov ecx,1
    call [calloc]
    test rax,rax
    jnz .ok
    lea rcx,[err_memory]
    call fatal
.ok:
endpx
procx host_apply_error
    mov [g_applyerror],rcx
endpx
procx duplicate
    mov rsi,rcx
    mov rbx,rdx
    lea rcx,[rbx+1]
    call alloc
    mov rdi,rax
    mov rcx,rax
    mov rdx,rsi
    mov r8,rbx
    call [memcpy]
    mov rax,rdi
endpx
procx equal
    call [strcmp]
    test eax,eax
    sete al
    movzx eax,al
endpx
procx utf16
    mov rbx,rcx
    call [strlen]
    lea rcx,[rax*2+2]
    mov rsi,rcx
    call alloc
    mov rdi,rax
    mov ecx,65001
    xor edx,edx
    mov r8,rbx
    mov r9d,-1
    mov [rsp+32],rdi
    shr rsi,1
    mov [rsp+40],rsi
    call [MultiByteToWideChar]
    mov rax,rdi
endpx
procx utf8
    mov rbx,rcx
    mov rcx,65536
    call alloc
    mov rdi,rax
    mov ecx,65001
    xor edx,edx
    mov r8,rbx
    mov r9d,-1
    mov [rsp+32],rdi
    mov qword [rsp+40],65536
    mov qword [rsp+48],0
    mov qword [rsp+56],0
    call [WideCharToMultiByte]
    test eax,eax
    jz .bad
    mov rax,rdi
endpx
.bad:
    lea rcx,[err_path]
    call fatal
procx fullpath
    call utf16
    mov rbx,rax
    mov ecx,65536
    call alloc
    mov rsi,rax
    mov rcx,rbx
    mov edx,32768
    mov r8,rsi
    xor r9d,r9d
    call [GetFullPathNameW]
    test eax,eax
    jz .bad
    cmp eax,32768
    jae .bad
    mov rcx,rsi
    call utf8
endpx
.bad:
    lea rcx,[err_path]
    call fatal
procx readfile
    call utf16
    mov rcx,rax
    lea rdx,[mode_read]
    call [wfopen]
    test rax,rax
    jz .bad
    mov rbx,rax
    mov rcx,rax
    xor edx,edx
    mov r8d,2
    call [fseek]
    mov rcx,rbx
    call [ftell]
    movsxd rsi,eax
    test rsi,rsi
    js .bad
    cmp rsi,1048576
    ja .large
    mov rcx,rbx
    xor edx,edx
    xor r8d,r8d
    call [fseek]
    lea rcx,[rsi+2]
    call alloc
    mov rdi,rax
    mov rcx,rax
    mov edx,1
    mov r8,rsi
    mov r9,rbx
    call [fread]
    cmp rax,rsi
    jne .bad
    mov rcx,rbx
    call [fclose]
    xor ecx,ecx
.validate:
    cmp rcx,rsi
    jae .utf8
    cmp byte [rdi+rcx],0
    je .invalid
    inc rcx
    jmp .validate
.utf8:
    test rsi,rsi
    jz .valid
    mov ecx,65001
    mov edx,8
    mov r8,rdi
    mov r9,rsi
    mov qword [rsp+32],0
    mov qword [rsp+40],0
    call [MultiByteToWideChar]
    test eax,eax
    jz .invalid
.valid:
    mov rax,rdi
    mov rdx,rsi
endpx
.large:
    lea rcx,[err_limit]
    call fatal
.invalid:
    lea rcx,[err_lex]
    call fatal
.bad:
    lea rcx,[err_read]
    call fatal
procx fatal
    mov rbx,rcx
    lea rax,[err_memory]
    cmp rcx,rax
    je .nomemory
    mov r12,[g_file]
    test r12,r12
    jnz .path
    lea r12,[empty]
.path:
    mov rsi,[g_line]
    mov rdi,[g_col]
    mov rax,[g_tok]
    test rax,rax
    jz .format
    mov rsi,[rax+24]
    mov rdi,[rax+32]
    mov r12,[rax+40]
.format:
    call text_begin
    cmp qword [g_json],0
    jne .json
    mov rcx,r12
    call text_append
    lea rcx,[colon_text]
    call text_append
    mov rcx,rsi
    call text_number
    lea rcx,[colon_text]
    call text_append
    mov rcx,rdi
    call text_number
    lea rcx,[error_prefix]
    call text_append
    mov rcx,rbx
    call text_append
    lea rcx,[newline_text]
    call text_append
    jmp .buffer
.json:
    lea rcx,[json_diagnostic]
    call text_append
    lea rcx,[json_file]
    call text_append
    mov rcx,r12
    call json_string
    lea rcx,[json_line]
    call text_append
    mov rcx,rsi
    call text_number
    lea rcx,[json_column]
    call text_append
    mov rcx,rdi
    call text_number
    lea rcx,[json_message]
    call text_append
    mov rcx,rbx
    call json_string
    lea rcx,[json_diagnostic_end]
    call text_append
.buffer:
    mov rsi,[g_text]
    mov rdi,[g_textlen]
    jmp .write
.nomemory:
    mov rsi,rcx
    call [strlen]
    mov rdi,rax
.write:
    mov ecx,-12
    call [GetStdHandle]
    mov rcx,rax
    mov rdx,rsi
    mov r8,rdi
    lea r9,[rbp-64]
    mov qword [rsp+32],0
    call [WriteFile]
    mov ecx,1
    call [ExitProcess]
    ud2
endpx
procx node_new
    mov rbx,rcx
    mov rsi,rdx
    mov ecx,NSIZE
    call alloc
    mov [rax+NK],rbx
    mov [rax+NT],rsi
endpx
procx token_is
    mov rdx,rcx
    mov rax,[g_tok]
    cmp qword [rax],TK_NAME
    jne .no
    mov rcx,[rax+8]
    call equal
endpx
.no:
    xor eax,eax
    endpx
procx accept
    mov rax,[g_tok]
    cmp [rax],rcx
    jne .no
    add qword [g_tok],48
endpx
.no:
    xor eax,eax
    endpx
procx expect
    call accept
    test rax,rax
    jnz .ok
    lea rcx,[err_expected]
    call fatal
.ok:
endpx
procx advance
    mov rax,[g_tok]
    add qword [g_tok],48
endpx



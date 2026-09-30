; Bounded UTF-8 JSON/text builder shared by diagnostics and semantic tooling.
procx text_begin
    mov ecx,1048576
    call alloc
    mov [g_text],rax
    mov qword [g_textlen],0
endpx
procx text_bytes
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,[g_textlen]
    lea rax,[rdi+rsi+1]
    cmp rax,1048576
    ja .bad
    mov rcx,[g_text]
    add rcx,rdi
    mov rdx,rbx
    mov r8,rsi
    call [memcpy]
    add rdi,rsi
    mov [g_textlen],rdi
    mov rax,[g_text]
    mov byte [rax+rdi],0
endpx
.bad:
    lea rcx,[err_limit]
    call fatal
procx text_append
    mov rbx,rcx
    call [strlen]
    mov rdx,rax
    mov rcx,rbx
    call text_bytes
endpx
procx text_number
    mov r8,rcx
    lea rcx,[rbp-128]
    lea rdx,[fmt_number]
    call [sprintf]
    lea rcx,[rbp-128]
    call text_append
endpx
procx json_string
    mov rsi,rcx
    lea rcx,[json_quote]
    call text_append
.loop:
    movzx eax,byte [rsi]
    test eax,eax
    jz .done
    cmp al,34
    je .escape
    cmp al,92
    je .escape
    cmp al,32
    jb .control
    mov rcx,rsi
    mov edx,1
    call text_bytes
    jmp .next
.escape:
    mov byte [rbp-64],92
    mov [rbp-63],al
    lea rcx,[rbp-64]
    mov edx,2
    call text_bytes
    jmp .next
.control:
    mov r8,rax
    lea rcx,[rbp-80]
    lea rdx,[fmt_escape]
    call [sprintf]
    lea rcx,[rbp-80]
    call text_append
.next:
    inc rsi
    jmp .loop
.done:
    lea rcx,[json_quote]
    call text_append
endpx
procx json_location
    mov rbx,rcx
    lea rcx,[json_file]
    call text_append
    mov rcx,[rbx+40]
    call json_string
    lea rcx,[json_line]
    call text_append
    mov rcx,[rbx+24]
    call text_number
    lea rcx,[json_column]
    call text_append
    mov rcx,[rbx+32]
    call text_number
    lea rcx,[json_close]
    call text_append
endpx
; A source token points to its UTF-8 byte range, excluding BOM and comments.
procx token_offset
    mov rbx,rcx
    mov rsi,[g_modules]
.module:
    test rsi,rsi
    jz .bad
    mov rcx,[rsi]
    mov rdx,[rbx+40]
    call equal
    test eax,eax
    jnz .found
    mov rsi,[rsi+32]
    jmp .module
.found:
    mov rdi,[rsi+8]
    mov r12,rdi
    cmp byte [rdi],0EFh
    jne .lines
    cmp word [rdi+1],0BFBBh
    jne .lines
    add rdi,3
.lines:
    mov rcx,[rbx+24]
    dec rcx
.line:
    test rcx,rcx
    jz .column
    cmp byte [rdi],0
    je .bad
    cmp byte [rdi],10
    jne .next
    dec rcx
.next:
    inc rdi
    jmp .line
.column:
    add rdi,[rbx+32]
    dec rdi
    mov rax,rdi
    sub rax,r12
    mov rdx,rsi
endpx
.bad:
    lea rcx,[err_internal]
    call fatal
procx type_name
    lea rax,[kw_int]
    cmp rcx,T_INT
    je .done
    lea rax,[kw_bool]
    cmp rcx,T_BOOL
    je .done
    lea rax,[kw_string]
    cmp rcx,T_STRING
    je .done
    lea rax,[kw_void]
    cmp rcx,T_VOID
    je .done
    lea rax,[kind_change]
    cmp rcx,T_CHANGE
    je .done
    lea rax,[kind_object]
    cmp rcx,4096
    jb .done
    cmp qword [rcx],7
    je .class
    cmp qword [rcx],5
    je .list
    cmp qword [rcx],8
    je .task
    jmp .done
.class:
    mov rax,[rcx+8]
    mov rax,[rax+NV]
    jmp .done
.list:
    lea rax,[kind_list]
    jmp .done
.task:
    lea rax,[kind_task]
.done:
endpx
procx json_sir
    call text_begin
    lea rcx,[json_array]
    call text_append
    mov rsi,[g_functions]
    xor edi,edi
.function:
    test rsi,rsi
    jz .done
    test edi,edi
    jz .name
    lea rcx,[comma]
    call text_append
.name:
    lea rcx,[json_name]
    call text_append
    mov rax,[rsi+F_AST]
    lea rcx,[main_name]
    test rax,rax
    jz .main
    mov rcx,[rax+NV]
.main:
    call json_string
    lea rcx,[json_slots]
    call text_append
    mov rcx,[rsi+F_SLOTS]
    call text_number
    lea rcx,[json_instructions]
    call text_append
    mov rbx,[rsi+F_HEAD]
    xor r12d,r12d
.instruction:
    test rbx,rbx
    jz .endfunction
    test r12d,r12d
    jz .op
    lea rcx,[comma]
    call text_append
.op:
    lea rcx,[json_op]
    call text_append
    mov rax,[rbx]
    lea rcx,[opcode_names]
    mov rcx,[rcx+rax*8-8]
    call json_string
    mov rax,[rbx+8]
    test rax,rax
    jz .endins
    mov r13,rax
    lea rcx,[json_out]
    call text_append
    mov rcx,[r13+8]
    call text_number
    lea rcx,[json_type]
    call text_append
    mov rcx,[r13]
    call type_name
    mov rcx,rax
    call json_string
    cmp qword [rbx],SI_CONST
    jne .endins
    cmp qword [r13],T_STRING
    ja .endins
    lea rcx,[json_value]
    call text_append
    mov rcx,[rbx+16]
    cmp qword [r13],T_STRING
    jne .number
    call json_string
    jmp .endins
.number:
    call text_number
.endins:
    lea rcx,[json_close]
    call text_append
    inc r12d
    mov rbx,[rbx+48]
    jmp .instruction
.endfunction:
    lea rcx,[inspect_footer]
    call text_append
    inc edi
    mov rsi,[rsi+F_NEXT]
    jmp .function
.done:
    lea rcx,[json_endarray]
    call text_append
    mov rax,[g_text]
endpx

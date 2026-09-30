T_CHANGE=5
procx compile_ia
    ; AST and operation string. Queries resolve after all type specializations.
    mov rbx,rcx
    mov rsi,rdx
    mov rcx,rsi
    lea rdx,[kw_preview]
    call equal
    test eax,eax
    jnz .preview
    mov rcx,rsi
    lea rdx,[kw_apply]
    call equal
    test eax,eax
    jnz .apply
    mov edi,T_STRING
    mov rcx,rsi
    lea rdx,[kw_rename]
    call equal
    test eax,eax
    jnz .rename
    mov rcx,rsi
    lea rdx,[kw_edit]
    call equal
    test eax,eax
    jnz .edit
    mov rcx,rsi
    lea rdx,[kw_errors]
    call equal
    test eax,eax
    jnz .errors
    cmp qword [rbx+NC],1
    jne .arity
    jmp .request
.errors:
    cmp qword [rbx+NC],0
    jne .arity
    jmp .request
.rename:
    cmp qword [rbx+NC],2
    jne .arity
    mov edi,T_CHANGE
    jmp .request
.edit:
    cmp qword [rbx+NC],1
    jne .arity
    cmp qword [rbx+NX],0
    je .arity
    mov edi,T_CHANGE
.request:
    mov rcx,rdi
    lea rdx,[empty]
    call constant
    mov r12,rax
    mov rax,[g_ctx]
    mov r13,[rax+F_TAIL]
    mov ecx,32
    call alloc
    mov [rax],rbx
    mov [rax+8],rsi
    mov [rax+16],r13
    mov rcx,[g_iarequests]
    mov [rax+24],rcx
    mov [g_iarequests],rax
    mov rax,r12
    jmp .done
.preview:
    xor edi,edi
    jmp .change
.apply:
    mov edi,1
.change:
    cmp qword [rbx+NC],1
    jne .arity
    mov rcx,[rbx+NB]
    call compile_expr
    cmp qword [rax],T_CHANGE
    jne .bad
    mov r12,rax
    test edi,edi
    jnz .applycall
    mov ecx,T_STRING
    call value_new
    mov rdi,rax
    mov ecx,SI_FIELD_GET
    mov rdx,rax
    mov r8,r12
    xor r9d,r9d
    call sir
    mov rax,rdi
    jmp .done
.applycall:
    mov rcx,RT_APPLY
    mov edx,T_BOOL
    mov r8,r12
    xor r9d,r9d
    call native_call
    mov rdi,rax
    call error_check
    mov rax,rdi
.done:
endpx
.arity:
    lea rcx,[err_arity]
    call fatal
.bad:
    lea rcx,[err_type]
    call fatal
procx resolve_ia
    mov rbx,[g_iarequests]
.loop:
    test rbx,rbx
    jz .done
    mov rsi,[rbx]
    mov rax,[rsi+NT]
    mov [g_tok],rax
    mov rcx,[rsi+NB]
    xor edi,edi
    test rcx,rcx
    jz .operation
    call symbolic_name
    mov rdi,rax
.operation:
    mov rcx,[rbx+8]
    lea rdx,[kw_rename]
    call equal
    test eax,eax
    jnz .rename
    mov rcx,[rbx+8]
    lea rdx,[kw_edit]
    call equal
    test eax,eax
    jnz .edit
    mov rcx,[rbx+8]
    mov rdx,rdi
    call query_index
    jmp .save
.rename:
    mov rax,[rsi+NB]
    mov rcx,[rax+NN]
    call symbolic_name
    mov rdx,rax
    mov rcx,rdi
    call plan_rename
    jmp .save
.edit:
    mov rcx,rdi
    mov rdx,[rsi+NX]
    mov r8,[rsi+NZ]
    call plan_edit
.save:
    mov rcx,[rbx+16]
    mov [rcx+16],rax
    mov rbx,[rbx+24]
    jmp .loop
.done:
endpx

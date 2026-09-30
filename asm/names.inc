; Lexical scopes are validated even for uninstantiated generic functions.
procx name_find
    mov rbx,rcx
    mov rsi,[g_env]
.loop:
    test rsi,rsi
    jz .done
    mov rcx,[rsi]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .done
    mov rsi,[rsi+16]
    jmp .loop
.done:
    mov rax,rsi
endpx
procx name_bind
    mov rbx,rcx
    mov rsi,rdx
    mov ecx,24
    call alloc
    mov [rax],rbx
    mov [rax+8],rsi
    mov rcx,[g_env]
    mov [rax+16],rcx
    mov [g_env],rax
endpx
procx ia_operation
    mov rax,[rcx+NA]
    cmp qword [rax+NK],N_MEMBER
    jne .no
    mov rbx,rax
    mov rsi,[rax+NA]
    cmp qword [rsi+NK],N_MEMBER
    jne .no
    mov rcx,[rsi+NV]
    lea rdx,[kw_ia]
    call equal
    test eax,eax
    jz .no
    mov rsi,[rsi+NA]
    cmp qword [rsi+NK],N_NAME
    jne .no
    mov rcx,[rsi+NV]
    lea rdx,[kw_sys]
    call equal
    test eax,eax
    jz .no
    mov rax,[rbx+NV]
    jmp .done
.no:
    xor eax,eax
.done:
endpx
procx validate_expr
    mov rbx,rcx
    test rbx,rbx
    jz .done
    mov rax,[rbx+NT]
    mov [g_tok],rax
    mov rax,[rax+40]
    mov [g_file],rax
    mov rax,[rbx+NK]
    cmp rax,N_NAME
    je .name
    cmp rax,N_CALL
    je .call
    cmp rax,N_MEMBER
    je .one
    cmp rax,N_LIST
    je .chain
    cmp rax,N_OBJECT
    je .chain
    cmp rax,N_BINARY
    je .two
    cmp rax,N_INDEX
    je .two
    cmp rax,N_UNARY
    je .one
    cmp rax,N_AWAIT
    je .one
    cmp rax,N_PROPAGATE
    je .one
    cmp rax,N_FIELD
    je .one
    jmp .done
.name:
    mov rcx,[rbx+NV]
    call name_find
    test rax,rax
    jz .undefined
    mov rax,[rax+8]
    mov [rbx+80],rax
    jmp .done
.call:
    mov rsi,[rbx+NA]
    mov rcx,rbx
    call ia_operation
    test rax,rax
    jnz .ia
    cmp qword [rsi+NK],N_NAME
    jne .callee
    mov rcx,[rsi+NV]
    call find_definition
    test rax,rax
    jz .callee
    mov rcx,[rax+NC]
    cmp qword [rax+NK],N_CLASS
    jne .arity
    mov rdi,[rax+NA]
    xor r12d,r12d
.ctor:
    test rdi,rdi
    jz .ctorarity
    mov rcx,[rdi+NV]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jnz .ctorfound
    mov rdi,[rdi+NN]
    jmp .ctor
.ctorfound:
    mov r12,[rdi+NC]
.ctorarity:
    mov rcx,r12
.arity:
    cmp rcx,[rbx+NC]
    jne .badarity
.callee:
    mov rcx,rsi
    call validate_expr
.args:
    mov rsi,[rbx+NB]
    jmp .loop
.ia:
    mov rcx,rax
    lea rdx,[kw_preview]
    call equal
    test eax,eax
    jnz .args
    mov rcx,[rsi+NV]
    lea rdx,[kw_apply]
    call equal
    test eax,eax
    jnz .args
    jmp .done
.two:
    mov rcx,[rbx+NB]
    call validate_expr
.one:
    mov rcx,[rbx+NA]
    call validate_expr
    jmp .done
.chain:
    mov rsi,[rbx+NA]
.loop:
    test rsi,rsi
    jz .done
    mov rcx,rsi
    call validate_expr
    mov rsi,[rsi+NN]
    jmp .loop
.done:
endpx
.undefined:
    lea rcx,[err_name]
    call fatal
.badarity:
    lea rcx,[err_arity]
    call fatal
procx validate_sequence
    mov rbx,rcx
.loop:
    test rbx,rbx
    jz .done
    mov rcx,rbx
    call validate_statement
    mov rbx,[rbx+NN]
    jmp .loop
.done:
endpx
procx validate_statement
    mov rbx,rcx
    mov rax,[rbx+NT]
    mov [g_tok],rax
    mov rax,[rax+40]
    mov [g_file],rax
    mov rax,[rbx+NK]
    cmp rax,N_ASSIGN
    je .assign
    cmp rax,N_FOR
    je .for
    cmp rax,N_IF
    je .if
    cmp rax,N_CATCH
    je .catch
    cmp rax,N_FUNC
    je .definition
    cmp rax,N_CLASS
    je .definition
    cmp rax,N_IMPORT
    je .import
    cmp rax,N_BREAK
    je .loopcontrol
    cmp rax,N_CONTINUE
    je .loopcontrol
    cmp rax,N_RETURN
    je .return
.expression:
    mov rcx,[rbx+NA]
    call validate_expr
    jmp .done
.return:
    cmp qword [g_infunction],0
    je .bad
    jmp .expression
.loopcontrol:
    cmp qword [g_inloop],0
    je .bad
    jmp .done
.definition:
    cmp qword [g_infunction],0
    jne .bad
    cmp qword [g_indepth],0
    jne .bad
    jmp .done
.import:
    mov rcx,[rbx+NA]
    call validate_sequence
    jmp .done
.assign:
    mov rcx,[rbx+NB]
    call validate_expr
    mov rsi,[rbx+NA]
    cmp qword [rsi+NK],N_NAME
    jne .left
    mov rcx,[rsi+NV]
    call name_find
    test rax,rax
    jnz .bound
    cmp qword [rbx+NV],'='
    jne .undefined
    mov rcx,[rsi+NV]
    mov rdx,rsi
    call name_bind
.bound:
    mov rax,[rax+8]
    mov [rsi+80],rax
    jmp .done
.left:
    mov rcx,rsi
    call validate_expr
    jmp .done
.for:
    mov rcx,[rbx+NA]
    call validate_expr
    mov r12,[g_env]
    mov rcx,[rbx+NV]
    mov rdx,rbx
    call name_bind
    mov rcx,[rbx+NB]
    call validate_expr
    inc qword [g_inloop]
    inc qword [g_indepth]
    mov rcx,[rbx+NC]
    call validate_sequence
    mov rcx,[rbx+NX]
    test rcx,rcx
    jz .forend
    call validate_statement
.forend:
    dec qword [g_inloop]
    dec qword [g_indepth]
    mov [g_env],r12
    jmp .done
.if:
    mov r12,[g_env]
    mov rsi,[rbx+NA]
    inc qword [g_indepth]
.branch:
    test rsi,rsi
    jz .ifend
    mov rcx,[rsi+NA]
    call validate_expr
    mov rcx,[rsi+NB]
    call validate_sequence
    mov [g_env],r12
    mov rsi,[rsi+NN]
    jmp .branch
.ifend:
    dec qword [g_indepth]
    jmp .done
.catch:
    mov r12,[g_env]
    mov [rbp-64],r12
    mov rcx,[rbx+NA]
    call validate_statement
    mov r13,[g_env]
    mov [g_env],r12
    mov rcx,[rbx+NV]
    mov rdx,rbx
    call name_bind
    inc qword [g_indepth]
    mov rcx,[rbx+NB]
    call validate_sequence
    dec qword [g_indepth]
    mov r14,[g_env]
    mov rsi,r13
.join:
    cmp rsi,[rbp-64]
    je .joined
    test rsi,rsi
    jz .joined
    mov [g_env],r14
    mov rcx,[rsi]
    call name_find
    test rax,rax
    jz .joinnext
    mov [g_env],r12
    mov rcx,[rsi]
    mov rdx,[rsi+8]
    call name_bind
    mov r12,rax
.joinnext:
    mov rsi,[rsi+16]
    jmp .join
.joined:
    mov [g_env],r12
.done:
endpx
.bad:
    lea rcx,[err_scope]
    call fatal
.undefined:
    lea rcx,[err_name]
    call fatal
procx validate_function
    mov rbx,rcx
    mov r12,[g_env]
    mov rax,[rbx+NT]
    mov [g_tok],rax
    cmp qword [rbx+NX],0
    je .params
    lea rcx,[kw_this]
    mov rdx,[rbx+NX]
    call name_bind
.params:
    mov rsi,[rbx+NB]
.param:
    test rsi,rsi
    jz .body
    mov rax,[rsi+NT]
    mov [g_tok],rax
    mov rcx,[rsi+NV]
    call find_definition
    test rax,rax
    jnz .duplicate
    mov rdi,[rbx+NB]
.duplicates:
    cmp rdi,rsi
    je .annotation
    mov rcx,[rdi+NV]
    mov rdx,[rsi+NV]
    call equal
    test eax,eax
    jnz .duplicate
    mov rdi,[rdi+NN]
    jmp .duplicates
.annotation:
    mov rcx,[rsi+NX]
    test rcx,rcx
    jz .bind
    call annotation_type
.bind:
    mov rcx,[rsi+NV]
    mov rdx,rsi
    call name_bind
    mov [rsi+80],rsi
    mov rsi,[rsi+NN]
    jmp .param
.body:
    mov qword [g_infunction],1
    mov rcx,[rbx+NA]
    call validate_sequence
    mov qword [g_infunction],0
    mov [g_env],r12
endpx
.duplicate:
    lea rcx,[err_duplicate]
    call fatal
procx validate_names
    lea rcx,[kw_sys]
    xor edx,edx
    call name_bind
    mov rsi,[g_defs]
.defs:
    test rsi,rsi
    jz .main
    mov rdx,[rsi]
    mov rcx,[rdx+NV]
    call name_bind
    mov rsi,[rsi+8]
    jmp .defs
.main:
    mov rcx,[g_program]
    call validate_sequence
    mov rsi,[g_defs]
.functions:
    test rsi,rsi
    jz .done
    mov rcx,[rsi]
    cmp qword [rcx+NK],N_CLASS
    je .class
    call validate_function
    jmp .next
.class:
    mov rdi,[rcx+NA]
.methods:
    test rdi,rdi
    jz .next
    mov rcx,rdi
    call validate_function
    mov rdi,[rdi+NN]
    jmp .methods
.next:
    mov rsi,[rsi+8]
    jmp .functions
.done:
endpx

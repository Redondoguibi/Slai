procx compile_assign
    mov rbx,rcx
    mov rsi,[rbx+NA]
    mov r12,[g_ctx]
    mov qword [rbp-64],0
    mov qword [rbp-72],0
    mov qword [rbp-80],0
    mov rax,[rsi+NK]
    cmp rax,N_NAME
    je .name
    cmp rax,N_MEMBER
    je .member
    cmp rax,N_INDEX
    je .index
    jmp .bad
.name:
    mov rcx,[rsi+NV]
    call find_variable
    mov [rbp-64],rax
    cmp qword [rbx+NV],'='
    je .rhs
    test rax,rax
    jz .undefined
    mov rcx,rsi
    call compile_expr
    mov [rbp-80],rax
    jmp .rhs
.member:
    mov rcx,[rsi+NA]
    call compile_expr
    mov rdi,rax
    mov [rbp-72],rax
    mov rdx,[rax]
    mov rcx,rsi
    call mark_member
    mov rcx,[rdi]
    mov rdx,[rsi+NV]
    call find_field
    mov [rbp-64],rax
    test rax,rax
    jnz .knownfield
    mov rax,[rdi]
    cmp [r12+F_CLASS],rax
    jne .undefined
    mov rax,[r12+F_AST]
    mov rcx,[rax+NV]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jz .undefined
    cmp qword [r12+160],0
    jne .bad
    cmp qword [rbx+NV],'='
    jne .undefined
    jmp .rhs
.knownfield:
    cmp qword [rbx+NV],'='
    je .rhs
    mov rcx,[rax+8]
    mov r13,[rax+16]
    call value_new
    mov [rbp-80],rax
    mov ecx,SI_FIELD_GET
    mov rdx,rax
    mov r8,rdi
    mov r9,r13
    call sir
    jmp .rhs
.index:
    mov rcx,[rsi+NA]
    call compile_expr
    mov [rbp-72],rax
    mov rdi,[rax]
    cmp rdi,4096
    jb .bad
    cmp qword [rdi],5
    jne .bad
    mov rcx,[rsi+NB]
    call compile_expr
    mov rcx,rax
    mov edx,T_INT
    call require_type
    mov [rbp-64],rax
    cmp qword [rbx+NV],'='
    je .rhs
    mov rcx,RT_LIST_GET
    mov rdx,[rdi+8]
    mov r8,[rbp-72]
    mov r9,rax
    call native_call
    mov [rbp-80],rax
.rhs:
    mov rcx,[rbx+NB]
    cmp qword [rcx+NK],N_OBJECT
    jne .compile_rhs
    cmp qword [rsi+NK],N_NAME
    jne .compile_rhs
    mov rax,[rsi+NV]
    mov [rcx+NV],rax
.compile_rhs:
    call compile_expr
    mov rdi,rax
    cmp qword [rax],T_VOID
    je .bad
    cmp qword [rbp-80],0
    je .annotation
    mov rax,[rbx+NV]
    sub rax,TK_ADDSET
    lea rcx,[assign_operators]
    movzx ecx,byte [rcx+rax]
    mov rdx,[rbp-80]
    mov r8,rdi
    call compile_binary
    mov rdi,rax
.annotation:
    mov rcx,[rbx+NX]
    test rcx,rcx
    jz .store
    call annotation_type
    mov rdx,rax
    mov rcx,rdi
    call require_type
.store:
    cmp qword [rsi+NK],N_NAME
    je .storevar
    cmp qword [rsi+NK],N_MEMBER
    je .storemember
    mov rax,[rbp-72]
    mov rax,[rax]
    mov rdx,[rax+8]
    mov rcx,rdi
    call require_type
    mov rcx,RT_LIST_SET
    mov edx,T_VOID
    mov r8,[rbp-72]
    mov r9,[rbp-64]
    call native_call
    mov rax,[r12+F_TAIL]
    mov [rax+40],rdi
    jmp .done
.storemember:
    mov rax,[rbp-64]
    test rax,rax
    jnz .fieldtype
    mov rax,[rbp-72]
    mov rcx,[rax]
    mov rdx,[rsi+NV]
    mov r8,[rdi]
    mov r9,[rsi+NT]
    call add_field
.fieldtype:
    mov [rsi+NZ],rax
    mov r13,[rax+16]
    mov rdx,[rax+8]
    mov rcx,rdi
    call require_type
    mov ecx,SI_FIELD_SET
    xor edx,edx
    mov r8,[rbp-72]
    mov r9,rdi
    call sir
    mov [rax+32],r13
    jmp .done
.storevar:
    mov rax,[rbp-64]
    test rax,rax
    jnz .vartype
    mov rcx,[rsi+NV]
    mov rdx,[rdi]
    mov r8,[rsi+NT]
    call bind_variable
.vartype:
    mov [rsi+NZ],rax
    mov r13,[rax+16]
    mov rdx,[rax+8]
    mov rcx,rdi
    call require_type
    mov ecx,SI_COPY
    mov rdx,r13
    mov r8,rdi
    call sir
.done:
    xor eax,eax
endpx
.bad:
    lea rcx,[err_type]
    call fatal
.undefined:
    lea rcx,[err_name]
    call fatal

procx compile_block
    mov rbx,rcx
    mov rsi,rdx
    xor edi,edi
.loop:
    test rbx,rbx
    jz .done
    test rsi,rsi
    jz .stmt
    cmp qword [rbx+NN],0
    jne .stmt
    cmp qword [rbx+NK],N_EXPR
    jne .stmt
    mov ecx,N_RETURN
    mov rdx,[rbx+NT]
    call node_new
    mov rcx,[rbx+NA]
    mov [rax+NA],rcx
    mov rcx,rax
    jmp .compile
.stmt:
    mov rcx,rbx
.compile:
    call compile_statement
    or rdi,rax
    mov rbx,[rbx+NN]
    jmp .loop
.done:
    mov rax,rdi
endpx

procx compile_statement
    mov rbx,rcx
    mov r12,[g_ctx]
    mov rax,[rbx+NT]
    mov [g_tok],rax
    mov rax,[rax+40]
    mov [g_file],rax
    mov rax,[rbx+NK]
    cmp rax,N_ASSIGN
    je .assign
    cmp rax,N_EXPR
    je .expr
    cmp rax,N_RETURN
    je .return
    cmp rax,N_IF
    je .if
    cmp rax,N_FOR
    je .for
    cmp rax,N_IMPORT
    je .import
    cmp rax,N_FUNC
    je .definition
    cmp rax,N_CLASS
    je .definition
    cmp rax,N_BREAK
    je .break
    cmp rax,N_CONTINUE
    je .continue
    cmp rax,N_ERROR
    je .error
    cmp rax,N_CATCH
    je .catch
    jmp .bad
.assign:
    mov rcx,rbx
    call compile_assign
    jmp .false
.expr:
    mov rcx,[rbx+NA]
    call compile_expr
    jmp .false
.definition:
    cmp qword [r12+F_MAIN],0
    je .bad
    jmp .false
.import:
    mov rcx,[rbx+NA]
    xor edx,edx
    call compile_block
    jmp .false
.return:
    cmp qword [r12+F_MAIN],0
    jne .bad
    mov rcx,[rbx+NA]
    test rcx,rcx
    jnz .retvalue
    mov ecx,T_VOID
    xor edx,edx
    call constant
    jmp .rettype
.retvalue:
    call compile_expr
.rettype:
    mov rsi,rax
    mov rdx,[r12+F_TYPE]
    test rdx,rdx
    jz .setreturn
    mov rcx,rsi
    call require_type
.setreturn:
    mov rax,[rsi]
    mov [r12+F_TYPE],rax
    mov ecx,SI_RETURN
    xor edx,edx
    mov r8,rsi
    call sir
    jmp .true
.if:
    mov rsi,[rbx+NA]
    call new_label
    mov r13,rax
    mov qword [rbp-64],1
    mov qword [rbp-72],0
    mov rax,[r12+F_LOCALS]
    mov [rbp-80],rax
    mov rax,[g_globals]
    mov [rbp-88],rax
.branch:
    test rsi,rsi
    jz .ifend
    call new_label
    mov r14,rax
    mov rcx,[rsi+NA]
    test rcx,rcx
    jz .else
    call compile_expr
    cmp qword [rax],T_BOOL
    ja .bad
    mov ecx,SI_BRANCH
    xor edx,edx
    mov r8,rax
    mov r9,r14
    call sir
    jmp .branchbody
.else:
    mov qword [rbp-72],1
.branchbody:
    inc qword [r12+160]
    mov rcx,[rsi+NB]
    xor edx,edx
    call compile_block
    and [rbp-64],rax
    dec qword [r12+160]
    mov rax,[rbp-80]
    mov [r12+F_LOCALS],rax
    mov rax,[rbp-88]
    mov [g_globals],rax
    mov rcx,r13
    call put_jump
    mov rcx,r14
    call put_label
    mov rsi,[rsi+NN]
    jmp .branch
.ifend:
    mov rcx,r13
    call put_label
    mov rax,[rbp-64]
    and rax,[rbp-72]
    jmp .done
.for:
    mov rax,[r12+F_LOCALS]
    mov [rbp-64],rax
    mov rax,[g_globals]
    mov [rbp-72],rax
    mov rax,[r12+F_CONTINUE]
    mov [rbp-80],rax
    mov rax,[r12+F_BREAK]
    mov [rbp-88],rax
    mov rcx,[rbx+NA]
    call compile_expr
    mov rsi,rax
    mov rcx,rax
    mov edx,T_INT
    call require_type
    mov rcx,[rbx+NV]
    call find_variable
    test rax,rax
    jnz .counter
    mov rcx,[rbx+NV]
    mov edx,T_INT
    mov r8,[rbx+NT]
    call bind_variable
.counter:
    mov rdi,[rax+16]
    cmp qword [rdi],T_INT
    jne .bad
    mov ecx,SI_COPY
    mov rdx,rdi
    mov r8,rsi
    call sir
    call new_label
    mov r13,rax
    call new_label
    mov r14,rax
    mov [r12+F_CONTINUE],rax
    call new_label
    mov r15,rax
    mov [r12+F_BREAK],rax
    mov rcx,r13
    call put_label
    mov rcx,[rbx+NB]
    call compile_expr
    cmp qword [rax],T_BOOL
    ja .bad
    mov ecx,SI_BRANCH
    xor edx,edx
    mov r8,rax
    mov r9,r15
    call sir
    inc qword [r12+160]
    mov rcx,[rbx+NC]
    xor edx,edx
    call compile_block
    dec qword [r12+160]
    mov rcx,r14
    call put_label
    mov rcx,[rbx+NX]
    test rcx,rcx
    jnz .step
    mov ecx,T_INT
    mov edx,1
    call constant
    mov r8,rax
    mov rdx,rdi
    mov ecx,'+'
    call compile_binary
    mov r8,rax
    mov rdx,rdi
    mov ecx,SI_COPY
    call sir
    jmp .loopend
.step:
    call compile_statement
.loopend:
    mov rcx,r13
    call put_jump
    mov rcx,r15
    call put_label
    mov rax,[rbp-64]
    mov [r12+F_LOCALS],rax
    mov rax,[rbp-72]
    mov [g_globals],rax
    mov rax,[rbp-80]
    mov [r12+F_CONTINUE],rax
    mov rax,[rbp-88]
    mov [r12+F_BREAK],rax
    jmp .false
.break:
    mov rcx,[r12+F_BREAK]
    jmp .control
.continue:
    mov rcx,[r12+F_CONTINUE]
.control:
    test rcx,rcx
    jz .bad
    call put_jump
    jmp .true
.error:
    mov rcx,[rbx+NA]
    call compile_expr
    mov rcx,rax
    mov edx,T_STRING
    call require_type
    mov r8,rax
    mov rcx,RT_ERROR_SET
    mov edx,T_VOID
    xor r9d,r9d
    call native_call
    mov ecx,SI_ERRORJUMP
    xor edx,edx
    mov r8,[r12+F_CATCH]
    call sir
    jmp .true
.catch:
    mov rax,[r12+F_CATCH]
    mov [rbp-64],rax
    call new_label
    mov r13,rax
    call new_label
    mov r14,rax
    mov [r12+F_CATCH],r13
    mov rcx,[rbx+NA]
    call compile_statement
    mov rax,[rbp-64]
    mov [r12+F_CATCH],rax
    mov rax,[r12+F_LOCALS]
    mov [rbp-72],rax
    mov rax,[g_globals]
    mov [rbp-80],rax
    mov rcx,r14
    call put_jump
    mov rcx,r13
    call put_label
    mov rcx,RT_ERROR_TAKE
    mov edx,T_STRING
    xor r8d,r8d
    xor r9d,r9d
    call native_call
    mov rsi,rax
    mov rcx,[rbx+NV]
    mov edx,T_STRING
    mov r8,[rbx+NT]
    call bind_variable
    mov rdx,[rax+16]
    mov ecx,SI_COPY
    mov r8,rsi
    call sir
    mov rcx,[rbx+NB]
    xor edx,edx
    call compile_block
    mov rax,[rbp-72]
    mov [r12+F_LOCALS],rax
    mov rax,[rbp-80]
    mov [g_globals],rax
    mov rcx,r14
    call put_label
.false:
    xor eax,eax
    jmp .done
.true:
    mov eax,1
.done:
endpx
.bad:
    lea rcx,[err_scope]
    call fatal

procx instantiate
    ; function AST, argument Value array, count, receiver/constructor type.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    mov r12,r9
    xor r14d,r14d
    test r12,r12
    jz .arity
    mov rcx,[rbx+NV]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jnz .arity
    mov r14,1
.arity:
    mov rax,[rbx+NC]
    add rax,r14
    cmp rax,rdi
    jne .badarity
    mov r13,[g_functions]
.cache:
    test r13,r13
    jz .new
    cmp [r13+F_AST],rbx
    jne .next
    cmp [r13+F_CLASS],r12
    jne .next
    cmp [r13+F_COUNT],rdi
    jne .next
    xor ecx,ecx
    mov rdx,[r13+F_TYPES]
.match:
    cmp rcx,rdi
    jae .found
    mov rax,[rsi+rcx*8]
    mov rax,[rax]
    cmp [rdx+rcx*8],rax
    jne .next
    inc rcx
    jmp .match
.next:
    mov r13,[r13+F_NEXT]
    jmp .cache
.found:
    mov rax,r13
    jmp .done
.new:
    mov [rbp-64],r14
    mov rax,[g_ctx]
    mov [rbp-72],rax
    mov rcx,rbx
    call new_function
    mov r13,rax
    mov [g_ctx],rax
    mov [rax+F_CLASS],r12
    mov [rax+F_COUNT],rdi
    mov ecx,128
    call alloc
    mov [r13+F_TYPES],rax
    xor ecx,ecx
.types:
    cmp rcx,rdi
    jae .params
    mov rdx,[rsi+rcx*8]
    mov rdx,[rdx]
    mov [rax+rcx*8],rdx
    inc rcx
    jmp .types
.params:
    mov r15,[rbx+NB]
    xor r14d,r14d
    mov qword [rbp-80],0
    cmp qword [rbp-64],0
    je .constructor
    lea rcx,[kw_this]
    mov rdx,r12
    mov r8,[rbx+NT]
    call bind_variable
    mov rax,[rax+16]
    mov [r13+F_PARAMS],rax
    mov [rbp-80],rax
    inc r14
    jmp .paramloop
.constructor:
    test r12,r12
    jz .paramloop
    mov [r13+F_TYPE],r12
    lea rcx,[kw_this]
    mov rdx,r12
    mov r8,[rbx+NT]
    call bind_variable
    mov rax,[rax+16]
    mov [rbp-88],rax
    mov ecx,SI_ALLOC
    mov rdx,rax
    mov r8,r12
    xor r9d,r9d
    call sir
.paramloop:
    test r15,r15
    jz .body
    mov rax,[rsi+r14*8]
    mov rdi,[rax]
    mov rcx,[r15+NX]
    test rcx,rcx
    jz .bind
    call annotation_type
    cmp rax,rdi
    jne .badtype
.bind:
    mov rcx,[r15+NV]
    mov rdx,rdi
    mov r8,[r15+NT]
    call bind_variable
    mov rax,[rax+16]
    mov rcx,[rbp-80]
    test rcx,rcx
    jnz .append
    mov [r13+F_PARAMS],rax
    jmp .tail
.append:
    mov [rcx+16],rax
.tail:
    mov [rbp-80],rax
    inc r14
    mov r15,[r15+NN]
    jmp .paramloop
.body:
    mov rcx,[rbx+NA]
    mov edx,1
    test r12,r12
    jz .compile
    cmp qword [rbp-64],0
    jne .compile
    xor edx,edx
.compile:
    call compile_block
    mov r14,rax
    test r12,r12
    jz .normalend
    cmp qword [rbp-64],0
    jne .normalend
    mov ecx,SI_RETURN
    xor edx,edx
    mov r8,[rbp-88]
    xor r9d,r9d
    call sir
    jmp .complete
.normalend:
    test r14,r14
    jnz .complete
    mov rax,[r13+F_TYPE]
    test rax,rax
    jz .void
    cmp rax,T_VOID
    jne .missingreturn
.void:
    mov qword [r13+F_TYPE],T_VOID
    mov ecx,T_VOID
    xor edx,edx
    call constant
    mov r8,rax
    mov ecx,SI_RETURN
    xor edx,edx
    call sir
.complete:
    cmp qword [r13+F_TYPE],0
    jne .complete2
    mov qword [r13+F_TYPE],T_INT
.complete2:
    mov qword [r13+F_COMPLETE],1
    mov rax,[rbp-72]
    mov [g_ctx],rax
    mov rax,r13
.done:
endpx
.badarity:
    lea rcx,[err_arity]
    call fatal
.badtype:
    lea rcx,[err_type]
    call fatal
.missingreturn:
    lea rcx,[err_return]
    call fatal

procx construct
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    mov r12,[rbx+NA]
.ctor:
    test r12,r12
    jz .empty
    mov rcx,[r12+NV]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jnz .cache
    mov r12,[r12+NN]
    jmp .ctor
.empty:
    mov r12,[rbx+NZ]
    test r12,r12
    jnz .cache
    mov ecx,N_FUNC
    mov rdx,[rbx+NT]
    call node_new
    mov r12,rax
    lea rcx,[kw_args]
    mov [rax+NV],rcx
    mov [rax+NX],rbx
    mov [rbx+NZ],rax
.cache:
    mov r13,[g_functions]
.find:
    test r13,r13
    jz .newtype
    cmp [r13+F_AST],r12
    jne .next
    cmp [r13+F_COUNT],rdi
    jne .next
    mov rdx,[r13+F_TYPES]
    xor ecx,ecx
.args:
    cmp rcx,rdi
    jae .found
    mov rax,[rsi+rcx*8]
    mov rax,[rax]
    cmp [rdx+rcx*8],rax
    jne .next
    inc rcx
    jmp .args
.next:
    mov r13,[r13+F_NEXT]
    jmp .find
.found:
    cmp qword [r13+F_COMPLETE],0
    je .recursive
    mov rax,r13
    jmp .done
.newtype:
    mov ecx,64
    call alloc
    mov qword [rax],7
    mov [rax+8],rbx
    mov r9,rax
    mov rcx,r12
    mov rdx,rsi
    mov r8,rdi
    call instantiate
.done:
endpx
.recursive:
    lea rcx,[err_recursive]
    call fatal

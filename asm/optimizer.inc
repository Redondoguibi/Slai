; Fold only values written exactly once. Mutable locals/globals never become
; constants; checked division failures remain runtime operations.
procx optimize_function
    mov r12,rcx
    mov rcx,[r12+F_SLOTS]
    inc rcx
    imul rcx,24
    call alloc
    mov r13,rax
    mov rbx,[r12+F_HEAD]
.count:
    test rbx,rbx
    jz .begin
    mov rax,[rbx+8]
    test rax,rax
    jz .countnext
    mov rax,[rax+8]
    test rax,rax
    jle .countnext
    imul rax,24
    inc qword [r13+rax]
.countnext:
    mov rbx,[rbx+48]
    jmp .count
.begin:
    mov rbx,[r12+F_HEAD]
.loop:
    test rbx,rbx
    jz .done
    mov rax,[rbx+8]
    test rax,rax
    jz .next
    mov r14,[rax+8]
    test r14,r14
    jle .next
    imul r14,24
    add r14,r13
    cmp qword [r14],1
    jne .next
    cmp qword [rbx],SI_CONST
    je .constant
    cmp qword [rbx],SI_COPY
    je .copy
    cmp qword [rbx],SI_BINARY
    je .binary
    cmp qword [rbx],SI_UNARY
    je .unary
    cmp qword [rbx],SI_COMPARE
    je .binary
    jmp .next
.constant:
    mov rax,[rbx+16]
    mov qword [r14+8],1
    mov [r14+16],rax
    jmp .next
.copy:
    mov rcx,[rbx+16]
    call .known
    test rdx,rdx
    jz .next
    jmp .fold
.unary:
    mov rcx,[rbx+16]
    call .known
    test rdx,rdx
    jz .next
    cmp qword [rbx+24],'-'
    je .neg
    cmp qword [rbx+24],'+'
    je .fold
    test rax,rax
    setnz dl
    cmp qword [rbx+24],'!'
    jne .boolean
    xor dl,1
.boolean:
    movzx eax,dl
    jmp .fold
.neg:
    neg rax
    jmp .fold
.binary:
    mov rcx,[rbx+16]
    call .known
    test rdx,rdx
    jz .next
    mov rsi,rax
    mov rcx,[rbx+24]
    call .known
    test rdx,rdx
    jz .next
    mov rdi,rax
    cmp qword [rbx],SI_COMPARE
    je .compare
    mov rax,rsi
    mov rcx,[rbx+32]
    cmp rcx,'+'
    je .add
    cmp rcx,'-'
    je .sub
    cmp rcx,'*'
    je .mul
    test rdi,rdi
    jz .next
    mov rdx,8000000000000000h
    cmp rax,rdx
    jne .divide
    cmp rdi,-1
    je .next
.divide:
    cqo
    idiv rdi
    cmp rcx,'/'
    je .fold
    mov rax,rdx
    jmp .fold
.add:
    add rax,rdi
    jmp .fold
.sub:
    sub rax,rdi
    jmp .fold
.mul:
    imul rax,rdi
    jmp .fold
.compare:
    mov rax,[rbx+16]
    cmp qword [rax],T_STRING
    jne .numeric
    mov rcx,rsi
    mov rdx,rdi
    call [strcmp]
    movsxd rsi,eax
    xor edi,edi
.numeric:
    mov rcx,[rbx+32]
    cmp rcx,TK_EQ
    je .eq
    cmp rcx,TK_NE
    je .ne
    cmp rcx,TK_LE
    je .le
    cmp rcx,TK_GE
    je .ge
    cmp rcx,'<'
    je .lt
    cmp rsi,rdi
    setg al
    jmp .boolresult
.eq:
    cmp rsi,rdi
    sete al
    jmp .boolresult
.ne:
    cmp rsi,rdi
    setne al
    jmp .boolresult
.le:
    cmp rsi,rdi
    setle al
    jmp .boolresult
.ge:
    cmp rsi,rdi
    setge al
    jmp .boolresult
.lt:
    cmp rsi,rdi
    setl al
.boolresult:
    movzx eax,al
.fold:
    mov qword [rbx],SI_CONST
    mov [rbx+16],rax
    mov qword [rbx+24],0
    mov qword [rbx+32],0
    mov qword [rbx+40],0
    inc qword [g_folded]
    jmp .constant
.next:
    mov rbx,[rbx+48]
    jmp .loop
.known:
    mov rax,[rcx+8]
    xor edx,edx
    test rax,rax
    jle .knownend
    imul rax,24
    add rax,r13
    mov rdx,[rax+8]
    mov rax,[rax+16]
.knownend:
    ret
.done:
endpx
procx optimize_program
    cmp qword [g_release],0
    je .done
    mov rsi,[g_functions]
.loop:
    test rsi,rsi
    jz .done
    mov rcx,rsi
    call optimize_function
    mov rsi,[rsi+F_NEXT]
    jmp .loop
.done:
endpx

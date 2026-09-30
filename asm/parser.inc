procx precedence
    mov rax,[g_tok]
    mov rdx,[rax]
    mov eax,1
    cmp rdx,TK_OR
    je .done
    mov eax,2
    cmp rdx,TK_AND
    je .done
    mov eax,3
    cmp rdx,TK_EQ
    je .done
    cmp rdx,TK_NE
    je .done
    mov eax,4
    cmp rdx,'<'
    je .done
    cmp rdx,'>'
    je .done
    cmp rdx,TK_LE
    je .done
    cmp rdx,TK_GE
    je .done
    mov eax,5
    cmp rdx,'+'
    je .done
    cmp rdx,'-'
    je .done
    mov eax,6
    cmp rdx,'*'
    je .done
    cmp rdx,'/'
    je .done
    cmp rdx,'%'
    je .done
    lea rcx,[kw_and]
    call token_is
    test eax,eax
    jz .or
    mov eax,2
    mov edx,TK_AND
    jmp .done
.or:
    lea rcx,[kw_or]
    call token_is
    test eax,eax
    jz .none
    mov eax,1
    mov edx,TK_OR
    jmp .done
.none:
    xor eax,eax
.done:
endpx

procx parse_expr
    mov r15,rcx
    call advance
    mov rbx,rax
    mov r12,[rbx]
    mov ecx,N_INT
    cmp r12,TK_INT
    je .literal
    mov ecx,N_STRING
    cmp r12,TK_STRING
    je .literal
    cmp r12,'$'
    je .shortstr
    cmp r12,'('
    je .group
    cmp r12,'['
    je .list
    cmp r12,'-'
    je .unary
    cmp r12,'+'
    je .unary
    cmp r12,'!'
    je .unary
    cmp r12,TK_NAME
    jne .bad
    mov rcx,[rbx+8]
    lea rdx,[kw_true]
    call equal
    test eax,eax
    jnz .true
    mov rcx,[rbx+8]
    lea rdx,[kw_false]
    call equal
    test eax,eax
    jnz .false
    mov rcx,[rbx+8]
    lea rdx,[kw_not]
    call equal
    test eax,eax
    jnz .not
    mov rcx,[rbx+8]
    lea rdx,[kw_await]
    call equal
    test eax,eax
    jnz .await
    mov ecx,N_NAME
    mov rdx,rbx
    call node_new
    mov rcx,[rbx+8]
    mov [rax+NV],rcx
    mov rsi,rax
    jmp .post
.literal:
    mov rdx,rbx
    call node_new
    mov rcx,[rbx+16]
    cmp r12,TK_INT
    je .litset
    mov rcx,[rbx+8]
.litset:
    mov [rax+NV],rcx
    mov rsi,rax
    jmp .post
.shortstr:
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_STRING
    mov r12,TK_STRING
    jmp .literal
.true:
    mov r12,1
    jmp .boolean
.false:
    xor r12d,r12d
.boolean:
    mov ecx,N_BOOL
    mov rdx,rbx
    call node_new
    mov [rax+NV],r12
    mov rsi,rax
    jmp .post
.not:
    mov r12,'!'
.unary:
    mov ecx,N_UNARY
    mov rdx,rbx
    call node_new
    mov [rax+NV],r12
    mov rsi,rax
    mov ecx,7
    call parse_expr
    mov [rsi+NA],rax
    jmp .post
.await:
    mov ecx,N_AWAIT
    mov rdx,rbx
    call node_new
    mov rsi,rax
    mov ecx,7
    call parse_expr
    mov [rsi+NA],rax
    jmp .post
.group:
    xor ecx,ecx
    call parse_expr
    mov rsi,rax
    mov ecx,')'
    call expect
    jmp .post
.list:
    mov ecx,N_LIST
    mov rdx,rbx
    call node_new
    mov rsi,rax
    mov ecx,']'
    call accept
    test rax,rax
    jnz .post
    xor edi,edi
.listitem:
    xor ecx,ecx
    call parse_expr
    test rdi,rdi
    jnz .listnext
    mov [rsi+NA],rax
    jmp .listtail
.listnext:
    mov [rdi+NN],rax
.listtail:
    mov rdi,rax
    mov ecx,','
    call accept
    test rax,rax
    jz .listend
    mov rax,[g_tok]
    cmp qword [rax],']'
    jne .listitem
.listend:
    mov ecx,']'
    call expect
.post:
    cmp r15,8
    je .binary
    mov rax,[g_tok]
    mov r12,[rax]
    cmp r12,'.'
    je .member
    cmp r12,'('
    je .call
    cmp r12,'$'
    je .index
    cmp r12,'?'
    je .propagate
    jmp .binary
.member:
    call advance
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov rdx,rax
    mov ecx,N_MEMBER
    call node_new
    mov [rax+NA],rsi
    mov rdx,[rbx+8]
    mov [rax+NV],rdx
    mov rsi,rax
    jmp .post
.index:
    call advance
    mov ecx,N_INDEX
    mov rdx,rax
    call node_new
    mov [rax+NA],rsi
    mov rsi,rax
    mov ecx,8
    call parse_expr
    mov [rsi+NB],rax
    jmp .post
.propagate:
    call advance
    mov ecx,N_PROPAGATE
    mov rdx,rax
    call node_new
    mov [rax+NA],rsi
    mov rsi,rax
    jmp .post
.call:
    call advance
    mov ecx,N_CALL
    mov rdx,[rsi+NT]
    call node_new
    mov [rax+NA],rsi
    mov rsi,rax
    mov ecx,')'
    call accept
    test rax,rax
    jnz .post
    xor edi,edi
.arg:
    xor ecx,ecx
    call parse_expr
    inc qword [rsi+NC]
    test rdi,rdi
    jnz .argnext
    mov [rsi+NB],rax
    jmp .argtail
.argnext:
    mov [rdi+NN],rax
.argtail:
    mov rdi,rax
    mov ecx,','
    call accept
    test rax,rax
    jnz .arg
    mov ecx,')'
    call expect
    jmp .post
.binary:
    call precedence
    test eax,eax
    jz .done
    cmp rax,r15
    jb .done
    mov r12,rax
    mov r13,rdx
    call advance
    mov rdx,rax
    mov ecx,N_BINARY
    call node_new
    mov [rax+NA],rsi
    mov [rax+NV],r13
    mov rsi,rax
    lea rcx,[r12+1]
    call parse_expr
    mov [rsi+NB],rax
    jmp .post
.done:
    mov rax,rsi
endpx
.bad:
    lea rcx,[err_expression]
    call fatal

procx parse_simple
    mov rbx,[g_tok]
    lea rcx,[kw_return]
    call token_is
    test eax,eax
    jnz .return
    lea rcx,[kw_error]
    call token_is
    test eax,eax
    jnz .error
    lea rcx,[kw_break]
    call token_is
    mov r12,N_BREAK
    test eax,eax
    jnz .control
    lea rcx,[kw_continue]
    call token_is
    mov r12,N_CONTINUE
    test eax,eax
    jnz .control
    mov ecx,'.'
    call accept
    xor ecx,ecx
    call parse_expr
    mov rsi,rax
    xor r13d,r13d
    mov ecx,','
    call accept
    test rax,rax
    jz .operator
    mov ecx,TK_NAME
    call expect
    mov r13,[rax+8]
.operator:
    mov rax,[g_tok]
    mov r12,[rax]
    cmp r12,TK_ARROW
    je .inlineobject
    cmp r12,'='
    je .assign
    cmp r12,TK_ADDSET
    jb .expression
    cmp r12,TK_MODSET
    jbe .assign
.expression:
    test r13,r13
    jnz .bad
    mov ecx,N_EXPR
    mov rdx,rbx
    call node_new
    mov [rax+NA],rsi
    jmp .done
.assign:
    call advance
    mov ecx,N_ASSIGN
    mov rdx,rbx
    call node_new
    mov [rax+NA],rsi
    mov [rax+NV],r12
    mov [rax+NX],r13
    mov rdi,rax
    xor ecx,ecx
    call parse_expr
    mov [rdi+NB],rax
    mov rax,rdi
    jmp .done
.inlineobject:
    call advance
    mov ecx,N_ASSIGN
    mov rdx,rbx
    call node_new
    mov [rax+NA],rsi
    mov qword [rax+NV],'='
    mov rdi,rax
    mov ecx,N_OBJECT
    mov rdx,rbx
    call node_new
    mov [rdi+NB],rax
    mov rsi,rax
    xor r12d,r12d
.inlinefield:
    call parse_field
    test r12,r12
    jnz .inlineappend
    mov [rsi+NA],rax
    jmp .inlinetail
.inlineappend:
    mov [r12+NN],rax
.inlinetail:
    mov r12,rax
    mov ecx,';'
    call accept
    test rax,rax
    jnz .inlinefield
    mov rax,rdi
    jmp .done
.return:
    call advance
    mov ecx,N_RETURN
    mov rdx,rbx
    call node_new
    mov rsi,rax
    mov rax,[g_tok]
    cmp qword [rax],TK_NEWLINE
    je .retready
    xor ecx,ecx
    call parse_expr
    mov [rsi+NA],rax
.retready:
    mov rax,rsi
    jmp .done
.error:
    call advance
    mov ecx,N_ERROR
    mov rdx,rbx
    call node_new
    mov rsi,rax
    mov rax,[g_tok]
    cmp qword [rax],TK_NAME
    jne .errstr
    call advance
    mov rbx,rax
    mov ecx,N_STRING
    mov rdx,rax
    call node_new
    mov rdx,[rbx+8]
    mov [rax+NV],rdx
    jmp .errready
.errstr:
    xor ecx,ecx
    call parse_expr
.errready:
    mov [rsi+NA],rax
    mov rax,rsi
    jmp .done
.control:
    call advance
    mov rcx,r12
    mov rdx,rbx
    call node_new
.done:
endpx
.bad:
    lea rcx,[err_expected]
    call fatal

procx parse_field
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,':'
    call expect
    mov ecx,N_FIELD
    mov rdx,rbx
    call node_new
    mov rdx,[rbx+8]
    mov [rax+NV],rdx
    mov rsi,rax
    xor ecx,ecx
    call parse_expr
    mov [rsi+NA],rax
    mov rax,rsi
endpx

procx block_begin
    mov ecx,':'
    call expect
    mov ecx,TK_NEWLINE
    call expect
    mov ecx,TK_INDENT
    call expect
endpx
procx parse_block
    call block_begin
    call parse_sequence
    mov rbx,rax
    mov ecx,TK_DEDENT
    call expect
    mov rax,rbx
endpx
procx parse_sequence
    xor ebx,ebx
    xor esi,esi
.loop:
    mov rax,[g_tok]
    cmp qword [rax],0
    je .done
    cmp qword [rax],TK_DEDENT
    je .done
    mov ecx,TK_NEWLINE
    call accept
    test rax,rax
    jnz .loop
    call parse_statement
    test rbx,rbx
    jnz .append
    mov rbx,rax
    jmp .tail
.append:
    mov [rsi+NN],rax
.tail:
    mov rsi,rax
    jmp .loop
.done:
    mov rax,rbx
endpx

procx parse_function
    ; rcx=1 for .method, 0 for func.
    mov r15,rcx
    call advance
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_FUNC
    mov rdx,rax
    call node_new
    mov rsi,rax
    mov rax,[rbx+8]
    mov [rsi+NV],rax
    mov ecx,'('
    call expect
    xor edi,edi
    mov ecx,')'
    call accept
    test rax,rax
    jnz .body
.param:
    mov ecx,'$'
    call accept
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_PARAM
    mov rdx,rax
    call node_new
    mov rdx,[rbx+8]
    mov [rax+NV],rdx
    test rdi,rdi
    jnz .append
    mov [rsi+NB],rax
    jmp .tail
.append:
    mov [rdi+NN],rax
.tail:
    mov rdi,rax
    inc qword [rsi+NC]
    cmp qword [rsi+NC],16
    ja .bad
    mov ecx,':'
    call accept
    test rax,rax
    jz .nextparam
    mov ecx,TK_NAME
    call expect
    mov rax,[rax+8]
    mov [rdi+NX],rax
.nextparam:
    mov ecx,','
    call accept
    test rax,rax
    jnz .param
    mov ecx,')'
    call expect
.body:
    mov ecx,TK_ARROW
    call accept
    test rax,rax
    jz .block
    call parse_simple
    mov [rsi+NA],rax
    mov ecx,TK_NEWLINE
    call expect
    jmp .done
.block:
    call parse_block
    mov [rsi+NA],rax
.done:
    mov rax,rsi
endpx
.bad:
    lea rcx,[err_limit]
    call fatal

procx parse_statement
    mov rbx,[g_tok]
    lea rcx,[kw_func]
    call token_is
    test eax,eax
    jnz .func
    lea rcx,[kw_class]
    call token_is
    test eax,eax
    jnz .class
    lea rcx,[kw_obj]
    call token_is
    test eax,eax
    jnz .object
    lea rcx,[kw_if]
    call token_is
    test eax,eax
    jnz .if
    lea rcx,[kw_for]
    call token_is
    test eax,eax
    jnz .for
    lea rcx,[kw_import]
    call token_is
    test eax,eax
    jnz .import
    call parse_simple
    mov rsi,rax
    mov rax,[g_tok]
    cmp qword [rax],':'
    je .iaedit
    mov ecx,TK_CATCH
    call accept
    test rax,rax
    jnz .catch
    mov ecx,TK_NEWLINE
    call expect
    mov rax,rsi
    jmp .done
.catch:
    jmp .catchbody
.iaedit:
    mov rdi,[rsi+NA]
    cmp qword [rsi+NK],N_ASSIGN
    jne .editcall
    mov rdi,[rsi+NB]
.editcall:
    cmp qword [rdi+NK],N_CALL
    jne .bad
    mov rcx,rdi
    call ia_operation
    test rax,rax
    jz .bad
    mov rcx,rax
    lea rdx,[kw_edit]
    call equal
    test eax,eax
    jz .bad
    call block_begin
    mov ecx,'.'
    call expect
    lea rcx,[kw_param]
    call token_is
    test eax,eax
    jz .bad
    call advance
    mov ecx,TK_NAME
    call expect
    mov rax,[rax+8]
    mov [rdi+NX],rax
    mov ecx,','
    call expect
    mov ecx,TK_NAME
    call expect
    mov rax,[rax+8]
    mov [rdi+NZ],rax
    mov ecx,TK_NEWLINE
    call expect
    mov ecx,TK_DEDENT
    call expect
    mov rax,rsi
    jmp .done
.catchbody:
    mov ecx,TK_NAME
    call expect
    mov rdi,rax
    mov ecx,N_CATCH
    mov rdx,rdi
    call node_new
    mov [rax+NA],rsi
    mov rdx,[rdi+8]
    mov [rax+NV],rdx
    mov rsi,rax
    call parse_block
    mov [rsi+NB],rax
    mov rax,rsi
    jmp .done
.func:
    xor ecx,ecx
    call parse_function
    jmp .done
.class:
    call advance
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_CLASS
    mov rdx,rax
    call node_new
    mov rsi,rax
    mov rax,[rbx+8]
    mov [rsi+NV],rax
    call block_begin
    xor edi,edi
.method:
    mov ecx,TK_DEDENT
    call accept
    test rax,rax
    jnz .classdone
    mov rax,[g_tok]
    cmp qword [rax],'.'
    jne .bad
    mov ecx,1
    call parse_function
    mov [rax+NX],rsi
    test rdi,rdi
    jnz .methodappend
    mov [rsi+NA],rax
    jmp .methodtail
.methodappend:
    mov [rdi+NN],rax
.methodtail:
    mov rdi,rax
    jmp .method
.classdone:
    mov rax,rsi
    jmp .done
.object:
    call advance
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_NAME
    mov rdx,rax
    call node_new
    mov rcx,[rbx+8]
    mov [rax+NV],rcx
    mov rdi,rax
    mov ecx,N_ASSIGN
    mov rdx,rbx
    call node_new
    mov [rax+NA],rdi
    mov qword [rax+NV],'='
    mov rsi,rax
    mov ecx,N_OBJECT
    mov rdx,rbx
    call node_new
    mov [rsi+NB],rax
    mov r12,rax
    call block_begin
    xor edi,edi
.field:
    mov ecx,TK_DEDENT
    call accept
    test rax,rax
    jnz .classdone
    call parse_field
    test rdi,rdi
    jnz .fieldappend
    mov [r12+NA],rax
    jmp .fieldtail
.fieldappend:
    mov [rdi+NN],rax
.fieldtail:
    mov rdi,rax
    mov ecx,TK_NEWLINE
    call expect
    jmp .field
.if:
    call advance
    mov ecx,N_IF
    mov rdx,rbx
    call node_new
    mov rsi,rax
    call block_begin
    xor edi,edi
    xor r13d,r13d
.branch:
    mov ecx,TK_DEDENT
    call accept
    test rax,rax
    jnz .classdone
    test r13,r13
    jnz .bad
    mov ecx,N_BRANCH
    mov rdx,[g_tok]
    call node_new
    mov r12,rax
    test rdi,rdi
    jnz .branchappend
    mov [rsi+NA],rax
    jmp .branchtail
.branchappend:
    mov [rdi+NN],rax
.branchtail:
    mov rdi,rax
    lea rcx,[kw_else]
    call token_is
    test eax,eax
    jnz .else
    xor ecx,ecx
    call parse_expr
    mov [r12+NA],rax
    jmp .action
.else:
    call advance
    mov r13,1
.action:
    mov ecx,TK_ARROW
    call accept
    test rax,rax
    jz .actionblock
    call parse_simple
    mov [r12+NB],rax
    mov ecx,TK_NEWLINE
    call expect
    jmp .branch
.actionblock:
    call parse_block
    mov [r12+NB],rax
    jmp .branch
.for:
    call advance
    mov ecx,TK_NAME
    call expect
    mov rbx,rax
    mov ecx,N_FOR
    mov rdx,rax
    call node_new
    mov rsi,rax
    mov rax,[rbx+8]
    mov [rsi+NV],rax
    mov ecx,'='
    call expect
    xor ecx,ecx
    call parse_expr
    mov [rsi+NA],rax
    mov ecx,';'
    call expect
    call precedence
    cmp eax,3
    jb .fullcondition
    cmp eax,4
    ja .fullcondition
    mov r12,rdx
    call advance
    mov ecx,N_NAME
    mov rdx,rbx
    call node_new
    mov rdx,[rbx+8]
    mov [rax+NV],rdx
    mov rdi,rax
    mov ecx,N_BINARY
    mov rdx,rbx
    call node_new
    mov [rax+NV],r12
    mov [rax+NA],rdi
    mov rdi,rax
    xor ecx,ecx
    call parse_expr
    mov [rdi+NB],rax
    mov rax,rdi
    jmp .condition
.fullcondition:
    xor ecx,ecx
    call parse_expr
.condition:
    mov [rsi+NB],rax
    mov ecx,';'
    call accept
    test rax,rax
    jz .loopbody
    call parse_simple
    mov [rsi+NX],rax
.loopbody:
    call parse_block
    mov [rsi+NC],rax
    mov rax,rsi
    jmp .done
.import:
    call advance
    mov ecx,N_IMPORT
    mov rdx,rbx
    call node_new
    mov rsi,rax
    mov ecx,TK_STRING
    call accept
    test rax,rax
    jnz .importname
    mov ecx,'$'
    call expect
    mov ecx,4096
    call alloc
    mov rdi,rax
    mov ecx,TK_NAME
    call expect
    mov rcx,rdi
    mov rdx,[rax+8]
    call [strcpy]
.importpart:
    mov ecx,'.'
    call accept
    test rax,rax
    jz .importpath
    mov rcx,rdi
    lea rdx,[dot]
    call [strcat]
    mov ecx,TK_NAME
    call expect
    mov rcx,rdi
    mov rdx,[rax+8]
    call [strcat]
    jmp .importpart
.importpath:
    mov [rsi+NV],rdi
    jmp .importend
.importname:
    mov rax,[rax+8]
    mov [rsi+NV],rax
.importend:
    mov ecx,TK_NEWLINE
    call expect
    mov rax,rsi
.done:
endpx
.bad:
    lea rcx,[err_expected]
    call fatal

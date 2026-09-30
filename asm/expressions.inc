procx compile_binary
    ; operation, left Value, right Value -> result Value.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    mov rcx,rdi
    mov rdx,[rsi]
    call require_type
    mov r12,SI_COMPARE
    mov rcx,T_BOOL
    cmp rbx,TK_EQ
    je .comparison
    cmp rbx,TK_NE
    je .comparison
    cmp rbx,TK_LE
    je .comparison
    cmp rbx,TK_GE
    je .comparison
    cmp rbx,'<'
    je .comparison
    cmp rbx,'>'
    je .comparison
    cmp qword [rsi],T_INT
    jne .bad
    mov rcx,T_INT
    mov r12,SI_BINARY
    jmp .make
.comparison:
    cmp qword [rsi],T_STRING
    ja .bad
.make:
    call value_new
    mov r13,rax
    mov rcx,r12
    mov rdx,rax
    mov r8,rsi
    mov r9,rdi
    call sir
    mov [rax+32],rbx
    mov rax,r13
endpx
.bad:
    lea rcx,[err_type]
    call fatal

procx compile_expr
    mov rbx,rcx
    mov rax,[rbx+NT]
    mov [g_tok],rax
    mov rax,[rax+40]
    mov [g_file],rax
    mov rax,[rbx+NK]
    cmp rax,N_INT
    je .int
    cmp rax,N_BOOL
    je .bool
    cmp rax,N_STRING
    je .string
    cmp rax,N_NAME
    je .name
    cmp rax,N_BINARY
    je .binary
    cmp rax,N_UNARY
    je .unary
    cmp rax,N_CALL
    je .call
    cmp rax,N_LIST
    je .list
    cmp rax,N_OBJECT
    je .object
    cmp rax,N_MEMBER
    je .member
    cmp rax,N_INDEX
    je .index
    cmp rax,N_PROPAGATE
    je .propagate
    cmp rax,N_AWAIT
    je .await
    jmp .bad
.int:
    mov rdx,[rbx+NV]
    test rdx,rdx
    js .bad
    mov ecx,T_INT
    call constant
    jmp .done
.bool:
    mov ecx,T_BOOL
    mov rdx,[rbx+NV]
    call constant
    jmp .done
.string:
    mov ecx,T_STRING
    mov rdx,[rbx+NV]
    call constant
    jmp .done
.name:
    mov rcx,[rbx+NV]
    call find_variable
    test rax,rax
    jz .undefined
    mov rsi,[rax+16]
    mov [rbx+NZ],rax
    mov rcx,[rsi]
    call value_new
    mov rdi,rax
    mov ecx,SI_COPY
    mov rdx,rax
    mov r8,rsi
    call sir
    mov rax,rdi
    jmp .done
.binary:
    mov rcx,[rbx+NA]
    call compile_expr
    mov rsi,rax
    cmp qword [rbx+NV],TK_AND
    je .logical
    cmp qword [rbx+NV],TK_OR
    je .logical
    mov rcx,[rbx+NB]
    call compile_expr
    mov r8,rax
    mov rdx,rsi
    mov rcx,[rbx+NV]
    call compile_binary
    jmp .done
.logical:
    cmp qword [rsi],T_BOOL
    ja .bad
    mov ecx,T_BOOL
    xor edx,edx
    cmp qword [rbx+NV],TK_OR
    sete dl
    call constant
    mov rdi,rax
    call new_label
    mov r12,rax
    mov ecx,SI_BRANCH
    xor edx,edx
    mov r8,rsi
    mov r9,r12
    call sir
    xor edx,edx
    cmp qword [rbx+NV],TK_OR
    sete dl
    mov [rax+32],rdx
    mov rcx,[rbx+NB]
    call compile_expr
    cmp qword [rax],T_BOOL
    ja .bad
    mov r8,rax
    mov ecx,SI_UNARY
    mov rdx,rdi
    mov r9,'b'
    call sir
    mov rcx,r12
    call put_label
    mov rax,rdi
    jmp .done
.unary:
    mov rsi,[rbx+NA]
    cmp qword [rbx+NV],'-'
    jne .normalunary
    cmp qword [rsi+NK],N_INT
    jne .normalunary
    mov rax,8000000000000000h
    cmp [rsi+NV],rax
    jne .normalunary
    mov rdx,rax
    mov ecx,T_INT
    call constant
    jmp .done
.normalunary:
    mov rcx,rsi
    call compile_expr
    mov rsi,rax
    mov ecx,T_INT
    cmp qword [rbx+NV],'!'
    jne .numeric
    cmp qword [rsi],T_BOOL
    ja .bad
    mov ecx,T_BOOL
    jmp .unaryresult
.numeric:
    cmp qword [rsi],T_INT
    jne .bad
.unaryresult:
    call value_new
    mov rdi,rax
    mov ecx,SI_UNARY
    mov rdx,rax
    mov r8,rsi
    mov r9,[rbx+NV]
    call sir
    mov rax,rdi
    jmp .done
.call:
    mov rcx,rbx
    call compile_call
    jmp .done
.list:
    mov rsi,[rbx+NA]
    test rsi,rsi
    jz .bad
    mov rcx,rsi
    call compile_expr
    mov r12,rax
    mov rdx,[rax]
    mov ecx,5
    call composite_type
    mov rdi,rax
    mov rcx,RT_LIST_NEW
    mov rdx,rdi
    xor r8d,r8d
    xor r9d,r9d
    call native_call
    mov r13,rax
.push:
    mov rcx,r12
    mov rdx,[rdi+8]
    call require_type
    mov rcx,RT_LIST_PUSH
    mov edx,T_VOID
    mov r8,r13
    mov r9,r12
    call native_call
    mov rsi,[rsi+NN]
    test rsi,rsi
    jz .listdone
    mov rcx,rsi
    call compile_expr
    mov r12,rax
    jmp .push
.listdone:
    mov rax,r13
    jmp .done
.object:
    mov ecx,64
    call alloc
    mov r12,rax
    mov qword [rax],6
    mov [rax+8],rbx
    mov rcx,r12
    call value_new
    mov r13,rax
    mov ecx,SI_ALLOC
    mov rdx,rax
    mov r8,r12
    call sir
    mov rsi,[rbx+NA]
.objectfield:
    test rsi,rsi
    jz .objectdone
    mov rcx,[rsi+NA]
    call compile_expr
    mov rdi,rax
    mov rcx,r12
    mov rdx,[rsi+NV]
    call find_field
    test rax,rax
    jnz .bad
    mov rcx,r12
    mov rdx,[rsi+NV]
    mov r8,[rdi]
    mov r9,[rsi+NT]
    call add_field
    mov [rsi+NZ],rax
    mov r14,[rax+16]
    mov ecx,SI_FIELD_SET
    xor edx,edx
    mov r8,r13
    mov r9,rdi
    call sir
    mov [rax+32],r14
    mov rsi,[rsi+NN]
    jmp .objectfield
.objectdone:
    mov rax,r13
    jmp .done
.member:
    mov rcx,[rbx+NA]
    call compile_expr
    mov rsi,rax
    mov rdi,[rax]
    cmp rdi,4096
    jb .bad
    cmp qword [rdi],5
    jne .objmember
    mov rcx,[rbx+NV]
    lea rdx,[kw_length]
    call equal
    test eax,eax
    jz .bad
    mov ecx,T_INT
    xor r12d,r12d
    jmp .getmember
.objmember:
    mov rcx,rbx
    mov rdx,rdi
    call mark_member
    mov rcx,rdi
    mov rdx,[rbx+NV]
    call find_field
    test rax,rax
    jz .undefined
    mov [rbx+NZ],rax
    mov rcx,[rax+8]
    mov r12,[rax+16]
.getmember:
    call value_new
    mov rdi,rax
    mov ecx,SI_FIELD_GET
    mov rdx,rax
    mov r8,rsi
    mov r9,r12
    call sir
    mov rax,rdi
    jmp .done
.index:
    mov rcx,[rbx+NA]
    call compile_expr
    mov rsi,rax
    mov rdi,[rax]
    cmp rdi,4096
    jb .bad
    cmp qword [rdi],5
    jne .bad
    mov rcx,[rbx+NB]
    call compile_expr
    mov rcx,rax
    mov edx,T_INT
    call require_type
    mov r9,rax
    mov r8,rsi
    mov rdx,[rdi+8]
    mov rcx,RT_LIST_GET
    call native_call
    jmp .done
.propagate:
    mov rsi,[g_ctx]
    mov rdi,[rsi+F_CATCH]
    mov qword [rsi+F_CATCH],0
    mov rcx,[rbx+NA]
    mov rax,[rcx+NK]
    cmp rax,N_CALL
    je .propagatecall
    cmp rax,N_AWAIT
    jne .bad
.propagatecall:
    call compile_expr
    mov [rsi+F_CATCH],rdi
    jmp .done
.await:
    mov rcx,[rbx+NA]
    call compile_expr
    mov rsi,rax
    mov rdx,[rax]
    cmp rdx,4096
    jb .bad
    cmp qword [rdx],8
    jne .bad
    mov rdx,[rdx+8]
    mov rcx,RT_AWAIT
    mov r8,rsi
    xor r9d,r9d
    call native_call
    mov rsi,rax
    call error_check
    mov rax,rsi
.done:
    mov rcx,[rax]
    mov [rbx+NY],rcx
endpx
.undefined:
    lea rcx,[err_name]
    call fatal
.bad:
    lea rcx,[err_type]
    call fatal

procx compile_args
    ; AST argument chain -> array of Value pointers (max16), count in rdx.
    mov rbx,rcx
    mov ecx,128
    call alloc
    mov rsi,rax
    xor edi,edi
.loop:
    test rbx,rbx
    jz .done
    cmp rdi,16
    jae .bad
    mov rcx,rbx
    call compile_expr
    mov [rsi+rdi*8],rax
    inc rdi
    mov rbx,[rbx+NN]
    jmp .loop
.done:
    mov rax,rsi
    mov rdx,rdi
endpx
.bad:
    lea rcx,[err_limit]
    call fatal

procx compile_call
    mov rbx,rcx
    call ia_operation
    test rax,rax
    jz .ordinary
    mov rdx,rax
    mov rcx,rbx
    call compile_ia
    jmp .done
.ordinary:
    mov rsi,[rbx+NA]
    cmp qword [rsi+NK],N_NAME
    je .function
    cmp qword [rsi+NK],N_MEMBER
    jne .bad
    mov rdi,[rsi+NA]
    cmp qword [rdi+NK],N_NAME
    jne .method
    mov rcx,[rdi+NV]
    lea rdx,[kw_sys]
    call equal
    test eax,eax
    jnz .system
.method:
    mov rcx,rdi
    call compile_expr
    mov r12,rax
    mov rdi,[rax]
    cmp rdi,4096
    jb .bad
    cmp qword [rdi],5
    je .push
    cmp qword [rdi],7
    jne .bad
    mov rcx,rsi
    mov rdx,rdi
    call mark_member
    mov rax,[rdi+8]
    mov r13,[rax+NA]
.findmethod:
    test r13,r13
    jz .undefined
    mov rcx,[r13+NV]
    mov rdx,[rsi+NV]
    call equal
    test eax,eax
    jnz .methodfound
    mov r13,[r13+NN]
    jmp .findmethod
.methodfound:
    mov rcx,[rbx+NB]
    call compile_args
    cmp rdx,15
    ja .bad
    mov r14,rdx
.shift:
    test rdx,rdx
    jz .insert
    mov rcx,[rax+rdx*8-8]
    mov [rax+rdx*8],rcx
    dec rdx
    jmp .shift
.insert:
    mov [rax],r12
    inc r14
    mov r12,rax
    mov rcx,r13
    mov rdx,r12
    mov r8,r14
    mov r9,rdi
    call instantiate
    jmp .emitcall
.push:
    mov rcx,[rsi+NV]
    lea rdx,[kw_push]
    call equal
    test eax,eax
    jz .undefined
    cmp qword [rbx+NC],1
    jne .arity
    mov rcx,[rbx+NB]
    call compile_expr
    mov rcx,rax
    mov rdx,[rdi+8]
    call require_type
    mov r9,rax
    mov r8,r12
    mov rcx,RT_LIST_PUSH
    mov edx,T_VOID
    call native_call
    jmp .done
.function:
    mov rcx,[rsi+NV]
    call find_definition
    test rax,rax
    jz .undefined
    mov r13,rax
    mov rcx,[rbx+NB]
    call compile_args
    mov r12,rax
    mov r14,rdx
    cmp qword [r13+NK],N_CLASS
    je .construct
    mov rcx,r13
    mov rdx,r12
    mov r8,r14
    xor r9d,r9d
    call instantiate
    jmp .emitcall
.construct:
    mov rcx,r13
    mov rdx,r12
    mov r8,r14
    call construct
.emitcall:
    mov r13,rax
    mov rcx,[rax+F_TYPE]
    test rcx,rcx
    jnz .calltype
    mov ecx,T_INT
    mov [r13+F_TYPE],rcx
.calltype:
    call value_new
    mov rdi,rax
    mov ecx,SI_CALL
    mov rdx,rax
    mov r8,r13
    mov r9,r12
    call sir
    mov [rax+32],r14
    call error_check
    mov rax,rdi
    jmp .done
.system:
    mov rcx,[rsi+NV]
    lea rdx,[kw_log]
    call equal
    test eax,eax
    jnz .log
    mov rcx,[rsi+NV]
    lea rdx,[kw_spawn]
    call equal
    test eax,eax
    jnz .spawn
    mov rcx,[rsi+NV]
    lea rdx,[kw_sleep]
    call equal
    mov r13,RT_SLEEP
    test eax,eax
    jnz .single
    mov rcx,[rsi+NV]
    lea rdx,[kw_assert]
    call equal
    mov r13,RT_ASSERT
    test eax,eax
    jnz .single
    jmp .unsupported
.log:
    cmp qword [rbx+NC],1
    jne .arity
    mov rcx,[rbx+NB]
    call compile_expr
    mov r8,rax
    mov rcx,RT_LOG_INT
    cmp qword [rax],T_INT
    je .logcall
    mov rcx,RT_LOG_STR
    cmp qword [rax],T_STRING
    je .logcall
    mov rcx,RT_LOG_BOOL
    cmp qword [rax],T_BOOL
    jne .bad
.logcall:
    mov edx,T_VOID
    xor r9d,r9d
    call native_call
    jmp .done
.single:
    cmp qword [rbx+NC],1
    jne .arity
    mov rcx,[rbx+NB]
    call compile_expr
    mov r12,rax
    mov rax,[rax]
    cmp rax,T_BOOL
    ja .bad
    xor edi,edi
    mov rax,RT_ASSERT
    cmp r13,rax
    jne .singlecall
    mov ecx,T_STRING
    lea rdx,[assert_message]
    call constant
    mov rdi,rax
.singlecall:
    mov rcx,r13
    mov edx,T_VOID
    mov r8,r12
    mov r9,rdi
    call native_call
    mov rdi,rax
    call error_check
    mov rax,rdi
    jmp .done
.spawn:
    mov rsi,[rbx+NB]
    test rsi,rsi
    jz .arity
    cmp qword [rsi+NK],N_NAME
    jne .bad
    mov rcx,[rsi+NV]
    call find_definition
    test rax,rax
    jz .undefined
    cmp qword [rax+NK],N_FUNC
    jne .bad
    mov r13,rax
    mov rcx,[rsi+NN]
    call compile_args
    mov r12,rax
    mov r14,rdx
    mov rcx,r13
    mov rdx,r12
    mov r8,r14
    xor r9d,r9d
    call instantiate
    mov r13,rax
    mov rdx,[rax+F_TYPE]
    mov ecx,8
    call composite_type
    mov rdi,rax
    mov ecx,T_INT
    call value_new
    mov rsi,rax
    mov ecx,SI_FADDR
    mov rdx,rax
    mov r8,r13
    call sir
    mov ecx,T_INT
    call value_new
    mov r13,rax
    mov ecx,SI_ARRAY
    mov rdx,rax
    mov r8,r12
    mov r9,r14
    call sir
    mov ecx,T_INT
    mov rdx,r14
    call constant
    mov r14,rax
    mov rcx,RT_SPAWN
    mov rdx,rdi
    mov r8,rsi
    mov r9,r13
    call native_call
    mov rdi,rax
    mov rax,[g_ctx]
    mov rax,[rax+F_TAIL]
    mov [rax+40],r14
    mov rax,rdi
.done:
endpx
.undefined:
    lea rcx,[err_name]
    call fatal
.arity:
    lea rcx,[err_arity]
    call fatal
.unsupported:
    lea rcx,[err_unsupported]
    call fatal
.bad:
    lea rcx,[err_type]
    call fatal


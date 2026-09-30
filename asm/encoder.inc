; Direct x86-64 encoder. No assembler is launched by the Slai compiler.
procx code_bytes
    mov r8,rdx
    mov rdx,rcx
    mov rcx,[g_codesize]
    add r8,rcx
    cmp r8,1048576
    ja .limit
    mov [g_codesize],r8
    sub r8,rcx
    add rcx,[g_code]
    call [memcpy]
endpx
.limit:
    lea rcx,[err_limit]
    call fatal
procx code_u32
    mov [rbp-64],ecx
    lea rcx,[rbp-64]
    mov edx,4
    call code_bytes
endpx
procx code_u64
    mov [rbp-64],rcx
    lea rcx,[rbp-64]
    mov edx,8
    call code_bytes
endpx
procx code_imm
    mov rbx,rcx
    emitbytes 048h,0B8h
    mov rcx,rbx
    call code_u64
endpx
procx code_load
    ; Value -> RAX. Only RAX/R11 are changed in generated code.
    mov rbx,[rcx+8]
    test rbx,rbx
    js .global
    emitbytes 048h,08Bh,085h
    neg rbx
    lea rcx,[rbx*8]
    call code_u32
    jmp .done
.global:
    neg rbx
    lea rbx,[rbx*8+8]
    mov rax,DATA_BASE
    add rbx,rax
    emitbytes 049h,0BBh
    mov rcx,rbx
    call code_u64
    emitbytes 049h,08Bh,003h
.done:
endpx
procx code_save
    mov rbx,[rcx+8]
    test rbx,rbx
    js .global
    emitbytes 048h,089h,085h
    neg rbx
    lea rcx,[rbx*8]
    call code_u32
    jmp .done
.global:
    neg rbx
    lea rbx,[rbx*8+8]
    mov rax,DATA_BASE
    add rbx,rax
    emitbytes 049h,0BBh
    mov rcx,rbx
    call code_u64
    emitbytes 049h,089h,003h
.done:
endpx
procx code_native
    call code_imm
    emitbytes 0FFh,0D0h
endpx
procx code_function_address
    mov rbx,rcx
    emitbytes 048h,0B8h
    mov ecx,24
    call alloc
    mov rcx,[g_code]
    add rcx,[g_codesize]
    mov [rax],rcx
    mov [rax+8],rbx
    mov rcx,[g_fixups]
    mov [rax+16],rcx
    mov [g_fixups],rax
    xor ecx,ecx
    call code_u64
endpx
procx code_jump
    ; opcode (E9, 84=JZ,85=JNZ), label; label zero is error exit.
    mov rbx,rdx
    cmp ecx,0E9h
    je .shortop
    mov [rbp-64],cl
    emitbytes 00Fh
    lea rcx,[rbp-64]
    mov edx,1
    call code_bytes
    jmp .fix
.shortop:
    emitbytes 0E9h
.fix:
    mov ecx,24
    call alloc
    mov rcx,[g_codesize]
    mov [rax],rcx
    mov [rax+8],rbx
    mov rcx,[g_jumpfix]
    mov [rax+16],rcx
    mov [g_jumpfix],rax
    xor ecx,ecx
    call code_u32
endpx
procx code_epilog
    emitbytes 048h,081h,0C4h
    mov rax,[g_encodingfn]
    mov rcx,[rax+F_FRAME]
    call code_u32
    emitbytes 05Dh,0C3h
endpx
procx add_string
    mov rbx,rcx
    call [strlen]
    lea rsi,[rax+1]
    mov rdi,[g_constsize]
    lea rax,[rdi+rsi]
    cmp rax,1048576
    ja .bad
    mov [g_constsize],rax
    mov rcx,[g_constants]
    add rcx,rdi
    mov rdx,rbx
    mov r8,rsi
    call [memcpy]
    mov rax,CONST_BASE
    add rax,rdi
endpx
.bad:
    lea rcx,[err_limit]
    call fatal
procx code_arguments
    ; array of Value*, count. Windows x64 ABI, 16 args maximum.
    mov rbx,rcx
    mov rsi,rdx
    xor edi,edi
.loop:
    cmp rdi,rsi
    jae .done
    mov rcx,[rbx+rdi*8]
    call code_load
    cmp rdi,0
    je .rcx
    cmp rdi,1
    je .rdx
    cmp rdi,2
    je .r8
    cmp rdi,3
    je .r9
    emitbytes 048h,089h,084h,024h
    lea rcx,[rdi*8]
    call code_u32
    jmp .next
.rcx:
    emitbytes 048h,089h,0C1h
    jmp .next
.rdx:
    emitbytes 048h,089h,0C2h
    jmp .next
.r8:
    emitbytes 049h,089h,0C0h
    jmp .next
.r9:
    emitbytes 049h,089h,0C1h
.next:
    inc rdi
    jmp .loop
.done:
endpx
procx encode_function
    mov r12,rcx
    mov [g_encodingfn],rcx
    mov ecx,1048576
    call alloc
    mov [r12+F_CODE],rax
    mov [g_code],rax
    mov qword [g_codesize],0
    mov qword [g_jumpfix],0
    mov rcx,[g_label]
    lea rcx,[rcx*8+8]
    call alloc
    mov [g_labels],rax
    mov rax,[r12+F_SLOTS]
    lea rax,[rax*8+143]
    and rax,-16
    mov [r12+F_FRAME],rax
    emitbytes 055h,048h,089h,0E5h,048h,081h,0ECh
    mov rcx,[r12+F_FRAME]
    call code_u32
    mov rsi,[r12+F_PARAMS]
    xor edi,edi
.params:
    test rsi,rsi
    jz .body
    cmp rdi,0
    je .rcx
    cmp rdi,1
    je .rdx
    cmp rdi,2
    je .r8
    cmp rdi,3
    je .r9
    emitbytes 048h,08Bh,085h
    lea rcx,[rdi*8+16]
    call code_u32
    jmp .saveparam
.rcx:
    emitbytes 048h,089h,0C8h
    jmp .saveparam
.rdx:
    emitbytes 048h,089h,0D0h
    jmp .saveparam
.r8:
    emitbytes 04Ch,089h,0C0h
    jmp .saveparam
.r9:
    emitbytes 04Ch,089h,0C8h
.saveparam:
    mov rcx,rsi
    call code_save
    inc rdi
    mov rsi,[rsi+16]
    jmp .params
.body:
    mov rbx,[r12+F_HEAD]
.loop:
    test rbx,rbx
    jz .finish
    mov rcx,rbx
    call encode_instruction
    mov rbx,[rbx+48]
    jmp .loop
.finish:
    mov rax,[g_labels]
    mov rcx,[g_codesize]
    mov [rax],rcx
    emitbytes 031h,0C0h
    call code_epilog
    mov rax,[g_codesize]
    mov [r12+F_SIZE],rax
    mov rsi,[g_jumpfix]
.patch:
    test rsi,rsi
    jz .done
    mov rcx,[rsi+8]
    mov rax,[g_labels]
    mov rax,[rax+rcx*8]
    mov rcx,[rsi]
    sub rax,rcx
    sub rax,4
    add rcx,[g_code]
    mov [rcx],eax
    mov rsi,[rsi+16]
    jmp .patch
.done:
endpx
procx encode_instruction
    mov rbx,rcx
    mov rax,[rbx]
    cmp rax,SI_CONST
    je .const
    cmp rax,SI_COPY
    je .copy
    cmp rax,SI_BINARY
    je .binary
    cmp rax,SI_COMPARE
    je .compare
    cmp rax,SI_UNARY
    je .unary
    cmp rax,SI_NATIVE
    je .native
    cmp rax,SI_CALL
    je .call
    cmp rax,SI_LABEL
    je .label
    cmp rax,SI_JUMP
    je .jump
    cmp rax,SI_BRANCH
    je .branch
    cmp rax,SI_RETURN
    je .return
    cmp rax,SI_FIELD_GET
    je .get
    cmp rax,SI_FIELD_SET
    je .set
    cmp rax,SI_ERRORCHECK
    je .check
    cmp rax,SI_ERRORJUMP
    je .jump
    cmp rax,SI_ALLOC
    je .alloc
    cmp rax,SI_FADDR
    je .address
    cmp rax,SI_ARRAY
    je .array
    lea rcx,[err_internal]
    call fatal
.const:
    mov rsi,[rbx+16]
    mov rax,[rbx+8]
    cmp qword [rax],T_CHANGE
    je .change
    cmp qword [rax],T_STRING
    jne .imm
    mov rcx,rsi
    call add_string
    mov rsi,rax
.imm:
    mov rcx,rsi
    call code_imm
    jmp .save
.change:
    mov rcx,rsi
    xor edx,edx
    call encode_plan
    mov rsi,rax
    jmp .imm
.copy:
    mov rcx,[rbx+16]
    call code_load
    jmp .save
.binary:
    mov rcx,[rbx+24]
    call code_load
    emitbytes 049h,089h,0C2h
    mov rcx,[rbx+16]
    call code_load
    mov rsi,[rbx+32]
    cmp rsi,'+'
    je .add
    cmp rsi,'-'
    je .sub
    cmp rsi,'*'
    je .mul
    emitbytes 048h,089h,0C1h,04Ch,089h,0D2h
    mov rcx,RT_DIV
    cmp rsi,'/'
    je .div
    mov rcx,RT_MOD
.div:
    call code_native
    jmp .save
.add:
    emitbytes 04Ch,001h,0D0h
    jmp .save
.sub:
    emitbytes 04Ch,029h,0D0h
    jmp .save
.mul:
    emitbytes 049h,00Fh,0AFh,0C2h
    jmp .save
.compare:
    mov rcx,[rbx+24]
    call code_load
    emitbytes 049h,089h,0C2h
    mov rcx,[rbx+16]
    call code_load
    mov rax,[rbx+16]
    cmp qword [rax],T_STRING
    jne .cmpint
    emitbytes 048h,089h,0C1h,04Ch,089h,0D2h
    mov rcx,RT_COMPARE
    call code_native
    emitbytes 048h,083h,0F8h,0
    jmp .condition
.cmpint:
    emitbytes 04Ch,039h,0D0h
.condition:
    mov rax,[rbx+32]
    mov esi,094h
    cmp rax,TK_EQ
    je .cc
    mov esi,095h
    cmp rax,TK_NE
    je .cc
    mov esi,09Ch
    cmp rax,'<'
    je .cc
    mov esi,09Fh
    cmp rax,'>'
    je .cc
    mov esi,09Eh
    cmp rax,TK_LE
    je .cc
    mov esi,09Dh
.cc:
    emitbytes 00Fh
    mov [rbp-64],sil
    lea rcx,[rbp-64]
    mov edx,1
    call code_bytes
    emitbytes 0C0h,048h,00Fh,0B6h,0C0h
    jmp .save
.unary:
    mov rcx,[rbx+16]
    call code_load
    cmp qword [rbx+24],'-'
    je .neg
    cmp qword [rbx+24],'+'
    je .save
    emitbytes 048h,085h,0C0h
    mov esi,094h
    cmp qword [rbx+24],'!'
    je .cc
    mov esi,095h
    jmp .cc
.neg:
    emitbytes 048h,0F7h,0D8h
    jmp .save
.native:
    mov rcx,[rbx+24]
    test rcx,rcx
    jz .native2
    call code_load
    emitbytes 048h,089h,0C1h
.native2:
    mov rcx,[rbx+32]
    test rcx,rcx
    jz .native3
    call code_load
    emitbytes 048h,089h,0C2h
.native3:
    mov rcx,[rbx+40]
    test rcx,rcx
    jz .nativecall
    call code_load
    emitbytes 049h,089h,0C0h
.nativecall:
    mov rcx,[rbx+16]
    call code_native
    jmp .save
.call:
    mov rcx,[rbx+24]
    mov rdx,[rbx+32]
    call code_arguments
    mov rcx,[rbx+16]
    call code_function_address
    emitbytes 0FFh,0D0h
    jmp .save
.label:
    mov rcx,[rbx+16]
    mov rax,[g_labels]
    mov rdx,[g_codesize]
    mov [rax+rcx*8],rdx
    jmp .done
.jump:
    mov ecx,0E9h
    mov rdx,[rbx+16]
    call code_jump
    jmp .done
.branch:
    mov rcx,[rbx+16]
    call code_load
    emitbytes 048h,085h,0C0h
    mov ecx,084h
    cmp qword [rbx+32],0
    je .branchcode
    inc ecx
.branchcode:
    mov rdx,[rbx+24]
    call code_jump
    jmp .done
.return:
    mov rcx,[rbx+16]
    call code_load
    call code_epilog
    jmp .done
.get:
    mov rcx,[rbx+16]
    call code_load
    emitbytes 048h,08Bh,080h
    mov rcx,[rbx+24]
    call code_u32
    jmp .save
.set:
    mov rcx,[rbx+24]
    call code_load
    emitbytes 049h,089h,0C2h
    mov rcx,[rbx+16]
    call code_load
    emitbytes 04Ch,089h,090h
    mov rcx,[rbx+32]
    call code_u32
    jmp .done
.check:
    mov rcx,RT_ERROR_GET
    call code_native
    emitbytes 048h,085h,0C0h
    mov ecx,085h
    mov rdx,[rbx+16]
    call code_jump
    jmp .done
.alloc:
    mov rax,[rbx+16]
    mov rcx,[rax+24]
    test rcx,rcx
    jnz .allocsize
    mov ecx,8
.allocsize:
    call code_imm
    emitbytes 048h,089h,0C1h
    mov rcx,RT_ALLOC
    call code_native
    jmp .save
.address:
    mov rcx,[rbx+16]
    call code_function_address
    jmp .save
.array:
    mov rsi,[rbx+24]
    lea rcx,[rsi*8+8]
    call code_imm
    emitbytes 048h,089h,0C1h
    mov rcx,RT_ALLOC
    call code_native
    mov rcx,[rbx+8]
    call code_save
    xor edi,edi
.arrayloop:
    cmp rdi,rsi
    jae .done
    mov rax,[rbx+16]
    mov rcx,[rax+rdi*8]
    call code_load
    emitbytes 049h,089h,0C2h
    mov rcx,[rbx+8]
    call code_load
    emitbytes 04Ch,089h,090h
    lea rcx,[rdi*8]
    call code_u32
    inc rdi
    jmp .arrayloop
.save:
    mov rcx,[rbx+8]
    call code_save
.done:
endpx

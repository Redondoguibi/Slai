; PE32+ image writer. Raw sections are compact; virtual ranges are independent.
procx make_image
    mov ecx,1048576
    call alloc
    mov [g_constants],rax
    mov rsi,[g_functions]
    mov edi,runtime_size
.encode:
    test rsi,rsi
    jz .fixups
    add rdi,15
    and rdi,-16
    mov rax,TEXT_BASE
    add rax,rdi
    mov [rsi+F_RVA],rax
    mov rcx,rsi
    call encode_function
    add rdi,[rsi+F_SIZE]
    cmp rdi,IMPORT_BASE-TEXT_BASE
    jae .large
    mov rsi,[rsi+F_NEXT]
    jmp .encode
.fixups:
    mov [g_textsize],rdi
    mov rsi,[g_fixups]
.fix:
    test rsi,rsi
    jz .layout
    mov rax,[rsi+8]
    mov rax,[rax+F_RVA]
    mov rcx,[rsi]
    mov [rcx],rax
    mov rsi,[rsi+16]
    jmp .fix
.layout:
    call build_unwind
    ; Section raw lengths (file alignment 512).
    lea rax,[rdi+511]
    and rax,-512
    mov [g_textraw],rax
    add rax,1024
    mov [g_importraw],rax
    add rax,(imports_size+511) and -512
    mov [g_dataraw],rax
    mov rcx,[g_globalcount]
    lea rcx,[rcx*8+16]
    mov [g_datasize],rcx
    add rcx,511
    and rcx,-512
    add rax,rcx
    mov [g_constraw],rax
    mov rcx,[g_constsize]
    inc rcx
    add rcx,511
    and rcx,-512
    add rcx,rax
    mov [g_pdataraw],rcx
    mov rax,[g_pdatasize]
    add rax,511
    and rax,-512
    add rcx,rax
    mov [g_imagesize],rcx
    call alloc
    mov rbx,rax
    mov [g_image],rax
    mov rcx,rax
    lea rdx,[pe_template]
    mov r8d,pe_template_size
    call [memcpy]
    mov eax,dword [g_textraw]
    mov [rbx+09Ch],eax
    mov eax,dword [g_pdatasize]
    add eax,PDATA_BASE-IMAGE_BASE+4095
    and eax,-4096
    mov [rbx+0D0h],eax
    mov eax,dword [g_pdatatablesize]
    mov [rbx+0124h],eax
    lea rsi,[rbx+0188h]
    mov eax,dword [g_textsize]
    mov dword [rsi+8],IMPORT_BASE-TEXT_BASE
    mov eax,dword [g_textraw]
    mov [rsi+16],eax
    mov eax,dword [g_importraw]
    mov [rsi+40+20],eax
    mov eax,dword [g_datasize]
    mov dword [rsi+80+8],CONST_BASE-DATA_BASE
    add eax,511
    and eax,-512
    mov [rsi+80+16],eax
    mov eax,dword [g_dataraw]
    mov [rsi+80+20],eax
    mov eax,dword [g_constsize]
    inc eax
    mov dword [rsi+120+8],PDATA_BASE-CONST_BASE
    add eax,511
    and eax,-512
    mov [rsi+120+16],eax
    mov eax,dword [g_constraw]
    mov [rsi+120+20],eax
    mov eax,dword [g_pdatasize]
    mov [rsi+160+8],eax
    add eax,511
    and eax,-512
    mov [rsi+160+16],eax
    mov eax,dword [g_pdataraw]
    mov [rsi+160+20],eax
    lea rcx,[rbx+1024]
    lea rdx,[runtime_blob]
    mov r8d,runtime_size
    call [memcpy]
    mov rsi,[g_functions]
.copyfunctions:
    test rsi,rsi
    jz .sections
    mov rcx,[rsi+F_RVA]
    mov rax,TEXT_BASE
    sub rcx,rax
    lea rcx,[rcx+rbx+1024]
    mov rdx,[rsi+F_CODE]
    mov r8,[rsi+F_SIZE]
    call [memcpy]
    mov rsi,[rsi+F_NEXT]
    jmp .copyfunctions
.sections:
    mov rcx,[g_importraw]
    add rcx,rbx
    lea rdx,[imports_blob]
    mov r8d,imports_size
    call [memcpy]
    mov rcx,[g_dataraw]
    add rcx,rbx
    mov rax,[g_main]
    mov rax,[rax+F_RVA]
    mov [rcx],rax
    mov rcx,[g_constraw]
    add rcx,rbx
    mov rdx,[g_constants]
    mov r8,[g_constsize]
    call [memcpy]
    mov rcx,[g_pdataraw]
    add rcx,rbx
    mov rdx,[g_pdata]
    mov r8,[g_pdatasize]
    call [memcpy]
    mov rax,rbx
endpx
.large:
    lea rcx,[err_limit]
    call fatal
procx write_image
    mov rcx,[g_output]
    call fullpath
    mov rbx,rax
    cmp qword [g_command],1
    jne .protectbegin
    mov rcx,rbx
    mov edx,'.'
    call [strrchr]
    test rax,rax
    jz .bad
    mov rcx,rax
    lea rdx,[suffix_exe]
    call [stricmp]
    test eax,eax
    jnz .bad
.protectbegin:
    mov rsi,[g_modules]
.protect:
    test rsi,rsi
    jz .stage
    mov rcx,rbx
    mov rdx,[rsi]
    call [stricmp]
    test eax,eax
    jz .bad
    mov rsi,[rsi+32]
    jmp .protect
.stage:
    mov rcx,rbx
    call utf16
    mov rbx,rax
    mov rcx,rax
    mov rdx,[g_image]
    mov r8,[g_imagesize]
    call rt_stage_file
    test rax,rax
    jz .bad
    mov rsi,rax
    mov rcx,rax
    mov rdx,rbx
    mov r8d,9
    call [MoveFileExW]
    test eax,eax
    jnz .done
    mov rcx,rsi
    call [DeleteFileW]
    jmp .bad
.done:
    mov rcx,rsi
    call [free]
endpx
.bad:
    lea rcx,[err_write]
    call fatal
procx build_unwind
    mov rax,[g_functioncount]
    imul rax,12
    lea rcx,[rax+RT_PDATA_SIZE]
    mov [g_pdatatablesize],rcx
    add rcx,rax
    mov [g_pdatasize],rcx
    call alloc
    mov [g_pdata],rax
    mov rbx,rax
    mov rcx,rax
    lea rdx,[runtime_blob+RT_PDATA-TEXT_BASE]
    mov r8,RT_PDATA_SIZE
    call [memcpy]
    lea rdi,[rbx+RT_PDATA_SIZE]
    mov r12,[g_pdatatablesize]
    add r12,rbx
    mov rsi,[g_functions]
.loop:
    test rsi,rsi
    jz .done
    mov rax,[rsi+F_RVA]
    mov rdx,IMAGE_BASE
    sub rax,rdx
    mov [rdi],eax
    add rax,[rsi+F_SIZE]
    mov [rdi+4],eax
    mov rax,r12
    sub rax,rbx
    add eax,PDATA_BASE-IMAGE_BASE
    mov [rdi+8],eax
    ; push rbp (1 byte), mov rbp,rsp (3), sub rsp,imm32 (7).
    ; Fixed allocation: frame register is intentionally unused by the unwinder.
    mov dword [r12],00030B01h
    mov word [r12+4],010Bh
    mov rax,[rsi+F_FRAME]
    shr rax,3
    mov [r12+6],ax
    mov word [r12+8],5001h
    mov word [r12+10],0
    add rdi,12
    add r12,12
    mov rsi,[rsi+F_NEXT]
    jmp .loop
.done:
endpx


procx start
    mov ecx,65001
    call [SetConsoleOutputCP]
    call [GetCommandLineW]
    mov rcx,rax
    lea rdx,[g_argc]
    call [CommandLineToArgvW]
    mov rbx,rax
    cmp dword [g_argc],2
    jb .usage
    mov rcx,[rbx+8]
    call utf8
    mov rsi,rax
    mov rcx,rax
    lea rdx,[arg_version]
    call equal
    test eax,eax
    jnz .version
    mov rcx,rsi
    lea rdx,[arg_build]
    call equal
    mov edi,1
    test eax,eax
    jnz .command
    mov rcx,rsi
    lea rdx,[arg_run]
    call equal
    mov edi,2
    test eax,eax
    jnz .command
    mov rcx,rsi
    lea rdx,[arg_check]
    call equal
    mov edi,3
    test eax,eax
    jnz .command
    mov rcx,rsi
    lea rdx,[arg_inspect]
    call equal
    mov edi,4
    test eax,eax
    jnz .command
    mov rcx,rsi
    lea rdx,[arg_sir]
    call equal
    mov edi,5
    test eax,eax
    jnz .command
    mov rcx,rsi
    lea rdx,[kw_ia]
    call equal
    mov edi,6
    test eax,eax
    jnz .command
    ; A source path alone defaults to compile-and-run.
    cmp byte [rsi],'-'
    je .usage
    mov rcx,rsi
    mov edx,'.'
    call [strrchr]
    test rax,rax
    jz .usage
    mov rcx,rax
    lea rdx,[suffix_slai]
    call [stricmp]
    test eax,eax
    jnz .usage
    mov qword [g_command],2
    mov rcx,rsi
    call fullpath
    mov [g_input],rax
    mov [g_file],rax
    mov edi,2
    jmp .options
.command:
    mov [g_command],rdi
    cmp dword [g_argc],3
    jb .usage
    mov rcx,[rbx+16]
    call utf8
    mov rcx,rax
    call fullpath
    mov [g_input],rax
    mov [g_file],rax
    mov edi,3
    cmp qword [g_command],6
    jne .options
    cmp dword [g_argc],4
    jb .usage
    mov rcx,[rbx+24]
    call utf8
    mov [g_clioperation],rax
    mov edi,4
.options:
    cmp edi,[g_argc]
    jae .compile
    mov rcx,[rbx+rdi*8]
    call utf8
    mov rsi,rax
    mov rcx,rax
    lea rdx,[arg_json]
    call equal
    test eax,eax
    jnz .json
    mov rcx,rsi
    lea rdx,[arg_release]
    call equal
    test eax,eax
    jnz .release
    mov rcx,rsi
    lea rdx,[arg_apply]
    call equal
    test eax,eax
    jnz .applyoption
    mov rcx,rsi
    lea rdx,[arg_type]
    call equal
    test eax,eax
    jnz .typeoption
    mov rcx,rsi
    lea rdx,[arg_output]
    call equal
    test eax,eax
    jz .positional
    inc edi
    cmp edi,[g_argc]
    jae .usage
    mov rcx,[rbx+rdi*8]
    call utf8
    mov [g_output],rax
    jmp .next
.positional:
    cmp qword [g_command],6
    jne .usage
    cmp byte [rsi],'-'
    je .usage
    cmp qword [g_clitarget],0
    jne .newname
    mov [g_clitarget],rsi
    jmp .next
.newname:
    cmp qword [g_cliname],0
    jne .usage
    mov [g_cliname],rsi
    jmp .next
.applyoption:
    cmp qword [g_command],6
    jne .usage
    mov qword [g_cliapply],1
    jmp .next
.typeoption:
    cmp qword [g_command],6
    jne .usage
    inc edi
    cmp edi,[g_argc]
    jae .usage
    mov rcx,[rbx+rdi*8]
    call utf8
    mov [g_clitype],rax
    jmp .next
.json:
    mov qword [g_json],1
    jmp .next
.release:
    mov qword [g_release],1
.next:
    inc edi
    jmp .options
.compile:
    mov rcx,[g_input]
    call load_module
    mov [g_program],rax
    mov rcx,rax
    call collect_definitions
    call validate_names
    xor ecx,ecx
    call new_function
    mov [g_main],rax
    mov [g_ctx],rax
    mov qword [rax+F_MAIN],1
    mov qword [rax+F_TYPE],T_VOID
    mov rcx,[g_program]
    xor edx,edx
    call compile_block
    mov ecx,T_VOID
    xor edx,edx
    call constant
    mov r8,rax
    mov ecx,SI_RETURN
    xor edx,edx
    xor r9d,r9d
    call sir
    call build_index
    call resolve_ia
    call optimize_program
    cmp qword [g_command],6
    je .cliia
    call make_image
    mov qword [g_tok],0
    cmp qword [g_command],3
    je .checked
    cmp qword [g_command],4
    je .inspect
    cmp qword [g_command],5
    je .sir
    cmp qword [g_command],2
    je .temp
    cmp qword [g_output],0
    jne .write
    mov rcx,[g_input]
    call [strlen]
    lea rcx,[rax+8]
    call alloc
    mov rsi,rax
    mov rcx,rax
    mov rdx,[g_input]
    call [strcpy]
    mov rcx,rsi
    mov edx,'.'
    call [strrchr]
    test rax,rax
    jz .appendexe
    ; Input must end in .slai to replace the suffix safely.
    mov rcx,rax
    lea rdx,[suffix_slai]
    mov rdi,rax
    call equal
    test eax,eax
    jz .appendexe
    mov byte [rdi],0
.appendexe:
    mov rcx,rsi
    lea rdx,[suffix_exe]
    call [strcat]
    mov [g_output],rsi
    jmp .write
.temp:
    mov ecx,65536
    call alloc
    mov rsi,rax
    mov ecx,32768
    mov rdx,rsi
    call [GetTempPathW]
    test eax,eax
    jz .runerror
    cmp eax,32768
    jae .runerror
    mov ecx,65536
    call alloc
    mov rdi,rax
    mov rcx,rsi
    lea rdx,[temp_prefix]
    xor r8d,r8d
    mov r9,rdi
    call [GetTempFileNameW]
    test eax,eax
    jz .runerror
    mov [g_temp],rdi
    mov rcx,rdi
    call utf8
    mov [g_output],rax
.write:
    call write_image
    cmp qword [g_command],2
    je .run
    mov rdx,[g_output]
    lea rcx,[fmt_built]
    cmp qword [g_json],0
    je .print
    lea rcx,[json_ok]
    jmp .print
.checked:
    lea rcx,[check_ok]
    cmp qword [g_json],0
    je .print
    lea rcx,[json_ok]
    jmp .print
.inspect:
    lea rcx,[arg_inspect]
    xor edx,edx
    call query_index
    mov rdx,rax
    lea rcx,[fmt_text]
    jmp .print
.cliia:
    mov rcx,[g_clioperation]
    lea rdx,[kw_rename]
    call equal
    test eax,eax
    jnz .rename
    mov rcx,[g_clioperation]
    lea rdx,[kw_edit]
    call equal
    test eax,eax
    jnz .edit
    cmp qword [g_cliapply],0
    jne .usage
    mov rcx,[g_clioperation]
    mov rdx,[g_clitarget]
    call query_index
    jmp .iaprint
.rename:
    cmp qword [g_clitarget],0
    je .usage
    cmp qword [g_cliname],0
    je .usage
    mov rcx,[g_clitarget]
    mov rdx,[g_cliname]
    call plan_rename
    jmp .change
.edit:
    cmp qword [g_clitarget],0
    je .usage
    cmp qword [g_cliname],0
    je .usage
    mov rcx,[g_clitarget]
    mov rdx,[g_cliname]
    mov r8,[g_clitype]
    call plan_edit
.change:
    mov rsi,rax
    cmp qword [g_cliapply],0
    jne .applychange
    cmp qword [g_json],0
    jne .planjson
    mov rax,[rsi]
    jmp .iaprint
.planjson:
    mov rcx,rsi
    call plan_json
    jmp .iaprint
.applychange:
    mov rcx,rsi
    mov edx,1
    call encode_plan
    mov rcx,rax
    call rt_apply
    test eax,eax
    jz .applyfailed
    lea rax,[json_ok]
.iaprint:
    mov rdx,rax
    lea rcx,[fmt_text]
    jmp .print
.applyfailed:
    mov rcx,[g_applyerror]
    call fatal
.sir:
    call json_sir
    mov rdx,rax
    lea rcx,[fmt_text]
    jmp .print
.run:
    mov dword [g_si],104
    mov rcx,[g_output]
    call utf16
    mov rcx,rax
    xor edx,edx
    xor r8d,r8d
    xor r9d,r9d
    mov qword [rsp+32],1
    mov qword [rsp+40],0
    mov qword [rsp+48],0
    mov qword [rsp+56],0
    lea rax,[g_si]
    mov [rsp+64],rax
    lea rax,[g_pi]
    mov [rsp+72],rax
    call [CreateProcessW]
    test eax,eax
    jz .runerror
    mov rcx,qword [g_pi]
    mov edx,-1
    call [WaitForSingleObject]
    mov rcx,qword [g_pi]
    lea rdx,[g_exitcode]
    call [GetExitCodeProcess]
    mov rcx,qword [g_pi]
    call [CloseHandle]
    mov rcx,qword [g_pi+8]
    call [CloseHandle]
    mov rcx,[g_temp]
    call [DeleteFileW]
    mov ecx,[g_exitcode]
    call [ExitProcess]
.version:
    lea rcx,[version_text]
.print:
    call [printf]
    xor ecx,ecx
    call [ExitProcess]
.usage:
    lea rcx,[usage_text]
    call [printf]
    mov ecx,2
    call [ExitProcess]
.runerror:
    lea rcx,[err_run]
    call fatal
endpx






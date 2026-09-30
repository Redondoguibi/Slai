; Native name resolution, typed SIR and function specialization.
procx load_module
    call fullpath
    mov rbx,rax
    mov rsi,[g_modules]
.find:
    test rsi,rsi
    jz .new
    mov rcx,rbx
    mov rdx,[rsi]
    call [stricmp]
    test eax,eax
    jz .exists
    mov rsi,[rsi+32]
    jmp .find
.exists:
    cmp qword [rsi+24],1
    je .cycle
    xor eax,eax
    jmp .done
.new:
    mov r12,[g_tok]
    mov r13,[g_file]
    mov ecx,48
    call alloc
    mov rsi,rax
    mov [rsi],rbx
    mov qword [rsi+24],1
    mov rax,[g_modules]
    mov [rsi+32],rax
    mov [g_modules],rsi
    mov [g_file],rbx
    mov qword [g_tok],0
    mov rcx,rbx
    call readfile
    mov [rsi+8],rax
    mov rcx,rax
    call lex
    mov [g_tok],rax
    call parse_sequence
    mov [rsi+16],rax
    mov rdi,rax
.imports:
    test rdi,rdi
    jz .loaded
    cmp qword [rdi+NK],N_IMPORT
    jne .next
    mov ecx,8192
    call alloc
    mov r14,rax
    mov rcx,rax
    mov rdx,rbx
    call [strcpy]
    mov rcx,r14
    mov edx,92
    call [strrchr]
    test rax,rax
    jz .relative
    mov byte [rax+1],0
.relative:
    mov rcx,r14
    mov rdx,[rdi+NV]
    call [strcat]
    mov rcx,r14
    call load_module
    mov [rdi+NA],rax
.next:
    mov rdi,[rdi+NN]
    jmp .imports
.loaded:
    mov qword [rsi+24],2
    mov [g_tok],r12
    mov [g_file],r13
    mov rax,[rsi+16]
.done:
endpx
.cycle:
    lea rcx,[err_cycle]
    call fatal

procx find_definition
    mov rbx,rcx
    mov rsi,[g_defs]
.loop:
    test rsi,rsi
    jz .none
    mov rax,[rsi]
    mov rcx,[rax+NV]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .found
    mov rsi,[rsi+8]
    jmp .loop
.found:
    mov rax,[rsi]
    jmp .done
.none:
    xor eax,eax
.done:
endpx
procx collect_definitions
    mov rbx,rcx
.loop:
    test rbx,rbx
    jz .done
    mov rax,[rbx+NT]
    mov [g_tok],rax
    cmp qword [rbx+NK],N_IMPORT
    je .import
    cmp qword [rbx+NK],N_FUNC
    je .add
    cmp qword [rbx+NK],N_CLASS
    jne .next
.add:
    mov rcx,[rbx+NV]
    call find_definition
    test rax,rax
    jnz .duplicate
    mov ecx,16
    call alloc
    mov [rax],rbx
    mov rcx,[g_defs]
    mov [rax+8],rcx
    mov [g_defs],rax
    jmp .next
.import:
    mov rcx,[rbx+NA]
    call collect_definitions
.next:
    mov rbx,[rbx+NN]
    jmp .loop
.done:
endpx
.duplicate:
    lea rcx,[err_duplicate]
    call fatal

procx new_function
    mov rbx,rcx
    inc qword [g_functioncount]
    cmp qword [g_functioncount],256
    jbe .allocate
    lea rcx,[err_limit]
    call fatal
.allocate:
    mov ecx,F_SIZEOF
    call alloc
    mov [rax+F_AST],rbx
    mov rcx,[g_lastfunction]
    test rcx,rcx
    jnz .tail
    mov [g_functions],rax
    jmp .done
.tail:
    mov [rcx+F_NEXT],rax
.done:
    mov [g_lastfunction],rax
endpx
procx value_new
    mov rbx,rcx
    mov ecx,24
    call alloc
    mov [rax],rbx
    mov rcx,[g_ctx]
    inc qword [rcx+F_SLOTS]
    mov rdx,[rcx+F_SLOTS]
    cmp rdx,400
    ja .limit
    mov [rax+8],rdx
endpx
.limit:
    lea rcx,[err_limit]
    call fatal
procx sir
    mov [rbp-64],rcx
    mov [rbp-72],rdx
    mov [rbp-80],r8
    mov [rbp-88],r9
    mov ecx,64
    call alloc
    mov rcx,[rbp-64]
    mov [rax],rcx
    mov rcx,[rbp-72]
    mov [rax+8],rcx
    mov rcx,[rbp-80]
    mov [rax+16],rcx
    mov rcx,[rbp-88]
    mov [rax+24],rcx
    mov rcx,[g_ctx]
    mov rdx,[rcx+F_TAIL]
    test rdx,rdx
    jnz .tail
    mov [rcx+F_HEAD],rax
    jmp .end
.tail:
    mov [rdx+48],rax
.end:
    mov [rcx+F_TAIL],rax
endpx
procx constant
    mov rbx,rdx
    call value_new
    mov rsi,rax
    mov ecx,SI_CONST
    mov rdx,rax
    mov r8,rbx
    xor r9d,r9d
    call sir
    mov rax,rsi
endpx
procx new_label
    inc qword [g_label]
    mov rax,[g_label]
endpx
procx put_label
    mov r8,rcx
    mov ecx,SI_LABEL
    xor edx,edx
    xor r9d,r9d
    call sir
endpx
procx put_jump
    mov r8,rcx
    mov ecx,SI_JUMP
    xor edx,edx
    xor r9d,r9d
    call sir
endpx
procx error_check
    mov rax,[g_ctx]
    mov r8,[rax+F_CATCH]
    mov ecx,SI_ERRORCHECK
    xor edx,edx
    xor r9d,r9d
    call sir
endpx
procx find_variable
    mov rbx,rcx
    mov rax,[g_ctx]
    mov rsi,[rax+F_LOCALS]
    xor edi,edi
.loop:
    test rsi,rsi
    jz .globals
    mov rcx,[rsi]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .found
    mov rsi,[rsi+24]
    jmp .loop
.globals:
    test rdi,rdi
    jnz .none
    inc rdi
    mov rsi,[g_globals]
    jmp .loop
.found:
    mov rax,rsi
    jmp .done
.none:
    xor eax,eax
.done:
endpx
procx bind_variable
    ; name, type, source token -> variable; never silently changes types.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    call find_definition
    test rax,rax
    jnz .bad
    mov ecx,48
    call alloc
    mov r12,rax
    mov [rax],rbx
    mov [rax+8],rsi
    mov [rax+32],rdi
    mov rcx,rsi
    call value_new
    mov [r12+16],rax
    mov rcx,[g_ctx]
    cmp qword [rcx+F_MAIN],0
    je .local
    inc qword [g_globalcount]
    mov rdx,[g_globalcount]
    neg rdx
    mov [rax+8],rdx
    mov [g_globals],r12
.local:
    mov rdx,[rcx+F_LOCALS]
    mov [r12+24],rdx
    mov [rcx+F_LOCALS],r12
    mov rax,r12
endpx
.bad:
    lea rcx,[err_duplicate]
    call fatal
procx require_type
    cmp [rcx],rdx
    jne .bad
    mov rax,rcx
endpx
.bad:
    lea rcx,[err_type]
    call fatal
procx annotation_type
    mov rbx,rcx
    lea rdx,[kw_int]
    call equal
    test eax,eax
    jnz .int
    mov rcx,rbx
    lea rdx,[kw_string]
    call equal
    test eax,eax
    jnz .string
    mov rcx,rbx
    lea rdx,[kw_bool]
    call equal
    test eax,eax
    jnz .bool
    lea rcx,[err_type]
    call fatal
.int:
    mov eax,T_INT
    jmp .done
.string:
    mov eax,T_STRING
    jmp .done
.bool:
    mov eax,T_BOOL
.done:
endpx
procx composite_type
    ; kind, element/class -> canonical list/task; records created separately.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,[g_types]
.find:
    test rdi,rdi
    jz .new
    cmp [rdi],rbx
    jne .next
    cmp [rdi+8],rsi
    je .found
.next:
    mov rdi,[rdi+32]
    jmp .find
.new:
    mov ecx,64
    call alloc
    mov rdi,rax
    mov [rax],rbx
    mov [rax+8],rsi
    mov rax,[g_types]
    mov [rdi+32],rax
    mov [g_types],rdi
.found:
    mov rax,rdi
endpx
procx find_field
    ; type, field-name -> field or null.
    mov rbx,rdx
    cmp rcx,4096
    jb .none
    mov rsi,[rcx+16]
.loop:
    test rsi,rsi
    jz .none
    mov rcx,[rsi]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .found
    mov rsi,[rsi+24]
    jmp .loop
.found:
    mov rax,rsi
    jmp .done
.none:
    xor eax,eax
.done:
endpx
procx add_field
    ; type,name,value-type,token. Appending gives stable sequential offsets.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    mov r12,r9
    mov ecx,48
    call alloc
    mov [rax],rsi
    mov [rax+8],rdi
    mov [rax+32],r12
    mov rcx,[rbx+24]
    mov [rax+16],rcx
    add qword [rbx+24],8
    mov rcx,[rbx+16]
    mov [rax+24],rcx
    mov [rbx+16],rax
endpx
procx native_call
    ; runtime address, result type, first Value, second Value -> result Value.
    mov rbx,rcx
    mov rsi,r8
    mov rdi,r9
    mov rcx,rdx
    call value_new
    mov r12,rax
    mov ecx,SI_NATIVE
    mov rdx,rax
    mov r8,rbx
    mov r9,rsi
    call sir
    mov [rax+32],rdi
    mov rax,r12
endpx

include 'expressions.inc'
include 'statements.inc'
include 'functions.inc'
include 'names.inc'
include 'index.inc'
include 'ia.inc'
include 'changes.inc'

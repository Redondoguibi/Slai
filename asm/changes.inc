; Source plans retain exact original bytes. Compilation only builds a plan;
; mutation occurs exclusively in explicit CLI --apply or generated sys.ia.apply.
procx valid_identifier
    mov rbx,rcx
    mov rsi,rcx
    xor edi,edi
.loop:
    movzx eax,byte [rsi]
    test eax,eax
    jz .keywords
    cmp al,'_'
    je .next
    cmp al,'A'
    jb .digit
    cmp al,'Z'
    jbe .next
    cmp al,'a'
    jb .bad
    cmp al,'z'
    jbe .next
    cmp al,128
    jae .next
    jmp .bad
.digit:
    test edi,edi
    jz .bad
    cmp al,'0'
    jb .bad
    cmp al,'9'
    ja .bad
.next:
    inc edi
    inc rsi
    jmp .loop
.keywords:
    test edi,edi
    jz .bad
    lea rsi,[reserved_words]
.word:
    mov rdx,[rsi]
    test rdx,rdx
    jz .done
    mov rcx,rbx
    call equal
    test eax,eax
    jnz .bad
    add rsi,8
    jmp .word
.done:
endpx
.bad:
    lea rcx,[err_identifier]
    call fatal
procx plan_token_edit
    ; token, replacement, original-length -> insert into a sorted edit list.
    mov rbx,rdx
    mov rsi,r8
    call token_offset
    mov rdi,rdx
    mov r12,rax
    mov ecx,40
    call alloc
    mov [rax],rdi
    mov [rax+8],r12
    add r12,rsi
    mov [rax+16],r12
    mov [rax+24],rbx
    mov rcx,[g_edits]
    mov [rax+32],rcx
    mov [g_edits],rax
endpx
procx plan_rename
    mov rbx,rdx
    call resolve_symbol
    mov rsi,rax
    mov rcx,rbx
    call valid_identifier
    mov rcx,[rsi+S_NAME]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jz .collisions
    cmp qword [rsi+S_OWNER],0
    jne .bad
.collisions:
    mov rdi,[g_symbols]
.collision:
    test rdi,rdi
    jz .ambiguities
    cmp rdi,rsi
    je .next
    mov rcx,[rdi+S_NAME]
    mov rdx,rbx
    call equal
    test eax,eax
    jz .next
    mov rax,[rsi+S_OWNER]
    test rax,rax
    jz .bad
    cmp [rdi+S_OWNER],rax
    je .bad
    mov rcx,[rsi+S_KIND]
    lea rdx,[kind_field]
    call equal
    test eax,eax
    jnz .next
    mov rcx,[rsi+S_KIND]
    lea rdx,[kind_method]
    call equal
    test eax,eax
    jz .bad
.next:
    mov rdi,[rdi+S_NEXT]
    jmp .collision
.ambiguities:
    mov rcx,[rsi+S_KIND]
    lea rdx,[kind_field]
    call equal
    test eax,eax
    jnz .checkambiguous
    mov rcx,[rsi+S_KIND]
    lea rdx,[kind_method]
    call equal
    test eax,eax
    jz .edits
.checkambiguous:
    mov rdi,[g_ambiguous]
.ambiguous:
    test rdi,rdi
    jz .edits
    mov rax,[rdi]
    mov rcx,[rax+NV]
    mov rdx,[rsi+S_NAME]
    call equal
    test eax,eax
    jnz .ambiguouserror
    mov rdi,[rdi+8]
    jmp .ambiguous
.edits:
    mov qword [g_edits],0
    mov rcx,[rsi+S_NAME]
    call [strlen]
    mov r12,rax
    mov rcx,[rsi+S_TOKEN]
    mov rdx,rbx
    mov r8,r12
    call plan_token_edit
    mov rdi,[rsi+S_REFS]
.refs:
    test rdi,rdi
    jz .materialize
    mov rcx,[rdi]
    mov rdx,rbx
    mov r8,r12
    call plan_token_edit
    mov rdi,[rdi+16]
    jmp .refs
.materialize:
    call materialize_plan
endpx
.bad:
    lea rcx,[err_duplicate]
    call fatal
.ambiguouserror:
    lea rcx,[err_ambiguous]
    call fatal
procx closing_paren
    ; Starting token -> module in RDX and byte offset of balanced closing paren.
    call token_offset
    mov rbx,rdx
    mov rsi,[rbx+8]
    add rsi,rax
.find:
    mov al,[rsi]
    test al,al
    jz .bad
    cmp al,'('
    je .start
    inc rsi
    jmp .find
.start:
    mov edi,1
    inc rsi
.loop:
    mov al,[rsi]
    test al,al
    jz .bad
    cmp al,34
    je .string
    cmp al,39
    je .string
    cmp al,'?'
    je .comment
    cmp al,'('
    je .open
    cmp al,')'
    je .close
    inc rsi
    jmp .loop
.open:
    inc edi
    inc rsi
    jmp .loop
.close:
    dec edi
    jz .done
    inc rsi
    jmp .loop
.string:
    mov dl,al
.str:
    inc rsi
    mov al,[rsi]
    test al,al
    jz .bad
    cmp al,92
    jne .quote
    inc rsi
    jmp .str
.quote:
    cmp al,dl
    jne .str
    inc rsi
    jmp .loop
.comment:
    inc rsi
    cmp byte [rsi],0
    je .bad
    cmp byte [rsi],10
    jne .comment
    inc rsi
    jmp .loop
.done:
    mov rax,rsi
    sub rax,[rbx+8]
    mov rdx,rbx
endpx
.bad:
    lea rcx,[err_expected]
    call fatal
procx plan_insert
    ; token identifying a call/declaration, text to insert before closing ')'.
    mov rbx,rdx
    call closing_paren
    mov rsi,rax
    mov rdi,rdx
    mov ecx,40
    call alloc
    mov [rax],rdi
    mov [rax+8],rsi
    mov [rax+16],rsi
    mov [rax+24],rbx
    mov rcx,[g_edits]
    mov [rax+32],rcx
    mov [g_edits],rax
endpx
procx plan_edit
    ; target key, new parameter name, annotation string.
    mov rbx,rdx
    mov r12,r8
    call resolve_symbol
    mov rsi,rax
    mov rdi,[rax+S_DECL]
    cmp qword [rdi+NK],N_FUNC
    jne .bad
    mov rcx,rbx
    call valid_identifier
    mov rcx,r12
    call annotation_type
    mov r13,rax
    mov r14,[rdi+NB]
.params:
    test r14,r14
    jz .declaration
    mov rcx,[r14+NV]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .duplicate
    mov r14,[r14+NN]
    jmp .params
.declaration:
    mov qword [g_edits],0
    call text_begin
    cmp qword [rdi+NC],0
    je .paramname
    lea rcx,[comma_space]
    call text_append
.paramname:
    mov rcx,rbx
    call text_append
    lea rcx,[colon_space]
    call text_append
    mov rcx,r12
    call text_append
    mov rdx,[g_text]
    mov rcx,[rdi+NT]
    call plan_insert
    mov r14,[rsi+S_REFS]
    mov rcx,[rsi+S_NAME]
    lea rdx,[kw_args]
    call equal
    test eax,eax
    jz .calls
    mov rcx,[rsi+S_OWNER]
    call symbol_decl
    test rax,rax
    jz .bad
    mov r14,[rax+S_REFS]
.calls:
    test r14,r14
    jz .materialize
    mov r15,[r14+24]
    test r15,r15
    jz .bad
    mov rcx,r15
    call ia_operation
    test rax,rax
    jnz .nextcall
    mov rax,[r14+8]
    cmp [r15+NA],rax
    je .call
    ; A reference as first argument of sys.spawn is also a direct invocation.
    cmp [r15+NB],rax
    jne .bad
    mov rax,[r15+NA]
    cmp qword [rax+NK],N_MEMBER
    jne .bad
    mov rcx,[rax+NV]
    lea rdx,[kw_spawn]
    call equal
    test eax,eax
    jz .bad
.call:
    call text_begin
    cmp qword [r15+NC],0
    je .default
    lea rcx,[comma_space]
    call text_append
.default:
    lea rcx,[default_int]
    cmp r13,T_INT
    je .value
    lea rcx,[kw_false]
    cmp r13,T_BOOL
    je .value
    lea rcx,[default_string]
.value:
    call text_append
    mov rdx,[g_text]
    mov rcx,[r15+NA]
    mov rcx,[rcx+NT]
    call plan_insert
.nextcall:
    mov r14,[r14+16]
    jmp .calls
.materialize:
    call materialize_plan
endpx
.bad:
    lea rcx,[err_ambiguous]
    call fatal
.duplicate:
    lea rcx,[err_duplicate]
    call fatal
procx materialize_plan
    mov ecx,32
    call alloc
    mov r12,rax
    mov rsi,[g_modules]
.module:
    test rsi,rsi
    jz .preview
    mov rdi,[g_edits]
    xor ebx,ebx
.find:
    test rdi,rdi
    jz .nextmodule
    cmp [rdi],rsi
    je .edited
    mov rdi,[rdi+32]
    jmp .find
.edited:
    call text_begin
    xor r13d,r13d
.copy:
    ; Select the first not-yet-consumed edit in this file (bounded source size).
    mov rdi,[g_edits]
    xor r14d,r14d
    mov r15,-1
.select:
    test rdi,rdi
    jz .selected
    cmp [rdi],rsi
    jne .selectnext
    mov rax,[rdi+8]
    cmp rax,r13
    jb .selectnext
    cmp rax,r15
    jae .selectnext
    mov r15,rax
    mov r14,rdi
.selectnext:
    mov rdi,[rdi+32]
    jmp .select
.selected:
    test r14,r14
    jz .tail
    mov rcx,[rsi+8]
    add rcx,r13
    mov rdx,r15
    sub rdx,r13
    call text_bytes
    mov rcx,[r14+24]
    call text_append
    mov r13,[r14+16]
    ; Mark consumed by clearing module pointer. Duplicated zero-width edits
    ; are discarded, preserving one insertion per physical source position.
    mov rdi,[g_edits]
.consume:
    test rdi,rdi
    jz .copy
    cmp [rdi],rsi
    jne .consumenext
    cmp [rdi+8],r15
    jne .consumenext
    mov qword [rdi],0
.consumenext:
    mov rdi,[rdi+32]
    jmp .consume
.tail:
    mov rcx,[rsi+8]
    add rcx,r13
    call text_append
    mov rbx,[g_text]
    mov ecx,24
    call alloc
    mov [rax],rsi
    mov [rax+8],rbx
    mov rcx,[r12+8]
    mov [rax+16],rcx
    mov [r12+8],rax
    inc qword [r12+16]
.nextmodule:
    mov rsi,[rsi+32]
    jmp .module
.preview:
    call text_begin
    mov rsi,[r12+8]
.file:
    test rsi,rsi
    jz .done
    lea rcx,[preview_before]
    call text_append
    mov rax,[rsi]
    mov rcx,[rax]
    call text_append
    lea rcx,[newline_text]
    call text_append
    mov rax,[rsi]
    mov rcx,[rax+8]
    call text_append
    lea rcx,[preview_after]
    call text_append
    mov rax,[rsi]
    mov rcx,[rax]
    call text_append
    lea rcx,[newline_text]
    call text_append
    mov rcx,[rsi+8]
    call text_append
    mov rsi,[rsi+16]
    jmp .file
.done:
    mov rax,[g_text]
    mov [r12],rax
    mov rax,r12
endpx
procx add_blob
    mov rbx,rcx
    mov rsi,rdx
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
procx plan_address
    cmp qword [g_hostplan],0
    jne .host
    call add_blob
    jmp .done
.host:
    mov rax,rcx
.done:
endpx
procx plan_string
    mov rbx,rcx
    call [strlen]
    lea rdx,[rax+1]
    mov rcx,rbx
    call plan_address
endpx
procx encode_plan
    ; plan, host-pointer mode -> descriptor usable by shared Assembly apply.
    mov rbx,rcx
    mov [g_hostplan],rdx
    mov rax,[rbx+16]
    imul rcx,rax,40
    add rcx,16
    mov r15,rcx
    call alloc
    mov r12,rax
    mov rax,[rbx+16]
    mov [r12+8],rax
    mov rcx,[rbx]
    call plan_string
    mov [r12],rax
    lea rdi,[r12+16]
    mov rsi,[rbx+8]
.file:
    test rsi,rsi
    jz .done
    mov r13,[rsi]
    mov rcx,[r13]
    call utf16
    mov rbx,rax
    xor edx,edx
.wide:
    cmp word [rbx+rdx],0
    je .wideend
    add rdx,2
    jmp .wide
.wideend:
    add rdx,2
    mov rcx,rbx
    call plan_address
    mov [rdi],rax
    mov rcx,[r13+8]
    call [strlen]
    mov [rdi+16],rax
    mov rcx,[r13+8]
    call plan_string
    mov [rdi+8],rax
    mov rcx,[rsi+8]
    call [strlen]
    mov [rdi+32],rax
    mov rcx,[rsi+8]
    call plan_string
    mov [rdi+24],rax
    add rdi,40
    mov rsi,[rsi+16]
    jmp .file
.done:
    mov rcx,r12
    mov rdx,r15
    call plan_address
endpx
procx plan_json
    mov rbx,rcx
    call text_begin
    lea rcx,[json_plan]
    call text_append
    mov rsi,[rbx+8]
    xor edi,edi
.loop:
    test rsi,rsi
    jz .end
    test edi,edi
    jz .file
    lea rcx,[comma]
    call text_append
.file:
    lea rcx,[json_file]
    call text_append
    mov rax,[rsi]
    mov rcx,[rax]
    call json_string
    lea rcx,[json_before]
    call text_append
    mov rax,[rsi]
    mov rcx,[rax+8]
    call json_string
    lea rcx,[json_after]
    call text_append
    mov rcx,[rsi+8]
    call json_string
    lea rcx,[json_close]
    call text_append
    inc edi
    mov rsi,[rsi+16]
    jmp .loop
.end:
    lea rcx,[json_preview]
    call text_append
    mov rcx,[rbx]
    call json_string
    lea rcx,[json_close]
    call text_append
    mov rax,[g_text]
endpx

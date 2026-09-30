; Tokenization is entirely native. Token offsets retain byte locations in UTF-8.
procx lex_add
    mov rax,[g_lexcount]
    imul rax,48
    add rax,[g_lexbuf]
    mov [rax],rcx
    mov [rax+8],rdx
    mov [rax+16],r8
    mov r10,[g_line]
    mov [rax+24],r10
    mov r10,[g_col]
    mov [rax+32],r10
    mov r10,[g_file]
    mov [rax+40],r10
    inc qword [g_lexcount]
    mov [g_lastkind],rcx
endpx
procx lex
    mov rsi,rcx
    mov [g_source],rcx
    call [strlen]
    add rax,1024
    imul rcx,rax,48
    call alloc
    mov [g_lexbuf],rax
    mov qword [g_lexcount],0
    mov qword [g_line],1
    mov qword [g_col],1
    mov qword [g_indent],0
    mov qword [g_levels],0
    xor r15d,r15d
    mov r14d,1
    cmp byte [rsi],0EFh
    jne .loop
    cmp word [rsi+1],0BFBBh
    jne .loop
    add rsi,3
.loop:
    movzx eax,byte [rsi]
    test eax,eax
    jz .done
    test r14,r14
    jz .char
    test r15,r15
    jnz .char
    xor ebx,ebx
.spaces:
    cmp byte [rsi],32
    jne .indent
    inc rsi
    inc rbx
    jmp .spaces
.indent:
    lea rax,[rbx+1]
    mov [g_col],rax
    xor r14d,r14d
    cmp byte [rsi],9
    je .invalid
    cmp byte [rsi],13
    je .char
    cmp byte [rsi],10
    je .char
    cmp byte [rsi],'?'
    je .char
    cmp word [rsi],03F2Fh
    je .char
    cmp byte [rsi],0
    je .done
    mov rax,[g_indent]
    lea r10,[g_levels]
    cmp rbx,[r10+rax*8]
    je .char
    jb .dedents
    inc rax
    cmp rax,256
    jae .invalid
    mov [g_indent],rax
    mov [r10+rax*8],rbx
    mov ecx,TK_INDENT
    call lex_add
    jmp .char
.dedents:
    cmp rbx,[r10+rax*8]
    ja .invalid
    je .char
    dec qword [g_indent]
    mov ecx,TK_DEDENT
    call lex_add
    mov rax,[g_indent]
    lea r10,[g_levels]
    jmp .dedents
.char:
    movzx eax,byte [rsi]
    cmp eax,32
    je .skip
    cmp eax,9
    je .skip
    cmp eax,13
    je .skip
    cmp eax,10
    je .newline
    cmp word [rsi],03F2Fh
    je .blockcomment
    cmp eax,'?'
    je .question
    cmp eax,'"'
    je .string
    cmp eax,39
    je .string
    cmp eax,'0'
    jb .operator
    cmp eax,'9'
    jbe .number
    cmp eax,'A'
    jb .operator
    cmp eax,'Z'
    jbe .name
    cmp eax,'_'
    je .name
    cmp eax,'a'
    jb .operator
    cmp eax,'z'
    jbe .name
    cmp eax,128
    jae .name
    jmp .operator
.skip:
    inc rsi
    inc qword [g_col]
    jmp .loop
.newline:
    test r15,r15
    jnz .nextline
    mov rax,[g_lastkind]
    cmp rax,TK_NEWLINE
    je .nextline
    cmp rax,TK_INDENT
    je .nextline
    cmp rax,TK_DEDENT
    je .nextline
    test rax,rax
    jz .nextline
    mov ecx,TK_NEWLINE
    call lex_add
.nextline:
    inc rsi
    inc qword [g_line]
    mov qword [g_col],1
    mov r14d,1
    jmp .loop
.blockcomment:
    add rsi,2
.bc:
    cmp byte [rsi],0
    je .invalid
    cmp word [rsi],02F3Fh
    je .bcend
    cmp byte [rsi],10
    jne .bcnext
    inc qword [g_line]
    mov qword [g_col],0
.bcnext:
    inc qword [g_col]
    inc rsi
    jmp .bc
.bcend:
    add rsi,2
    add qword [g_col],2
    jmp .loop
.question:
    cmp byte [rsi+1],'!'
    jne .propagate
    mov ecx,TK_CATCH
    call lex_add
    add rsi,2
    add qword [g_col],2
    jmp .loop
.propagate:
    cmp rsi,[g_source]
    je .comment
    cmp byte [rsi-1],32
    jbe .comment
    mov ecx,'?'
    call lex_add
    jmp .skip
.comment:
    cmp byte [rsi],0
    je .done
    cmp byte [rsi],10
    je .newline
    inc rsi
    jmp .comment
.name:
    mov rbx,rsi
.namechar:
    inc rsi
    movzx eax,byte [rsi]
    cmp eax,'0'
    jb .nameend
    cmp eax,'9'
    jbe .namechar
    cmp eax,'A'
    jb .nameend
    cmp eax,'Z'
    jbe .namechar
    cmp eax,'_'
    je .namechar
    cmp eax,'a'
    jb .nameend
    cmp eax,'z'
    jbe .namechar
    cmp eax,128
    jae .namechar
.nameend:
    mov rdx,rsi
    sub rdx,rbx
    mov r12,rdx
    mov rcx,rbx
    call duplicate
    mov rdx,rax
    mov ecx,TK_NAME
    call lex_add
    add [g_col],r12
    jmp .loop
.number:
    xor ebx,ebx
    xor r12d,r12d
.digit:
    movzx eax,byte [rsi]
    sub eax,'0'
    cmp eax,9
    ja .numend
    mov r10,0CCCCCCCCCCCCCCCh
    cmp rbx,r10
    ja .invalid
    imul rbx,10
    add rbx,rax
    jc .invalid
    inc rsi
    inc r12
    jmp .digit
.numend:
    mov ecx,TK_INT
    mov r8,rbx
    xor edx,edx
    call lex_add
    add [g_col],r12
    jmp .loop
.string:
    mov [rbp-64],rsi
    mov r13,rax
    inc rsi
    mov rcx,rsi
    call [strlen]
    lea rcx,[rax+1]
    call alloc
    mov rbx,rax
    xor r12d,r12d
.strchar:
    movzx eax,byte [rsi]
    test eax,eax
    jz .invalid
    cmp eax,10
    je .invalid
    cmp eax,r13d
    je .strend
    cmp eax,92
    jne .strput
    inc rsi
    movzx eax,byte [rsi]
    cmp eax,'n'
    jne .estr1
    mov eax,10
    jmp .strput
.estr1:
    cmp eax,'r'
    jne .estr2
    mov eax,13
    jmp .strput
.estr2:
    cmp eax,'t'
    jne .estr3
    mov eax,9
    jmp .strput
.estr3:
    cmp eax,'u'
    je .unicode4
    cmp eax,'U'
    je .unicode8
    cmp eax,'x'
    je .unicode2
    cmp eax,'b'
    je .backspace
    cmp eax,'f'
    je .formfeed
    cmp eax,'v'
    je .vertical
    cmp eax,'a'
    je .bell
    cmp eax,92
    je .strput
    cmp eax,34
    je .strput
    cmp eax,39
    je .strput
    jmp .invalid
.backspace:
    mov eax,8
    jmp .strput
.formfeed:
    mov eax,12
    jmp .strput
.vertical:
    mov eax,11
    jmp .strput
.bell:
    mov eax,7
    jmp .strput
.unicode2:
    mov r9d,2
    jmp .hexstart
.unicode4:
    mov r9d,4
    jmp .hexstart
.unicode8:
    mov r9d,8
.hexstart:
    xor r10d,r10d
.hex:
    inc rsi
    movzx eax,byte [rsi]
    cmp al,'0'
    jb .invalid
    cmp al,'9'
    jbe .hexdigit
    or al,32
    cmp al,'a'
    jb .invalid
    cmp al,'f'
    ja .invalid
    sub eax,'a'-10
    jmp .hexvalue
.hexdigit:
    sub eax,'0'
.hexvalue:
    shl r10d,4
    or r10d,eax
    dec r9d
    jnz .hex
    test r10d,r10d
    jz .invalid
    cmp r10d,10FFFFh
    ja .invalid
    cmp r10d,0D800h
    jb .utf8
    cmp r10d,0DFFFh
    jbe .invalid
.utf8:
    mov eax,r10d
    cmp eax,80h
    jb .strput
    cmp eax,800h
    jb .utf2
    cmp eax,10000h
    jb .utf3
    shr eax,18
    or al,0F0h
    mov [rbx+r12],al
    inc r12
    mov eax,r10d
    shr eax,12
    and al,3Fh
    or al,80h
    jmp .utfmiddle
.utf3:
    shr eax,12
    or al,0E0h
.utfmiddle:
    mov [rbx+r12],al
    inc r12
    mov eax,r10d
    shr eax,6
    and al,3Fh
    or al,80h
    jmp .utflast
.utf2:
    shr eax,6
    or al,0C0h
.utflast:
    mov [rbx+r12],al
    inc r12
    mov eax,r10d
    and al,3Fh
    or al,80h
.strput:
    mov [rbx+r12],al
    inc r12
    inc rsi
    jmp .strchar
.strend:
    inc rsi
    mov ecx,TK_STRING
    mov rdx,rbx
    xor r8d,r8d
    call lex_add
    mov rax,rsi
    sub rax,[rbp-64]
    add [g_col],rax
    jmp .loop
.operator:
    mov ecx,eax
    mov edx,1
    movzx r8d,byte [rsi+1]
    cmp eax,'('
    je .open
    cmp eax,'['
    je .open
    cmp eax,')'
    je .close
    cmp eax,']'
    je .close
    cmp r8d,'='
    je .pair
    cmp eax,'='
    jne .logic
    cmp r8d,'>'
    jne .one
    mov ecx,TK_ARROW
    jmp .two
.logic:
    cmp eax,'&'
    jne .or
    cmp r8d,'&'
    jne .invalid
    mov ecx,TK_AND
    jmp .two
.or:
    cmp eax,'|'
    jne .one
    cmp r8d,'|'
    jne .invalid
    mov ecx,TK_OR
    jmp .two
.open:
    inc r15
    jmp .one
.close:
    dec r15
    js .invalid
    jmp .one
.pair:
    lea r10,[pair_chars]
    xor ebx,ebx
.pairs:
    cmp byte [r10+rbx],0
    je .one
    cmp al,[r10+rbx]
    je .paired
    inc rbx
    jmp .pairs
.paired:
    lea rcx,[rbx+TK_EQ]
.two:
    mov edx,2
.one:
    mov rbx,rdx
    call lex_add
    add rsi,rbx
    add [g_col],rbx
    jmp .loop
.done:
    test r15,r15
    jnz .invalid
    cmp qword [g_lastkind],TK_NEWLINE
    je .endindent
    mov ecx,TK_NEWLINE
    call lex_add
.endindent:
    cmp qword [g_indent],0
    je .eof
    dec qword [g_indent]
    mov ecx,TK_DEDENT
    call lex_add
    jmp .endindent
.eof:
    xor ecx,ecx
    call lex_add
    mov rax,[g_lexbuf]
endpx
.invalid:
    lea rcx,[err_lex]
    call fatal

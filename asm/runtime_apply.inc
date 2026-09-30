; Shared implementation, assembled into both the compiler and generated apps.
; Plan = {preview, count, files[{UTF16 path, old bytes, old size, new bytes,new size}]}.
; Validate all snapshots, stage all writes beside their target, then replace.
; If replacement fails, restore already committed files from their snapshots.
macro acall host,guest {
    if HOST_APPLY = 1
        call [host]
    else
        api guest
    end if
}
procx rt_match_file
    mov rbx,rcx
    mov rcx,[rbx]
    lea rdx,[apply_read]
    acall wfopen,IWfopen
    test rax,rax
    jz .false
    mov rsi,rax
    mov rcx,rax
    xor edx,edx
    mov r8d,2
    acall fseek,IFseek
    test eax,eax
    jnz .closefalse
    mov rcx,rsi
    acall ftell,IFtell
    movsxd rax,eax
    cmp rax,[rbx+16]
    jne .closefalse
    lea rcx,[rax+1]
    call APPLY_ALLOC
    mov rdi,rax
    mov rcx,rsi
    xor edx,edx
    xor r8d,r8d
    acall fseek,IFseek
    mov rcx,rdi
    mov edx,1
    mov r8,[rbx+16]
    mov r9,rsi
    acall fread,IFread
    cmp rax,[rbx+16]
    jne .freefalse
    mov rcx,rdi
    mov rdx,[rbx+8]
    mov r8,[rbx+16]
    acall memcmp,IMemcmp
    mov r12d,eax
    mov rcx,rdi
    acall free,IFree
    mov rcx,rsi
    acall fclose,IFclose
    xor eax,eax
    test r12d,r12d
    sete al
    jmp .done
.freefalse:
    mov rcx,rdi
    acall free,IFree
.closefalse:
    mov rcx,rsi
    acall fclose,IFclose
.false:
    xor eax,eax
.done:
endpx
procx rt_stage_file
    ; Absolute UTF16 destination, content, size -> staged UTF16 path, or zero.
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    mov ecx,65536
    call APPLY_ALLOC
    mov rsi,rax
    xor edi,edi
    xor r14d,r14d
.copy:
    cmp edi,65532
    jae .false
    mov ax,[rbx+rdi]
    mov [rsi+rdi],ax
    test ax,ax
    jz .directory
    cmp ax,92
    jne .next
    mov r14,rdi
.next:
    add rdi,2
    jmp .copy
.directory:
    test r14,r14
    jz .false
    mov word [rsi+r14+2],0
    mov ecx,65536
    call APPLY_ALLOC
    mov rdi,rax
    mov rcx,rsi
    lea rdx,[apply_prefix]
    xor r8d,r8d
    mov r9,rdi
    acall GetTempFileNameW,ITempName
    test eax,eax
    jz .freepaths
    mov rcx,rdi
    lea rdx,[apply_write]
    acall wfopen,IWfopen
    test rax,rax
    jz .delete
    mov rbx,rax
    mov rcx,r12
    mov edx,1
    mov r8,r13
    mov r9,rbx
    acall fwrite,IFwrite
    mov r14,rax
    mov rcx,rbx
    acall fclose,IFclose
    test eax,eax
    jnz .delete
    cmp r14,r13
    jne .delete
    mov rcx,rsi
    acall free,IFree
    mov rax,rdi
    jmp .done
.delete:
    mov rcx,rdi
    acall DeleteFileW,IDelete
.freepaths:
    mov rcx,rdi
    acall free,IFree
.false:
    mov rcx,rsi
    acall free,IFree
    xor eax,eax
.done:
endpx
procx rt_apply
    mov r12,rcx
    mov r13,[rcx+8]
    cmp r13,1024
    ja .invalid
    lea rcx,[r13*8+8]
    call APPLY_ALLOC
    mov r14,rax
    xor edi,edi
.zero:
    cmp rdi,r13
    jae .start
    mov qword [r14+rdi*8],0
    inc rdi
    jmp .zero
.start:
    xor edi,edi
    lea rbx,[r12+16]
    lea rax,[apply_stale]
    mov [rbp-64],rax
.validate:
    cmp rdi,r13
    jae .stagebegin
    mov rcx,rbx
    call rt_match_file
    test eax,eax
    jz .cleanup
    inc rdi
    add rbx,40
    jmp .validate
.stagebegin:
    xor edi,edi
    lea rbx,[r12+16]
    lea rax,[apply_io]
    mov [rbp-64],rax
.stage:
    cmp rdi,r13
    jae .commitbegin
    mov rcx,[rbx]
    mov rdx,[rbx+24]
    mov r8,[rbx+32]
    call rt_stage_file
    test rax,rax
    jz .cleanup
    mov [r14+rdi*8],rax
    inc rdi
    add rbx,40
    jmp .stage
.commitbegin:
    xor r15d,r15d
    lea rbx,[r12+16]
.commit:
    cmp r15,r13
    jae .success
    ; Recheck immediately before each commit to detect intervening edits.
    mov rcx,rbx
    call rt_match_file
    test eax,eax
    jz .rollbackbegin
    mov rcx,[r14+r15*8]
    mov rdx,[rbx]
    mov r8d,9
    acall MoveFileExW,IMove
    test eax,eax
    jz .rollbackbegin
    mov rcx,[r14+r15*8]
    acall free,IFree
    mov qword [r14+r15*8],0
    inc r15
    add rbx,40
    jmp .commit
.rollbackbegin:
    xor edi,edi
    lea rbx,[r12+16]
.rollback:
    cmp rdi,r15
    jae .cleanup
    mov rcx,[rbx]
    mov rdx,[rbx+8]
    mov r8,[rbx+16]
    call rt_stage_file
    test rax,rax
    jz .rollbackfailed
    mov rsi,rax
    mov rcx,rax
    mov rdx,[rbx]
    mov r8d,9
    acall MoveFileExW,IMove
    test eax,eax
    jnz .rollbackfree
    mov rcx,rsi
    acall DeleteFileW,IDelete
    lea rax,[apply_rollback]
    mov [rbp-64],rax
.rollbackfree:
    mov rcx,rsi
    acall free,IFree
    jmp .rollbacknext
.rollbackfailed:
    lea rax,[apply_rollback]
    mov [rbp-64],rax
.rollbacknext:
    inc edi
    add rbx,40
    jmp .rollback
.cleanup:
    xor edi,edi
.clean:
    cmp rdi,r13
    jae .error
    mov rsi,[r14+rdi*8]
    test rsi,rsi
    jz .cleannext
    mov rcx,rsi
    acall DeleteFileW,IDelete
    mov rcx,rsi
    acall free,IFree
.cleannext:
    inc edi
    jmp .clean
.error:
    mov rcx,r14
    acall free,IFree
    mov rcx,[rbp-64]
    call APPLY_ERROR
    xor eax,eax
    jmp .done
.success:
    mov rcx,r14
    acall free,IFree
    mov eax,1
    jmp .done
.invalid:
    lea rcx,[apply_io]
    call APPLY_ERROR
    xor eax,eax
.done:
endpx
apply_read du 'rb',0
apply_write du 'wb',0
apply_prefix du 'sla',0
apply_stale db 'Slai IA: source changed since compilation; no changes applied',0
apply_io db 'Slai IA: file operation failed; changes rolled back',0
apply_rollback db 'Slai IA: rollback failed; inspect affected source files',0

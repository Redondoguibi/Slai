; Semantic index: entries retain declaration identities and bound source uses.
S_NAME=0
S_KIND=8
S_OWNER=16
S_TOKEN=24
S_DECL=32
S_REFS=40
S_NEXT=48
S_TYPE=56
S_KEY=64
S_DEPS=72
procx mark_member
    ; Record all receiver owners of a polymorphic AST member, not just the last.
    cmp rdx,4096
    jb .done
    cmp qword [rdx],6
    je .owner
    cmp qword [rdx],7
    jne .done
.owner:
    mov rax,[rdx+8]
    cmp qword [rcx+88],0
    je .set
    cmp [rcx+88],rax
    je .done
    mov rax,-1
.set:
    mov [rcx+88],rax
.done:
endpx
procx symbol_decl
    mov rax,[g_symbols]
.loop:
    test rax,rax
    jz .done
    cmp [rax+S_DECL],rcx
    je .done
    mov rax,[rax+S_NEXT]
    jmp .loop
.done:
endpx
procx symbol_member
    mov rbx,rcx
    mov rdi,rdx
    mov rsi,[g_symbols]
.loop:
    test rsi,rsi
    jz .done
    cmp [rsi+S_OWNER],rdi
    jne .next
    mov rcx,[rsi+S_NAME]
    mov rdx,rbx
    call equal
    test eax,eax
    jnz .done
.next:
    mov rsi,[rsi+S_NEXT]
    jmp .loop
.done:
    mov rax,rsi
endpx
procx symbol_add
    ; declaration AST, kind string, owner AST -> entry
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,r8
    call symbol_decl
    test rax,rax
    jnz .done
    mov ecx,96
    call alloc
    mov r12,rax
    mov rcx,[rbx+NV]
    mov [rax+S_NAME],rcx
    mov [rax+S_KIND],rsi
    mov [rax+S_OWNER],rdi
    mov rcx,[rbx+NT]
    mov [rax+S_TOKEN],rcx
    mov [rax+S_DECL],rbx
    mov rcx,[g_symbols]
    mov [rax+S_NEXT],rcx
    mov [g_symbols],rax
    ; Fully qualified keys also distinguish parameters and local variables.
    call text_begin
    test rdi,rdi
    jz .name
    mov rcx,[rdi+NV]
    test rcx,rcx
    jz .name
    call text_append
    lea rcx,[dot]
    call text_append
.name:
    mov rcx,[rbx+NV]
    call text_append
    mov rax,[g_text]
    mov [r12+S_KEY],rax
    mov rax,r12
.done:
endpx
procx symbol_ref
    ; entry, reference AST. Token identity deduplicates specializations.
    mov rbx,rcx
    mov rsi,rdx
    mov rdi,[rsi+NT]
    cmp [rbx+S_TOKEN],rdi
    je .done
    mov rax,[rbx+S_REFS]
.find:
    test rax,rax
    jz .add
    cmp [rax],rdi
    je .done
    mov rax,[rax+16]
    jmp .find
.add:
    mov ecx,40
    call alloc
    mov [rax],rdi
    mov [rax+8],rsi
    mov rcx,[g_walkcall]
    mov [rax+24],rcx
    mov rcx,[g_walkfn]
    mov [rax+32],rcx
    mov rcx,[rbx+S_REFS]
    mov [rax+16],rcx
    mov [rbx+S_REFS],rax
.done:
endpx
procx walk_ast
    mov rbx,rcx
.loop:
    test rbx,rbx
    jz .done
    mov r12,[g_walkfn]
    mov r13,[g_walkclass]
    mov r14,[g_walkcall]
    mov r15,[g_walkrecord]
    mov rcx,rbx
    call index_node
    mov rax,[rbx+NK]
    cmp rax,N_FUNC
    je .function
    cmp rax,N_CLASS
    je .class
    cmp rax,N_OBJECT
    je .object
    cmp rax,N_FOR
    je .for
    cmp rax,N_IF
    je .one
    cmp rax,N_BRANCH
    je .two
    cmp rax,N_ASSIGN
    je .two
    cmp rax,N_CATCH
    je .two
    cmp rax,N_CALL
    je .call
    cmp rax,N_BINARY
    je .two
    cmp rax,N_INDEX
    je .two
    cmp rax,N_INT
    je .next
    cmp rax,N_STRING
    je .next
    cmp rax,N_BOOL
    je .next
    cmp rax,N_NAME
    je .next
    cmp rax,N_PARAM
    je .next
    jmp .one
.function:
    mov [g_walkfn],rbx
    mov rcx,[rbx+NB]
    call walk_ast
    jmp .one
.class:
    mov [g_walkclass],rbx
    jmp .one
.object:
    mov [g_walkrecord],rbx
    jmp .one
.for:
    mov rcx,[rbx+NC]
    call walk_ast
    mov rcx,[rbx+NX]
    call walk_ast
    jmp .two
.call:
    mov rcx,rbx
    call ia_operation
    test rax,rax
    jnz .next
    mov [g_walkcall],rbx
.two:
    mov rcx,[rbx+NB]
    call walk_ast
.one:
    mov rcx,[rbx+NA]
    call walk_ast
.next:
    mov [g_walkfn],r12
    mov [g_walkclass],r13
    mov [g_walkcall],r14
    mov [g_walkrecord],r15
    mov rbx,[rbx+NN]
    jmp .loop
.done:
endpx
procx index_node
    mov rbx,rcx
    cmp qword [g_indexphase],2
    je .symbolic
    cmp qword [g_indexphase],0
    jne .refs
    mov rax,[rbx+NK]
    lea rdx,[kind_class]
    cmp rax,N_CLASS
    je .global
    lea rdx,[kind_function]
    cmp rax,N_FUNC
    je .function
    lea rdx,[kind_parameter]
    cmp rax,N_PARAM
    je .local
    lea rdx,[kind_variable]
    cmp rax,N_FOR
    je .local
    cmp rax,N_CATCH
    je .local
    cmp rax,N_NAME
    jne .done
    cmp [rbx+80],rbx
    jne .done
.local:
    mov r8,[g_walkfn]
    jmp .add
.global:
    xor r8d,r8d
    jmp .add
.function:
    mov r8,[rbx+NX]
    test r8,r8
    jz .add
    lea rdx,[kind_method]
.add:
    mov rcx,rbx
    call symbol_add
    jmp .done
.refs:
    mov rax,[rbx+NK]
    cmp rax,N_FIELD
    je .field
    cmp rax,N_NAME
    je .name
    cmp rax,N_MEMBER
    je .member
    jmp .done
.field:
    mov rcx,rbx
    lea rdx,[kind_field]
    mov r8,[g_walkrecord]
    call symbol_add
    jmp .done
.symbolic:
    cmp qword [rbx+NK],N_CALL
    jne .done
    mov rcx,rbx
    call ia_operation
    test rax,rax
    jz .done
    mov rsi,rax
    mov rcx,rax
    lea rdx,[kw_apply]
    call equal
    test eax,eax
    jnz .done
    mov rcx,rsi
    lea rdx,[kw_preview]
    call equal
    test eax,eax
    jnz .done
    mov rcx,[rbx+NB]
    test rcx,rcx
    jz .done
    mov r12,[g_walkcall]
    mov [g_walkcall],rbx
    call bind_symbolic_target
    mov [g_walkcall],r12
    jmp .done
.name:
    mov rcx,[rbx+NV]
    lea rdx,[kw_this]
    call equal
    test eax,eax
    jnz .done
    mov rcx,[rbx+80]
    test rcx,rcx
    jz .done
    call symbol_decl
    test rax,rax
    jz .done
    jmp .ref
.member:
    mov rdi,[rbx+88]
    test rdi,rdi
    jnz .owner
    mov rax,[rbx+NA]
    cmp qword [rax+NK],N_NAME
    jne .ambiguous
    mov rcx,[rax+NV]
    lea rdx,[kw_this]
    call equal
    test eax,eax
    jz .ambiguous
    mov rdi,[g_walkclass]
.owner:
    test rdi,rdi
    jz .ambiguous
    cmp rdi,-1
    je .ambiguous
    mov rcx,[rbx+NV]
    mov rdx,rdi
    call symbol_member
    test rax,rax
    jnz .ref
    mov rcx,rbx
    lea rdx,[kind_field]
    mov r8,rdi
    call symbol_add
    jmp .done
.ambiguous:
    ; Retain unresolved generic member uses; refactors must refuse ambiguity.
    mov ecx,16
    call alloc
    mov [rax],rbx
    mov rcx,[g_ambiguous]
    mov [rax+8],rcx
    mov [g_ambiguous],rax
    jmp .done
.ref:
    mov rcx,rax
    mov rdx,rbx
    call symbol_ref
.done:
endpx
procx build_index
    mov qword [g_indexphase],0
    mov rcx,[g_program]
    call walk_ast
    mov qword [g_indexphase],1
    mov rcx,[g_program]
    call walk_ast
    mov qword [g_indexphase],2
    mov rcx,[g_program]
    call walk_ast
endpx
procx bind_symbolic_target
    mov rbx,rcx
    cmp qword [rbx+NK],N_STRING
    je .done
    call symbolic_name
    mov rcx,rax
    call resolve_symbol
    mov rcx,rax
    mov rdx,rbx
    call symbol_ref
    cmp qword [rbx+NK],N_MEMBER
    jne .done
    mov rcx,[rbx+NA]
    call bind_symbolic_target
.done:
endpx
procx resolve_symbol
    mov rbx,rcx
    mov rsi,[g_symbols]
    xor edi,edi
.loop:
    test rsi,rsi
    jz .end
    mov rcx,[rsi+S_KEY]
    mov rdx,rbx
    call equal
    test eax,eax
    jz .next
    test rdi,rdi
    jnz .ambiguous
    mov rdi,rsi
.next:
    mov rsi,[rsi+S_NEXT]
    jmp .loop
.end:
    test rdi,rdi
    jz .bad
    mov rax,rdi
endpx
.bad:
    lea rcx,[err_name]
    call fatal
.ambiguous:
    lea rcx,[err_ambiguous]
    call fatal
procx symbolic_name
    ; Symbolic query AST -> qualified UTF-8 name (name or member chain).
    mov rbx,rcx
    cmp qword [rbx+NK],N_STRING
    je .simple
    cmp qword [rbx+NK],N_NAME
    je .simple
    cmp qword [rbx+NK],N_MEMBER
    jne .bad
    mov rcx,[rbx+NA]
    call symbolic_name
    mov rsi,rax
    call text_begin
    mov rcx,rsi
    call text_append
    lea rcx,[dot]
    call text_append
    mov rcx,[rbx+NV]
    call text_append
    mov rax,[g_text]
    jmp .done
.simple:
    mov rax,[rbx+NV]
.done:
endpx
.bad:
    lea rcx,[err_type]
    call fatal
procx json_refs
    mov rbx,rcx
    lea rcx,[json_array]
    call text_append
    mov rsi,[rbx+S_REFS]
    xor edi,edi
.loop:
    test rsi,rsi
    jz .done
    test edi,edi
    jz .location
    lea rcx,[comma]
    call text_append
.location:
    mov rcx,[rsi]
    call json_location
    inc edi
    mov rsi,[rsi+16]
    jmp .loop
.done:
    lea rcx,[json_endarray]
    call text_append
endpx
procx json_symbol
    mov rbx,rcx
    lea rcx,[json_name]
    call text_append
    mov rcx,[rbx+S_NAME]
    call json_string
    lea rcx,[json_id]
    call text_append
    mov rcx,[rbx+S_KEY]
    call json_string
    lea rcx,[json_kind]
    call text_append
    mov rcx,[rbx+S_KIND]
    call json_string
    lea rcx,[json_declaration]
    call text_append
    mov rcx,[rbx+S_TOKEN]
    call json_location
    lea rcx,[json_references]
    call text_append
    mov rcx,rbx
    call json_refs
    lea rcx,[json_type]
    call text_append
    mov rcx,rbx
    call json_symbol_types
    lea rcx,[json_close]
    call text_append
endpx
procx query_index
    ; operation, optional target -> allocated JSON string
    mov rbx,rcx
    mov rsi,rdx
    mov rcx,rbx
    lea rdx,[kw_errors]
    call equal
    test eax,eax
    jnz .empty
    test rsi,rsi
    jz .all
    mov rcx,rsi
    call resolve_symbol
    mov rdi,rax
    mov rcx,rbx
    lea rdx,[arg_inspect]
    call equal
    test eax,eax
    jnz .inspect
    mov rcx,rbx
    lea rdx,[kw_refs]
    call equal
    test eax,eax
    jnz .refs
    mov rcx,rbx
    lea rdx,[kw_type]
    call equal
    test eax,eax
    jnz .type
    mov rcx,rbx
    lea rdx,[kw_tree]
    call equal
    test eax,eax
    jnz .tree
    mov rcx,rbx
    lea rdx,[kw_dependencies]
    call equal
    test eax,eax
    jnz .dependencies
    jmp .unsupported
.all:
    mov rcx,rbx
    lea rdx,[arg_inspect]
    call equal
    test eax,eax
    jz .unsupported
    call text_begin
    lea rcx,[inspect_header]
    call text_append
    mov rsi,[g_symbols]
    xor edi,edi
.symbols:
    test rsi,rsi
    jz .allend
    test edi,edi
    jz .symbol
    lea rcx,[comma]
    call text_append
.symbol:
    mov rcx,rsi
    call json_symbol
    inc edi
    mov rsi,[rsi+S_NEXT]
    jmp .symbols
.allend:
    lea rcx,[inspect_footer]
    call text_append
    jmp .result
.inspect:
    call text_begin
    mov rcx,rdi
    call json_symbol
    jmp .result
.refs:
    call text_begin
    mov rcx,rdi
    call json_refs
    jmp .result
.type:
    call text_begin
    mov rcx,rdi
    call json_symbol_types
    jmp .result
.tree:
    call text_begin
    mov rcx,[rdi+S_DECL]
    call json_ast
    jmp .result
.dependencies:
    call text_begin
    mov rcx,rdi
    call json_dependencies
    jmp .result
.empty:
    lea rax,[json_empty]
    jmp .done
.result:
    mov rax,[g_text]
.done:
endpx
.unsupported:
    lea rcx,[err_unsupported]
    call fatal
procx json_ast
    mov rbx,rcx
    lea rcx,[json_astkind]
    call text_append
    mov rcx,[rbx+NK]
    call text_number
    lea rcx,[json_declaration]
    call text_append
    mov rcx,[rbx+NT]
    call json_location
    lea rcx,[json_children]
    call text_append
    xor edi,edi
    ; Typed AST children only: pointer-free scalar fields are never traversed.
    mov rax,[rbx+NK]
    cmp rax,N_INT
    je .end
    cmp rax,N_BOOL
    je .end
    cmp rax,N_STRING
    je .end
    cmp rax,N_NAME
    je .end
    cmp rax,N_PARAM
    je .end
    mov rsi,[rbx+NA]
    call .chain
    mov rax,[rbx+NK]
    cmp rax,N_CALL
    je .second
    cmp rax,N_ASSIGN
    je .second
    cmp rax,N_BINARY
    je .second
    cmp rax,N_INDEX
    je .second
    cmp rax,N_FUNC
    je .second
    cmp rax,N_BRANCH
    je .second
    cmp rax,N_CATCH
    je .second
    cmp rax,N_FOR
    jne .end
    mov rsi,[rbx+NC]
    call .chain
.second:
    mov rsi,[rbx+NB]
    call .chain
.end:
    lea rcx,[inspect_footer]
    call text_append
    jmp .done
.chain:
    ; The caller already owns shadow space; reserve another aligned call frame.
    sub rsp,40
.loop:
    test rsi,rsi
    jz .chainend
    test edi,edi
    jz .item
    lea rcx,[comma]
    call text_append
.item:
    mov rcx,rsi
    call json_ast
    inc edi
    mov rsi,[rsi+NN]
    jmp .loop
.chainend:
    add rsp,40
    ret
.done:
endpx
procx json_dependencies
    ; Declaration references within the target's source span provide direct edges.
    mov rbx,rcx
    lea rcx,[json_array]
    call text_append
    mov rsi,[g_symbols]
    xor edi,edi
.loop:
    test rsi,rsi
    jz .done
    cmp rsi,rbx
    je .next
    mov rax,[rbx+S_DECL]
    cmp [rsi+S_OWNER],rax
    je .next
    mov r12,[rsi+S_REFS]
.refs:
    test r12,r12
    jz .next
    mov rax,[r12+32]
    cmp rax,[rbx+S_DECL]
    je .add
    test rax,rax
    jz .refnext
    mov rax,[rax+NX]
    cmp rax,[rbx+S_DECL]
    je .add
.refnext:
    mov r12,[r12+16]
    jmp .refs
.add:
    test edi,edi
    jz .key
    lea rcx,[comma]
    call text_append
.key:
    mov rcx,rsi
    call json_symbol
    inc edi
.next:
    mov rsi,[rsi+S_NEXT]
    jmp .loop
.done:
    lea rcx,[json_endarray]
    call text_append
endpx
procx report_type
    mov rbx,rcx
    test rcx,rcx
    jz .done
    mov rax,[g_reporttypes]
.loop:
    test rax,rax
    jz .add
    cmp [rax],rbx
    je .done
    mov rax,[rax+8]
    jmp .loop
.add:
    mov ecx,16
    call alloc
    mov [rax],rbx
    mov rcx,[g_reporttypes]
    mov [rax+8],rcx
    mov [g_reporttypes],rax
.done:
endpx
procx json_symbol_types
    mov rbx,rcx
    mov qword [g_reporttypes],0
    mov rdi,[rbx+S_DECL]
    mov rax,[rdi+NK]
    cmp rax,N_CLASS
    je .class
    cmp rax,N_FUNC
    je .functions
    cmp rax,N_PARAM
    je .functions
    mov rax,[rdi+NZ]
    test rax,rax
    jz .functions
    mov rcx,[rax+8]
    call report_type
.functions:
    mov rsi,[g_functions]
.function:
    test rsi,rsi
    jz .array
    cmp [rsi+F_AST],rdi
    jne .locals
    mov rcx,[rsi+F_TYPE]
    call report_type
.locals:
    cmp qword [rdi+NK],N_PARAM
    jne .next
    mov r12,[rsi+F_LOCALS]
.local:
    test r12,r12
    jz .next
    mov rax,[r12+32]
    cmp [rbx+S_TOKEN],rax
    jne .localnext
    mov rcx,[r12+8]
    call report_type
.localnext:
    mov r12,[r12+24]
    jmp .local
.next:
    mov rsi,[rsi+F_NEXT]
    jmp .function
.array:
    lea rcx,[json_array]
    call text_append
    mov rsi,[g_reporttypes]
    xor edi,edi
.item:
    test rsi,rsi
    jz .end
    test edi,edi
    jz .type
    lea rcx,[comma]
    call text_append
.type:
    mov rcx,[rsi]
    call type_name
    mov rcx,rax
    call json_string
    inc edi
    mov rsi,[rsi+8]
    jmp .item
.class:
    lea rcx,[json_array]
    call text_append
    mov rcx,[rbx+S_NAME]
    call json_string
.end:
    lea rcx,[json_endarray]
    call text_append
endpx


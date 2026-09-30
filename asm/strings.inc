empty db 0
comma db ',',0
json_quote db 34,0
json_close db '}',0
json_array db '[',0
json_endarray db ']',0
json_file db '{"file":',0
json_line db ',"line":',0
json_column db ',"column":',0
fmt_number db '%I64d',0
fmt_escape db '\u%04x',0
fmt_text db '%s',10,0
mode_read du 'rb',0
mode_write du 'wb',0
temp_prefix du 'sla',0
suffix_slai db '.slai',0
suffix_exe db '.exe',0
pair_chars db '=!<>+-*/%',0
assign_operators db '+-*/%'
assert_message db 'Assertion failed',0
arg_version db '--version',0
arg_build db 'build',0
arg_run db 'run',0
arg_check db 'check',0
arg_inspect db 'inspect',0
arg_sir db 'sir',0
arg_json db '--json',0
arg_release db '--release',0
arg_output db '-o',0
arg_apply db '--apply',0
arg_type db '--type',0
json_plan db '{"files":[',0
json_before db ',"before":',0
json_after db ',"after":',0
json_preview db '],"preview":',0
version_text db 'Slai 0.2.0-asm (Assembly x86-64)',10,0
usage_text db 'slai file.slai [--release]',10,'slai build|run|check|inspect|sir file.slai [--release] [--json] [-o output.exe]',10,0
check_ok db 'OK',10,0
json_ok db '{"ok":true}',10,0
fmt_built db '%s',10,0
inspect_header db '{"implementation":"assembly-x64","symbols":[',0
inspect_footer db ']}',10,0
fmt_symbol db '{"name":"%s"}',0
sir_header db '{"instructions":[',0
fmt_sir db '{"opcode":%I64d}',0
fmt_error db '%s:%I64d:%I64d: error: %s',10,0
fmt_json_error db '{"ok":false,"line":%I64d,"column":%I64d,"message":"%s"}',10,0
err_memory db 'Out of memory',0
err_path db 'Invalid path',0
err_read db 'Cannot read source file',0
err_write db 'Cannot write output (source files cannot be overwritten)',0
err_run db 'Cannot start generated executable',0
err_expected db 'Unexpected token',0
err_expression db 'Expected expression',0
err_limit db 'Compiler resource limit exceeded',0
err_lex db 'Invalid token, indentation, literal or unterminated comment',0
err_cycle db 'Import cycle',0
err_duplicate db 'Duplicate or conflicting declaration',0
err_type db 'Incompatible or unsupported type',0
err_name db 'Undefined name or member',0
err_arity db 'Incorrect argument count',0
err_scope db 'Invalid statement in this scope',0
err_return db 'Not all paths return a value',0
err_recursive db 'Recursive constructor is unsupported',0
err_unsupported db 'Operation is not implemented in the Assembly compiler',0
err_internal db 'Invalid internal instruction',0
kw_int db 'int',0
kw_bool db 'bool',0
kw_string db 'string',0
kw_true db 'true',0
kw_false db 'false',0
kw_and db 'and',0
kw_or db 'or',0
kw_not db 'not',0
kw_await db 'await',0
kw_break db 'break',0
kw_continue db 'continue',0
kw_class db 'cl',0
kw_else db 'el',0
kw_error db 'error',0
kw_for db 'for',0
kw_func db 'func',0
kw_if db 'if',0
kw_import db 'import',0
kw_obj db 'obj',0
kw_return db 'return',0
kw_args db 'args',0
kw_this db 'this',0
kw_length db 'length',0
kw_push db 'push',0
kw_log db 'log',0
kw_sys db 'sys',0
kw_ia db 'ia',0
kw_preview db 'preview',0
kw_apply db 'apply',0
kw_errors db 'errors',0
kw_refs db 'refs',0
kw_type db 'type',0
kw_tree db 'tree',0
kw_dependencies db 'dependencies',0
kw_rename db 'rename',0
kw_edit db 'edit',0
kw_param db 'param',0
kind_class db 'class',0
kind_function db 'function',0
kind_method db 'method',0
kind_parameter db 'parameter',0
kind_variable db 'variable',0
kind_field db 'field',0
kind_object db 'object',0
json_empty db '[]',0
json_name db '{"name":',0
json_id db ',"id":',0
json_kind db ',"kind":',0
json_declaration db ',"declaration":',0
json_references db ',"references":',0
json_children db ',"children":[',0
json_astkind db '{"kind":',0
err_ambiguous db 'Ambiguous semantic reference; change refused',0
err_identifier db 'Invalid or reserved identifier',0
comma_space db ', ',0
colon_space db ': ',0
default_int db '0',0
default_string db '""',0
newline_text db 10,0
preview_before db '--- before: ',0
preview_after db 10,'+++ after: ',0
reserved_words dq kw_sys,kw_this,kw_true,kw_false,kw_if,kw_else,kw_for,kw_func,kw_class,kw_obj,kw_import,kw_return,kw_break,kw_continue,kw_await,kw_error,kw_not,kw_and,kw_or,0
kw_sleep db 'sleep',0
kw_assert db 'assert',0
kw_spawn db 'spawn',0

align 8
pe_template:
    dw 5A4Dh
    rb 58
    dd 80h
    rb 64
    dd 00004550h
    dw 8664h,5
    dd 0,0,0
    dw 240,23h
    dw 20Bh
    db 0,2
    dd 0,0,0
    dd RT_ENTRY-IMAGE_BASE,1000h
    dq IMAGE_BASE
    dd 1000h,200h
    dw 6,0,0,0,6,0
    dd 0,0,400h,0
    dw 3,100h
    dq 100000h,1000h,100000h,1000h
    dd 0,16
    dd 0,0
    dd IMPORT_BASE-IMAGE_BASE,60
    dq 0
    dd PDATA_BASE-IMAGE_BASE,0
    rq 12
    db '.text',0,0,0
    dd 0,1000h,0,400h,0,0
    dw 0,0
    dd 60000020h
    db '.idata',0,0
    dd DATA_BASE-IMPORT_BASE,IMPORT_BASE-IMAGE_BASE,(imports_size+511) and -512,0,0,0
    dw 0,0
    dd 0C0000040h
    db '.data',0,0,0
    dd 0,DATA_BASE-IMAGE_BASE,0,0,0,0
    dw 0,0
    dd 0C0000040h
    db '.rdata',0,0
    dd 0,CONST_BASE-IMAGE_BASE,0,0,0,0
    dw 0,0
    dd 40000040h
    db '.pdata',0,0
    dd 0,PDATA_BASE-IMAGE_BASE,0,0,0,0
    dw 0,0
    dd 40000040h
pe_template_size=$-pe_template
dot db '.',0

kw_void db 'void',0
kind_change db 'change',0
kind_list db 'list',0
kind_task db 'task',0
main_name db 'main',0
json_slots db ',"frame_slots":',0
json_instructions db ',"instructions":[',0
json_op db '{"op":',0
json_out db ',"out":',0
json_type db ',"type":',0
json_value db ',"value":',0
opcode_names dq op_const,op_copy,op_binary,op_unary,op_compare,op_call,op_native,op_label,op_jump,op_branch,op_return,op_get,op_set,op_errorcheck,op_errorjump,op_alloc,op_address,op_array
op_const db 'const',0
op_copy db 'copy',0
op_binary db 'binary',0
op_unary db 'unary',0
op_compare db 'compare',0
op_call db 'call',0
op_native db 'native',0
op_label db 'label',0
op_jump db 'jump',0
op_branch db 'branch',0
op_return db 'return',0
op_get db 'field_get',0
op_set db 'field_set',0
op_errorcheck db 'error_check',0
op_errorjump db 'error_jump',0
op_alloc db 'alloc',0
op_address db 'function_address',0
op_array db 'array',0
colon_text db ':',0
error_prefix db ': error: ',0
json_diagnostic db '{"ok":false,"diagnostics":[',0
json_message db ',"message":',0
json_diagnostic_end db ',"code":"ESLAI"}]}',10,0

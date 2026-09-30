format PE64 console 6.0
entry start
include '../tools/fasm/INCLUDE/WIN64A.INC'
include 'layout.inc'
include 'definitions.inc'

section '.text' code readable executable
include 'support.inc'
include 'json.inc'
include 'lexer.inc'
include 'parser.inc'
include 'semantic.inc'
include 'optimizer.inc'
include 'encoder.inc'
include 'pe.inc'
include 'cli.inc'
HOST_APPLY=1
APPLY_ALLOC equ alloc
APPLY_ERROR equ host_apply_error
include 'runtime_apply.inc'

section '.rdata' data readable
runtime_blob file 'runtime.bin'
runtime_size = $-runtime_blob
load RT_ENTRY qword from runtime_blob
load RT_ALLOC qword from runtime_blob+8
load RT_LOG_INT qword from runtime_blob+16
load RT_LOG_STR qword from runtime_blob+24
load RT_LOG_BOOL qword from runtime_blob+32
load RT_COMPARE qword from runtime_blob+40
load RT_LIST_NEW qword from runtime_blob+48
load RT_LIST_PUSH qword from runtime_blob+56
load RT_LIST_GET qword from runtime_blob+64
load RT_LIST_SET qword from runtime_blob+72
load RT_ERROR_GET qword from runtime_blob+80
load RT_ERROR_SET qword from runtime_blob+88
load RT_ERROR_TAKE qword from runtime_blob+96
load RT_PANIC qword from runtime_blob+104
load RT_DIV qword from runtime_blob+112
load RT_MOD qword from runtime_blob+120
load RT_SPAWN qword from runtime_blob+128
load RT_AWAIT qword from runtime_blob+136
load RT_SLEEP qword from runtime_blob+144
load RT_ASSERT qword from runtime_blob+152
load RT_APPLY qword from runtime_blob+160
load RT_PDATA qword from runtime_blob+168
load RT_PDATA_SIZE qword from runtime_blob+176
imports_blob file 'imports.bin'
imports_size = $-imports_blob
include 'strings.inc'

section '.data' data readable writeable
g_tok dq 0
g_file dq 0
g_source dq 0
g_lexbuf dq 0
g_lexcount dq 0
g_line dq 1
g_col dq 1
g_indent dq 0
g_levels rq 256
g_lastkind dq 0
g_modules dq 0
g_program dq 0
g_defs dq 0
g_env dq 0
g_text dq 0
g_textlen dq 0
g_infunction dq 0
g_inloop dq 0
g_indepth dq 0
g_symbols dq 0
g_walkfn dq 0
g_walkclass dq 0
g_indexphase dq 0
g_ambiguous dq 0
g_iarequests dq 0
g_edits dq 0
g_hostplan dq 0
g_applyerror dq 0
g_clioperation dq 0
g_clitarget dq 0
g_cliname dq 0
g_clitype dq kw_int
g_cliapply dq 0
g_walkcall dq 0
g_walkrecord dq 0
g_reporttypes dq 0
g_ctx dq 0
g_functions dq 0
g_lastfunction dq 0
g_main dq 0
g_globals dq 0
g_globalcount dq 0
g_types dq 0
g_constants dq 0
g_constsize dq 0
g_label dq 0
g_code dq 0
g_codesize dq 0
g_fixups dq 0
g_jumpfix dq 0
g_labels dq 0
g_json dq 0
g_release dq 0
g_folded dq 0
g_encodingfn dq 0
g_command dq 0
g_output dq 0
g_input dq 0
g_temp dq 0
g_exitcode dd 0
g_image dq 0
g_imagesize dq 0
g_textsize dq 0
g_textraw dq 0
g_importraw dq 0
g_dataraw dq 0
g_datasize dq 0
g_constraw dq 0
g_pdata dq 0
g_pdatasize dq 0
g_pdatatablesize dq 0
g_pdataraw dq 0
g_functioncount dq 0
g_argc dd 0
g_argv dq 0
g_pi rb 24
g_si rb 104

section '.idata' import data readable writeable
library kernel,'KERNEL32.DLL',crt,'msvcrt.dll',shell,'SHELL32.DLL'
import kernel,ExitProcess,'ExitProcess',GetCommandLineW,'GetCommandLineW',\
    WideCharToMultiByte,'WideCharToMultiByte',MultiByteToWideChar,'MultiByteToWideChar',\
    GetFullPathNameW,'GetFullPathNameW',CreateProcessW,'CreateProcessW',\
    WaitForSingleObject,'WaitForSingleObject',GetExitCodeProcess,'GetExitCodeProcess',\
    CloseHandle,'CloseHandle',SetConsoleOutputCP,'SetConsoleOutputCP',\
    GetTempPathW,'GetTempPathW',GetTempFileNameW,'GetTempFileNameW',DeleteFileW,'DeleteFileW',\
    MoveFileExW,'MoveFileExW',GetStdHandle,'GetStdHandle',WriteFile,'WriteFile'
import shell,CommandLineToArgvW,'CommandLineToArgvW'
import crt,malloc,'malloc',calloc,'calloc',realloc,'realloc',free,'free',\
    memcpy,'memcpy',memset,'memset',memcmp,'memcmp',strlen,'strlen',strcmp,'strcmp',\
    strcpy,'strcpy',strcat,'strcat',strrchr,'strrchr',printf,'printf',sprintf,'sprintf',\
    wfopen,'_wfopen',fclose,'fclose',fread,'fread',fwrite,'fwrite',fseek,'fseek',ftell,'ftell',stricmp,'_stricmp'


format binary
use64
include 'layout.inc'
org IMPORT_BASE
dd crt_iat-IMAGE_BASE,0,0,crt_name-IMAGE_BASE,crt_iat-IMAGE_BASE
dd kernel_iat-IMAGE_BASE,0,0,kernel_name-IMAGE_BASE,kernel_iat-IMAGE_BASE
dd 0,0,0,0,0
times 100h-($-IMPORT_BASE) db 0
crt_iat:
dq h_malloc-IMAGE_BASE,h_realloc-IMAGE_BASE,h_printf-IMAGE_BASE,h_fflush-IMAGE_BASE
dq h_strcmp-IMAGE_BASE,h_free-IMAGE_BASE,h_strlen-IMAGE_BASE
dq h_wfopen-IMAGE_BASE,h_fseek-IMAGE_BASE,h_ftell-IMAGE_BASE,h_fread-IMAGE_BASE
dq h_fwrite-IMAGE_BASE,h_fclose-IMAGE_BASE,h_memcmp-IMAGE_BASE,0
times 200h-($-IMPORT_BASE) db 0
kernel_iat:
dq h_exit-IMAGE_BASE,h_console-IMAGE_BASE,h_tlsalloc-IMAGE_BASE,h_tlsget-IMAGE_BASE
dq h_tlsset-IMAGE_BASE,h_thread-IMAGE_BASE,h_wait-IMAGE_BASE,h_close-IMAGE_BASE,h_sleep-IMAGE_BASE
dq h_tempname-IMAGE_BASE,h_move-IMAGE_BASE,h_delete-IMAGE_BASE,0
crt_name db 'msvcrt.dll',0
kernel_name db 'KERNEL32.dll',0
macro hint label,name { align 2
label dw 0
db name,0 }
hint h_malloc,'malloc'
hint h_realloc,'realloc'
hint h_printf,'printf'
hint h_fflush,'fflush'
hint h_strcmp,'strcmp'
hint h_free,'free'
hint h_strlen,'strlen'
hint h_exit,'ExitProcess'
hint h_console,'SetConsoleOutputCP'
hint h_tlsalloc,'TlsAlloc'
hint h_tlsget,'TlsGetValue'
hint h_tlsset,'TlsSetValue'
hint h_thread,'CreateThread'
hint h_wait,'WaitForSingleObject'
hint h_close,'CloseHandle'
hint h_sleep,'Sleep'
hint h_wfopen,'_wfopen'
hint h_fseek,'fseek'
hint h_ftell,'ftell'
hint h_fread,'fread'
hint h_fwrite,'fwrite'
hint h_fclose,'fclose'
hint h_memcmp,'memcmp'
hint h_tempname,'GetTempFileNameW'
hint h_move,'MoveFileExW'
hint h_delete,'DeleteFileW'

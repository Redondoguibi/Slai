TK_NAME=256
TK_INT=257
TK_STRING=258
TK_NEWLINE=259
TK_INDENT=260
TK_DEDENT=261
TK_ARROW=270
TK_EQ=271
TK_NE=272
TK_LE=273
TK_GE=274
TK_ADDSET=275
TK_SUBSET=276
TK_MULSET=277
TK_DIVSET=278
TK_MODSET=279
TK_CATCH=280
TK_AND=281
TK_OR=282

N_INT=1
N_BOOL=2
N_STRING=3
N_NAME=4
N_BINARY=5
N_UNARY=6
N_CALL=7
N_MEMBER=8
N_INDEX=9
N_LIST=10
N_OBJECT=11
N_FIELD=12
N_ASSIGN=20
N_EXPR=21
N_FUNC=22
N_CLASS=23
N_RETURN=24
N_IF=25
N_BRANCH=26
N_FOR=27
N_BREAK=28
N_CONTINUE=29
N_IMPORT=30
N_ERROR=31
N_CATCH=32
N_AWAIT=33
N_PROPAGATE=34
N_PARAM=35

; AST: kind, token, value, a, b, c, next, extra, type, binding, owner, spare.
NK=0
NT=8
NV=16
NA=24
NB=32
NC=40
NN=48
NX=56
NY=64
NZ=72
NSIZE=96

; SIR instruction: opcode, destination Value*, a, b, c, d, next.
SI_CONST=1
SI_COPY=2
SI_BINARY=3
SI_UNARY=4
SI_COMPARE=5
SI_CALL=6
SI_NATIVE=7
SI_LABEL=8
SI_JUMP=9
SI_BRANCH=10
SI_RETURN=11
SI_FIELD_GET=12
SI_FIELD_SET=13
SI_ERRORCHECK=14
SI_ERRORJUMP=15
SI_ALLOC=16
SI_FADDR=17
SI_ARRAY=18

T_INT=1
T_BOOL=2
T_STRING=3
T_VOID=4
; Compound types are pointers: kind(5=list,6=object,7=class,8=task),
; element/class AST, fields, byte size, next.

; Function context: AST, params(values), result type, locals, slots,
; SIR head/tail, code buffer/size, final code RVA, next, argtypes,
; count, class type, current catch, loop continue/break, main flag.
F_AST=0
F_PARAMS=8
F_TYPE=16
F_LOCALS=24
F_SLOTS=32
F_HEAD=40
F_TAIL=48
F_CODE=56
F_SIZE=64
F_RVA=72
F_NEXT=80
F_TYPES=88
F_COUNT=96
F_CLASS=104
F_CATCH=112
F_CONTINUE=120
F_BREAK=128
F_MAIN=136
F_FRAME=144
F_COMPLETE=152
F_SIZEOF=192
; Variable: name,type,Value*,next,token. Value: type,slot,next (parameters).
; Field: name,type,offset,next,token.

macro emitbytes [value] { common
    local .start,.finish
    lea rcx,[.start]
    mov edx,.finish-.start
    call code_bytes
    jmp .finish
.start: db value
.finish:
}


.data
i : 0
__TMP0 : 0

.text
_PRINCIPAL:
    JMP MAIN
FUNC_pisca:
    LDI 9
    STO $out_port
    RETURN 0
MAIN:
WHILE0:
    LDI 3
    STO __TMP0
    LD i
    SUB __TMP0
    BGE ENDWHILE0
    CALL FUNC_pisca
    LD i
    ADDI 1
    STO __TMP0
    LD __TMP0
    STO i
    JMP WHILE0
ENDWHILE0:
    RETURN 0
    HLT 0

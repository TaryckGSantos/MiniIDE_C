#ifndef CONSTANTS_H
#define CONSTANTS_H

enum TokenId 
{
    EPSILON  = 0,
    DOLLAR   = 1,
    t_KEY_INT = 2,
    t_KEY_FLOAT = 3,
    t_KEY_IF = 4,
    t_KEY_ELSIF = 5,
    t_KEY_ELSE = 6,
    t_KEY_ENTAO = 7,
    t_KEY_SED = 8,
    t_KEY_SENAO = 9,
    t_KEY_BOOL = 10,
    t_KEY_CHAR = 11,
    t_KEY_TRUE = 12,
    t_KEY_FALSE = 13,
    t_KEY_STRING = 14,
    t_KEY_WHILE = 15,
    t_KEY_DO = 16,
    t_KEY_LONG = 17,
    t_KEY_DOUBLE = 18,
    t_KEY_FOR = 19,
    t_KEY_BREAK = 20,
    t_KEY_CASED = 21,
    t_KEY_RETURN = 22,
    t_KEY_CONST = 23,
    t_KEY_STATIC = 24,
    t_KEY_STRUCT = 25,
    t_KEY_TYPEDEF = 26,
    t_KEY_SWITCH = 27,
    t_KEY_CONTINUE = 28,
    t_KEY_ENUM = 29,
    t_KEY_VOID = 30,
    t_KEY_SIGNED = 31,
    t_KEY_UNSIGNED = 32,
    t_KEY_AUTO = 33,
    t_KEY_REGISTER = 34,
    t_KEY_GOTO = 35,
    t_KEY_SIZEOF = 36,
    t_KEY_CLASS = 37,
    t_KEY_NAMESPACE = 38,
    t_KEY_TEMPLATE = 39,
    t_KEY_TYPENAME = 40,
    t_KEY_PUBLIC = 41,
    t_KEY_PRIVATE = 42,
    t_KEY_PROTECTED = 43,
    t_KEY_NEW = 44,
    t_KEY_DELETE = 45,
    t_KEY_THIS = 46,
    t_KEY_TRY = 47,
    t_KEY_CATCH = 48,
    t_KEY_FINAL = 49,
    t_KEY_USING = 50,
    t_KEY_NULLPTR = 51,
    t_KEY_CIN = 52,
    t_KEY_COUT = 53,
    t_DELIM_PONTOVIRGULA = 54,
    t_DELIM_VIRGULA = 55,
    t_DELIM_PONTO = 56,
    t_DELIM_COLCHETESE = 57,
    t_DELIM_COLCHETESD = 58,
    t_DELIM_PARENTESESD = 59,
    t_DELIM_PARENTESESE = 60,
    t_DELIM_CHAVEE = 61,
    t_DELIM_CHAVED = 62,
    t_OPA_SUM = 63,
    t_OPA_SUM1 = 64,
    t_OPA_SUB = 65,
    t_OPA_SUB1 = 66,
    t_OPA_MUL = 67,
    t_OPA_DIV = 68,
    t_OPA_MOD = 69,
    t_OPR_IGUAL = 70,
    t_OPR_DIFERENTE = 71,
    t_OPR_MAIOR_IGUAL = 72,
    t_OPR_MENOR_IGUAL = 73,
    t_OPR_MAIOR = 74,
    t_OPR_MENOR = 75,
    t_OPR_ATRIB = 76,
    t_OPL_AND = 77,
    t_OPL_OR = 78,
    t_OPL_DIFF = 79,
    t_OPBB_DE = 80,
    t_OPBB_DD = 81,
    t_OPBB_AND = 82,
    t_OPBB_OR = 83,
    t_OPBB_NOT = 84,
    t_OPBB_XOR = 85,
    t_LIT_DECIMAIS = 86,
    t_LIT_INTEIRO = 87,
    t_STRING = 88,
    t_HEXADECIMAL = 89,
    t_BINARIO = 90,
    t_CHAR = 91,
    t_ID = 92,
    t_COMENT_LINHA = 93,
    t_COMENT_BLOCO = 94
};

const int STATES_COUNT = 266;

extern int SCANNER_TABLE[STATES_COUNT][256];

extern int TOKEN_STATE[STATES_COUNT];

extern const char *SCANNER_ERROR[STATES_COUNT];

const int FIRST_SEMANTIC_ACTION = 151;

const int SHIFT  = 0;
const int REDUCE = 1;
const int ACTION = 2;
const int ACCEPT = 3;
const int GO_TO  = 4;
const int ERROR  = 5;

extern const int PARSER_TABLE[359][175][2];

extern const int PRODUCTIONS[159][2];

extern const char *PARSER_ERROR[359];

#endif

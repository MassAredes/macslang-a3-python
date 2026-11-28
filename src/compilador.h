#ifndef COMPILADOR_H
#define COMPILADOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Enumeração completa das palavras reservadas e dos símbolos reconhecidos pelo
 * lexer. Símbolos simples, como '+', usam diretamente o seu valor ASCII. */
enum ClasseToken {
    TK_FIM = 0, TK_NOME = 256, TK_INTEIRO, TK_REAL, TK_CHAR, TK_TEXTO,
    TK_VAR, TK_FUNC, TK_RETURN, TK_IF, TK_ELSE, TK_WHILE, TK_FOR,
    TK_PRINT, TK_INPUT, TK_INT, TK_FLOAT, TK_TIPO_CHAR, TK_BOOL,
    TK_STRING, TK_TRUE, TK_FALSE, TK_IGUAL, TK_DIF, TK_MENOR_IGUAL,
    TK_MAIOR_IGUAL, TK_E, TK_OU
};

typedef enum { T_NENHUM, T_INT, T_FLOAT, T_CHAR, T_BOOL, T_STRING } Tipo;

typedef struct {
    int classe;
    const char *texto;
    const char *valor;           /* String ou char após interpretar escapes. */
    size_t linha, coluna;
} Token;

typedef enum { E_LITERAL, E_VARIAVEL, E_CHAMADA, E_UNARIO, E_BINARIO } ClasseExpr;

typedef struct Expr {
    ClasseExpr classe;
    const Token *token;
    Tipo tipo;                   /* Preenchido na análise semântica. */
    bool promover_real;          /* Conversão int -> float autorizada pela semântica. */
    unsigned id;                 /* Número usado no nome C. */
    unsigned altura;             /* Limita árvores muito profundas. */
    int64_t inteiro;
    double real;
    struct Expr *esq, *dir;       /* O unário usa esq, a chamada usa esq para os argumentos. */
    struct Expr *prox;           /* Próximo argumento, quando houver. */
} Expr;

typedef enum {
    C_BLOCO, C_VAR, C_ATRIB, C_PRINT, C_INPUT, C_RETURN, C_CHAMADA,
    C_IF, C_WHILE, C_FOR
} ClasseCmd;

typedef struct Cmd {
    ClasseCmd classe;
    const Token *token;
    const Token *nome;           /* Usado pela declaração de variável. */
    Tipo tipo;
    unsigned id;
    Expr *expr;                  /* Valor de uma instrução ou condição de um controle. */
    Expr *destino;               /* Referência em input e atribuição. */
    struct Cmd *corpo, *alternativa;
    struct Cmd *inicio, *passo;   /* Trechos do cabeçalho do for. */
    struct Cmd *prox;
} Cmd;

typedef struct Parametro {
    const Token *nome;
    Tipo tipo;
    unsigned id;
    struct Parametro *prox;
} Parametro;

typedef struct Funcao {
    const Token *nome;
    Tipo retorno;
    unsigned id;
    Parametro *params;
    Cmd *corpo;
    struct Funcao *prox;
} Funcao;

typedef struct {
    Funcao *funcoes;
    Cmd *comandos;
} Programa;

void iniciar_memoria(void);
void *alocar(size_t quantidade, size_t tamanho);
char *copiar_texto(const char *inicio, size_t tamanho);
__declspec(noreturn) void falha(const char *mensagem);
__declspec(noreturn) void erro(const char *fase, const Token *token, const char *formato, ...);
const char *nome_tipo(Tipo tipo);
const char *nome_token(int classe);

/* Cada fase tem uma função pública e seu próprio arquivo de implementação. */
Token *lexar(const char *fonte);
Programa analisar_sintaxe(const Token *tokens);
void analisar_semantica(Programa *programa);
void gerar_c(FILE *saida, const Programa *programa);

void mostrar_arvore(const Programa *programa);
void mostrar_texto(FILE *saida, const char *texto, char aspas);

#endif

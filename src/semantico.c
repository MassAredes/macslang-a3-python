#include "compilador.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Um símbolo liga o nome escrito pelo usuário ao seu tipo e ao identificador
 * numérico usado pelo gerador. Cada escopo guarda apenas os símbolos locais. */
typedef struct Simbolo {
    const char *nome;
    Tipo tipo;
    unsigned id;
    struct Simbolo *prox;
} Simbolo;

typedef struct Escopo {
    Simbolo *simbolos;
    struct Escopo *fora;
} Escopo;

typedef struct {
    const Funcao *funcoes;
    Escopo *escopo;
    Tipo retorno;
    unsigned contador;
} Semantico;

static Tipo verificar_expr(Semantico *s, Expr *expr);
static bool verificar_cmd(Semantico *s, Cmd *cmd);

static unsigned declarar(Semantico *s, const Token *nome, Tipo tipo)
{
    for (const Simbolo *p = s->escopo->simbolos; p; p = p->prox)
        if (strcmp(p->nome, nome->texto) == 0)
            erro("semântico", nome, "variável '%s' já declarada neste escopo", nome->texto);

    Simbolo *simbolo = alocar(1, sizeof *simbolo);
    simbolo->nome = nome->texto;
    simbolo->tipo = tipo;
    simbolo->id = ++s->contador;
    simbolo->prox = s->escopo->simbolos;
    s->escopo->simbolos = simbolo;
    return simbolo->id;
}

static Tipo resolver_variavel(Semantico *s, Expr *expr)
{
    /* A busca começa nas chaves mais internas e segue para fora. */
    for (const Escopo *e = s->escopo; e; e = e->fora) {
        for (const Simbolo *p = e->simbolos; p; p = p->prox) {
            if (strcmp(p->nome, expr->token->texto) == 0) {
                expr->id = p->id;
                return expr->tipo = p->tipo;
            }
        }
    }
    erro("semântico", expr->token, "variável '%s' não declarada", expr->token->texto);
}

static bool numero(Tipo tipo)
{
    return tipo == T_INT || tipo == T_FLOAT;
}

static void conferir_valor(Semantico *s, Tipo destino, Expr *expr)
{
    Tipo origem = verificar_expr(s, expr);
    if (origem != destino && !(origem == T_INT && destino == T_FLOAT))
        erro("semântico", expr->token, "não é possível passar um valor %s para %s",
             nome_tipo(origem), nome_tipo(destino));
    if (origem == T_INT && destino == T_FLOAT)
        expr->promover_real = true;
}

static void conferir_condicao(Semantico *s, Expr *expr)
{
    if (verificar_expr(s, expr) != T_BOOL)
        erro("semântico", expr->token, "a condição precisa ser bool");
}

static Tipo verificar_literal(Expr *expr)
{
    const Token *token = expr->token;
    switch (token->classe) {
    case TK_INTEIRO: {
        errno = 0;
        intmax_t valor = strtoimax(token->texto, NULL, 10);
        if (errno == ERANGE || valor > INT64_MAX)
            erro("semântico", token, "literal int fora da faixa de 64 bits");
        expr->inteiro = (int64_t)valor;
        return T_INT;
    }
    case TK_REAL:
        expr->real = strtod(token->texto, NULL);
        if (!isfinite(expr->real))
            erro("semântico", token, "literal float fora da faixa");
        return T_FLOAT;
    case TK_CHAR: return T_CHAR;
    case TK_TEXTO: return T_STRING;
    case TK_TRUE: case TK_FALSE: return T_BOOL;
    default: falha("literal desconhecido");
    }
}

static Tipo verificar_chamada(Semantico *s, Expr *expr)
{
    const Funcao *funcao = s->funcoes;
    while (funcao && strcmp(funcao->nome->texto, expr->token->texto) != 0)
        funcao = funcao->prox;
    if (!funcao)
        erro("semântico", expr->token, "função '%s' não declarada", expr->token->texto);

    const Parametro *param = funcao->params;
    Expr *arg = expr->esq;
    for (; param && arg; param = param->prox, arg = arg->prox)
        conferir_valor(s, param->tipo, arg);
    if (param || arg)
        erro("semântico", expr->token, "quantidade de argumentos incorreta em '%s'",
             expr->token->texto);

    expr->id = funcao->id;
    return funcao->retorno;
}

static Tipo verificar_binario(Semantico *s, Expr *expr)
{
    Tipo esq = verificar_expr(s, expr->esq);
    Tipo dir = verificar_expr(s, expr->dir);
    int op = expr->token->classe;
    bool numericos = numero(esq) && numero(dir);
    if (numericos && esq != dir) {
        expr->esq->promover_real = esq == T_INT;
        expr->dir->promover_real = dir == T_INT;
    }

    if (op == '+' && (esq == T_STRING || dir == T_STRING))
        return T_STRING;
    if ((op == '+' || op == '-' || op == '*' || op == '/') && numericos)
        return esq == T_FLOAT || dir == T_FLOAT ? T_FLOAT : T_INT;
    if (op == '%' && esq == T_INT && dir == T_INT)
        return T_INT;
    if ((op == '<' || op == '>' || op == TK_MENOR_IGUAL || op == TK_MAIOR_IGUAL) && numericos)
        return T_BOOL;
    if ((op == TK_IGUAL || op == TK_DIF) && (esq == dir || numericos))
        return T_BOOL;
    if ((op == TK_E || op == TK_OU) && esq == T_BOOL && dir == T_BOOL)
        return T_BOOL;
    erro("semântico", expr->token, "operador '%s' não aceita %s e %s",
         expr->token->texto, nome_tipo(esq), nome_tipo(dir));
}

static Tipo verificar_expr(Semantico *s, Expr *expr)
{
    Tipo tipo;
    switch (expr->classe) {
    case E_LITERAL: tipo = verificar_literal(expr); break;
    case E_VARIAVEL: tipo = resolver_variavel(s, expr); break;
    case E_CHAMADA: tipo = verificar_chamada(s, expr); break;
    case E_BINARIO: tipo = verificar_binario(s, expr); break;
    case E_UNARIO:
        tipo = verificar_expr(s, expr->esq);
        if ((expr->token->classe == '!' && tipo != T_BOOL) ||
            (expr->token->classe == '-' && !numero(tipo)))
            erro("semântico", expr->token, "operador '%s' não aceita %s",
                 expr->token->texto, nome_tipo(tipo));
        break;
    default:
        falha("expressão desconhecida");
    }
    return expr->tipo = tipo;
}

static bool verificar_lista(Semantico *s, Cmd *cmd)
{
    bool retorna = false;
    for (; cmd; cmd = cmd->prox) {
        /* Continuamos a análise até o fim para diagnosticar cada trecho lido. */
        if (verificar_cmd(s, cmd))
            retorna = true;
    }
    return retorna;
}

static bool verificar_cmd(Semantico *s, Cmd *cmd)
{
    switch (cmd->classe) {
    case C_BLOCO: {
        Escopo bloco = {NULL, s->escopo};
        s->escopo = &bloco;
        bool retorna = verificar_lista(s, cmd->corpo);
        s->escopo = bloco.fora;
        return retorna;
    }
    case C_VAR:
        if (cmd->expr)
            conferir_valor(s, cmd->tipo, cmd->expr);
        cmd->id = declarar(s, cmd->nome, cmd->tipo);
        break;
    case C_ATRIB:
        conferir_valor(s, resolver_variavel(s, cmd->destino), cmd->expr);
        break;
    case C_INPUT:
        resolver_variavel(s, cmd->destino);
        break;
    case C_PRINT: case C_CHAMADA:
        verificar_expr(s, cmd->expr);
        break;
    case C_RETURN:
        if (s->retorno == T_NENHUM)
            erro("semântico", cmd->token, "return precisa estar dentro de uma função");
        conferir_valor(s, s->retorno, cmd->expr);
        return true;
    case C_IF: {
        conferir_condicao(s, cmd->expr);
        bool retorna = verificar_cmd(s, cmd->corpo);
        if (cmd->alternativa) {
            bool alternativa = verificar_cmd(s, cmd->alternativa);
            return retorna && alternativa;
        }
        break;
    }
    case C_WHILE:
        conferir_condicao(s, cmd->expr);
        verificar_cmd(s, cmd->corpo);
        break;
    case C_FOR: {
        Escopo laco = {NULL, s->escopo};
        s->escopo = &laco;
        if (cmd->inicio)
            verificar_cmd(s, cmd->inicio);
        if (cmd->expr)
            conferir_condicao(s, cmd->expr);
        if (cmd->passo)
            verificar_cmd(s, cmd->passo);
        verificar_cmd(s, cmd->corpo);
        s->escopo = laco.fora;
        break;
    }
    }
    /* Um laço pode executar zero vezes, por isso não garante retorno. */
    return false;
}

void analisar_semantica(Programa *programa)
{
    Semantico sem = {programa->funcoes, NULL, T_NENHUM, 0};
    unsigned id = 0;

    /* Numeramos todas as assinaturas antes de conferir os corpos. */
    for (Funcao *f = programa->funcoes; f; f = f->prox) {
        for (const Funcao *p = programa->funcoes; p != f; p = p->prox)
            if (strcmp(p->nome->texto, f->nome->texto) == 0)
                erro("semântico", f->nome, "função '%s' já declarada", f->nome->texto);
        f->id = ++id;
    }
    for (Funcao *f = programa->funcoes; f; f = f->prox) {
        Escopo escopo = {0};
        sem.escopo = &escopo;
        sem.retorno = f->retorno;
        for (Parametro *p = f->params; p; p = p->prox)
            p->id = declarar(&sem, p->nome, p->tipo);
        if (!verificar_cmd(&sem, f->corpo))
            erro("semântico", f->nome, "há caminho na função sem return");
    }
    Escopo principal = {0};
    sem.escopo = &principal;
    sem.retorno = T_NENHUM;
    verificar_lista(&sem, programa->comandos);
}

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

/* Função static: só pode ser usada neste arquivo e devolve o identificador de
 * uma variável criada no escopo atual. */
static unsigned declarar(Semantico *s, const Token *nome, Tipo tipo)
{
    /* Percorre todos os nomes já declarados no bloco atual. */
    for (const Simbolo *p = s->escopo->simbolos; p; p = p->prox)
        /* Se encontrou o mesmo nome, interrompe a compilação. */
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

/* Função static: procura uma variável em todos os escopos visíveis e grava
 * o seu tipo e identificador diretamente no nó da expressão. */
static Tipo resolver_variavel(Semantico *s, Expr *expr)
{
    /* Primeiro for: começa no escopo interno e avança para os escopos externos. */
    for (const Escopo *e = s->escopo; e; e = e->fora) {
        /* Segundo for: examina cada variável armazenada no escopo atual. */
        for (const Simbolo *p = e->simbolos; p; p = p->prox) {
            /* Se o nome coincide, esta é a variável que a expressão usa. */
            if (strcmp(p->nome, expr->token->texto) == 0) {
                expr->id = p->id;
                return expr->tipo = p->tipo;
            }
        }
    }
    /* Nenhum escopo possuía esse nome; informa o erro e encerra a compilação. */
    erro("semântico", expr->token, "variável '%s' não declarada", expr->token->texto);
}

/* Função static: diz se o tipo representa um número inteiro ou decimal. */
static bool numero(Tipo tipo)
{
    return tipo == T_INT || tipo == T_FLOAT;
}

/* Função static sem resultado: verifica um valor antes de atribuí-lo a um destino. */
static void conferir_valor(Semantico *s, Tipo destino, Expr *expr)
{
    Tipo origem = verificar_expr(s, expr);
    /* Se os tipos são diferentes e não é a conversão permitida int -> float,
     * o valor não pode ser usado no destino escolhido. */
    if (origem != destino && !(origem == T_INT && destino == T_FLOAT))
        erro("semântico", expr->token, "não é possível passar um valor %s para %s",
             nome_tipo(origem), nome_tipo(destino));
    /* O segundo if marca que o gerador deverá escrever uma conversão explícita. */
    if (origem == T_INT && destino == T_FLOAT)
        expr->promover_real = true;
}

/* Função static sem resultado: exige que uma condição produza um booleano. */
static void conferir_condicao(Semantico *s, Expr *expr)
{
    /* Um if aqui impede condições como números ou textos em if e while. */
    if (verificar_expr(s, expr) != T_BOOL)
        erro("semântico", expr->token, "a condição precisa ser bool");
}

/* Função static: converte um literal de texto em um valor de tipo conhecido. */
static Tipo verificar_literal(Expr *expr)
{
    const Token *token = expr->token;
    switch (token->classe) {
    case TK_INTEIRO: {
        errno = 0;
        intmax_t valor = strtoimax(token->texto, NULL, 10);
        /* Rejeita números maiores que a faixa aceita ou que sofreram overflow. */
        if (errno == ERANGE || valor > INT64_MAX)
            erro("semântico", token, "literal int fora da faixa de 64 bits");
        expr->inteiro = (int64_t)valor;
        return T_INT;
    }
    case TK_REAL:
        expr->real = strtod(token->texto, NULL);
        /* Rejeita infinito ou outro resultado real inválido. */
        if (!isfinite(expr->real))
            erro("semântico", token, "literal float fora da faixa");
        return T_FLOAT;
    case TK_CHAR: return T_CHAR;
    case TK_TEXTO: return T_STRING;
    case TK_TRUE: case TK_FALSE: return T_BOOL;
    default: falha("literal desconhecido");
    }
}

/* Função static: localiza uma função e compara seus parâmetros com os argumentos. */
static Tipo verificar_chamada(Semantico *s, Expr *expr)
{
    const Funcao *funcao = s->funcoes;
    /* Percorre a lista de funções até encontrar o nome informado. */
    while (funcao && strcmp(funcao->nome->texto, expr->token->texto) != 0)
        funcao = funcao->prox;
    /* if avisa quando não existe uma função com esse nome. */
    if (!funcao)
        erro("semântico", expr->token, "função '%s' não declarada", expr->token->texto);

    const Parametro *param = funcao->params;
    Expr *arg = expr->esq;
    /* for avançado percorre parâmetros e argumentos dois a dois. */
    for (; param && arg; param = param->prox, arg = arg->prox)
        conferir_valor(s, param->tipo, arg);
    /* if detecta quantidade diferente de parâmetros e argumentos. */
    if (param || arg)
        erro("semântico", expr->token, "quantidade de argumentos incorreta em '%s'",
             expr->token->texto);

    expr->id = funcao->id;
    return funcao->retorno;
}

/* Função static: verifica os dois lados e aplica regras aos operadores +, -, *, /
 * e aos operadores que produzem comparações ou resultados booleanos. */
static Tipo verificar_binario(Semantico *s, Expr *expr)
{
    Tipo esq = verificar_expr(s, expr->esq);
    Tipo dir = verificar_expr(s, expr->dir);
    int op = expr->token->classe;
    bool numericos = numero(esq) && numero(dir);

    /* if: somente quando os dois tipos são numéricos, mas diferentes. */
    if (numericos && esq != dir) {
        /* Marca int como conversão para float antes de gerar o C. */
        expr->esq->promover_real = esq == T_INT;
        expr->dir->promover_real = dir == T_INT;
    }

    /* if: soma com qualquer lado string usa concatenação em vez de soma numérica. */
    if (op == '+' && (esq == T_STRING || dir == T_STRING))
        return T_STRING;
    /* if: estas operações exigem que os dois lados sejam números. */
    if ((op == '+' || op == '-' || op == '*' || op == '/') && numericos)
        return esq == T_FLOAT || dir == T_FLOAT ? T_FLOAT : T_INT;
    /* if: o resto da divisão só é permitido para dois inteiros. */
    if (op == '%' && esq == T_INT && dir == T_INT)
        return T_INT;
    /* if: comparações ordenadas entre números produzem bool. */
    if ((op == '<' || op == '>' || op == TK_MENOR_IGUAL || op == TK_MAIOR_IGUAL) && numericos)
        return T_BOOL;
    /* if: igualdade ou diferença exige tipos iguais, ou dois tipos numéricos. */
    if ((op == TK_IGUAL || op == TK_DIF) && (esq == dir || numericos))
        return T_BOOL;
    /* if: && e || recebem apenas dois valores booleanos. */
    if ((op == TK_E || op == TK_OU) && esq == T_BOOL && dir == T_BOOL)
        return T_BOOL;
    erro("semântico", expr->token, "operador '%s' não aceita %s e %s",
         expr->token->texto, nome_tipo(esq), nome_tipo(dir));
}

/* Função static: percorre uma expressão e devolve o tipo que ela produzirá. */
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
        /* if: ! exige bool e - exige um tipo numérico. */
        if ((expr->token->classe == '!' && tipo != T_BOOL) ||
            (expr->token->classe == '-' && !numero(tipo)))
            erro("semântico", expr->token, "operador '%s' não aceita %s",
                 expr->token->texto, nome_tipo(tipo));
        break;
    default:
        /* else de um switch não existe; este default cobre classes desconhecidas. */
        falha("expressão desconhecida");
    }
    return expr->tipo = tipo;
}

/* Função static: percorre uma lista ligada de comandos, um após o outro. */
static bool verificar_lista(Semantico *s, Cmd *cmd)
{
    bool retorna = false;
    /* for: enquanto ainda existe um comando na lista, avança pelo campo prox. */
    for (; cmd; cmd = cmd->prox) {
        /* Continuamos a análise até o fim para diagnosticar cada trecho lido. */
        /* if: registra que pelo menos um comando garante um retorno. */
        if (verificar_cmd(s, cmd))
            retorna = true;
    }
    return retorna;
}

/* Função static: interpreta um comando e devolve verdadeiro apenas quando ele
 * garante que a função alcançará uma instrução return. */
static bool verificar_cmd(Semantico *s, Cmd *cmd)
{
    switch (cmd->classe) {
    case C_BLOCO: {
        /* Cria um novo escopo ligado ao escopo externo. */
        Escopo bloco = {NULL, s->escopo};
        s->escopo = &bloco;
        bool retorna = verificar_lista(s, cmd->corpo);
        /* Restaura o escopo externo ao sair das chaves. */
        s->escopo = bloco.fora;
        return retorna;
    }
    case C_VAR:
        /* if: só verifica o valor inicial quando ele existe. */
        if (cmd->expr)
            conferir_valor(s, cmd->tipo, cmd->expr);
        cmd->id = declarar(s, cmd->nome, cmd->tipo);
        break;
    case C_ATRIB:
        /* Primeiro resolve o destino e depois confere o valor atribuído. */
        conferir_valor(s, resolver_variavel(s, cmd->destino), cmd->expr);
        break;
    case C_INPUT:
        /* Resolve o nome informado para input e confirma que ele existe. */
        resolver_variavel(s, cmd->destino);
        break;
    case C_PRINT: case C_CHAMADA:
        verificar_expr(s, cmd->expr);
        break;
    case C_RETURN:
        /* if: return não pode aparecer fora de uma função. */
        if (s->retorno == T_NENHUM)
            erro("semântico", cmd->token, "return precisa estar dentro de uma função");
        /* Confere se o valor coincide com o tipo declarado pela função. */
        conferir_valor(s, s->retorno, cmd->expr);
        return true;
    case C_IF: {
        /* Exige que a condição do if seja booleana. */
        conferir_condicao(s, cmd->expr);
        bool retorna = verificar_cmd(s, cmd->corpo);
        /* if/else: somente garante retorno quando também existe else. */
        if (cmd->alternativa) {
            bool alternativa = verificar_cmd(s, cmd->alternativa);
            return retorna && alternativa;
        }
        /* Sem else, o corpo pode não executar e por isso não garante retorno. */
        break;
    }
    case C_WHILE:
        /* if interno de conferir_condicao exige uma condição bool. */
        conferir_condicao(s, cmd->expr);
        verificar_cmd(s, cmd->corpo);
        break;
    case C_FOR: {
        /* Cria um escopo próprio para a variável do cabeçalho do for. */
        Escopo laco = {NULL, s->escopo};
        s->escopo = &laco;
        /* if: verifica a parte inicial somente se ela foi escrita. */
        if (cmd->inicio)
            verificar_cmd(s, cmd->inicio);
        /* if: verifica a condição somente se ela não estiver vazia. */
        if (cmd->expr)
            conferir_condicao(s, cmd->expr);
        /* if: verifica o passo somente se ele foi escrito. */
        if (cmd->passo)
            verificar_cmd(s, cmd->passo);
        verificar_cmd(s, cmd->corpo);
        /* Ao sair do for, restaura o escopo externo. */
        s->escopo = laco.fora;
        break;
    }
    }
    /* Um laço ou um if sem else pode executar zero vezes; por isso não garante return. */
    return false;
}

/* Função publicamente disponível neste arquivo: inicia toda a análise semântica.
 * Ela não é static, pois pode ser chamada pelos outros módulos do compilador. */
void analisar_semantica(Programa *programa)
{
    Semantico sem = {programa->funcoes, NULL, T_NENHUM, 0};
    unsigned id = 0;

    /* Este for externo percorre todas as funções declaradas pelo programa. */
    for (Funcao *f = programa->funcoes; f; f = f->prox) {
        /* Este for interno verifica funções anteriores e impede nomes duplicados. */
        for (const Funcao *p = programa->funcoes; p != f; p = p->prox)
            /* if: compara o nome atual com cada nome já registrado. */
            if (strcmp(p->nome->texto, f->nome->texto) == 0)
                erro("semântico", f->nome, "função '%s' já declarada", f->nome->texto);
        f->id = ++id;
    }

    /* Segundo for externo: verifica o corpo de cada função em seu próprio escopo. */
    for (Funcao *f = programa->funcoes; f; f = f->prox) {
        Escopo escopo = {0};
        sem.escopo = &escopo;
        sem.retorno = f->retorno;

        /* for: declara todos os parâmetros como variáveis locais da função. */
        for (Parametro *p = f->params; p; p = p->prox)
            p->id = declarar(&sem, p->nome, p->tipo);

        /* if: exige um caminho de return em todas as situações da função. */
        if (!verificar_cmd(&sem, f->corpo))
            erro("semântico", f->nome, "há caminho na função sem return");
    }
    Escopo principal = {0};
    sem.escopo = &principal;
    sem.retorno = T_NENHUM;
    verificar_lista(&sem, programa->comandos);
}

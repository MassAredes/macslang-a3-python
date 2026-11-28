#include "compilador.h"

#include <inttypes.h>
#include <string.h>

/* tabelas traduzem os tipos internos do compilador para tipos C e valores iniciais.
 * Cada posição corresponde ao mesmo Tipo definido em compilador.h. */
static const char *tipos_c[] = {"", "int64_t", "double", "char", "bool", "const char *"};
static const char *iniciais[] = {"", "INT64_C(0)", "0.0", "0", "false", "\"\""};

static void expressao(FILE *saida, const Expr *expr);
static void comando(FILE *saida, const Cmd *cmd, unsigned nivel);

/* static void recuar: escreve espaços conforme o nível de cada bloco C. */
static void recuar(FILE *saida, unsigned nivel)
{
    /* for: repete a escrita de quatro espaços para cada nível. */
    for (unsigned i = 0; i < nivel; ++i)
        fputs("    ", saida);
}

/* static void string_c: protege aspas, barras e caracteres especiais em string C. */
static void string_c(FILE *saida, const char *valor)
{
    fputc('"', saida);
    /* for: percorre cada caractere até encontrar o terminador '\\0'. */
    for (; *valor; ++valor) {
        unsigned char c = (unsigned char)*valor;
        /* if: aspas, barras ou ? precisam de barra de escape. */
        if (c == '"' || c == '\\' || c == '?') {
            fputc('\\', saida);
            fputc(c, saida);
        /* else if: converte caracteres de controle em escapes conhecidos. */
        } else if (c == '\n') {
            fputs("\\n", saida);
        } else if (c == '\r') {
            fputs("\\r", saida);
        } else if (c == '\t') {
            fputs("\\t", saida);
        /* else if: imprime caracteres não visíveis em formato octal. */
        } else if (c < 32 || c == 127) {
            fprintf(saida, "\\%03o", (unsigned)c);
        /* else: caracteres normais podem ser escritos diretamente. */
        } else {
            fputc(c, saida);
        }
    }
    fputc('"', saida);
}

static void literal(FILE *saida, const Expr *expr)
{
    switch (expr->tipo) {
    case T_INT:
        fprintf(saida, "INT64_C(%" PRId64 ")", expr->inteiro);
        break;
    case T_FLOAT: {
        char buffer[40];
        snprintf(buffer, sizeof buffer, "%.17g", expr->real);
        fputs(buffer, saida);
        /* Em C, 2 é inteiro e 2.0 é real. Preservamos o tipo até aqui. */
        if (!strpbrk(buffer, ".eE"))
            fputs(".0", saida);
        break;
    }
    case T_BOOL:
        fputs(expr->token->classe == TK_TRUE ? "true" : "false", saida);
        break;
    case T_CHAR:
        fprintf(saida, "((char)%u)", (unsigned)(unsigned char)expr->token->valor[0]);
        break;
    case T_STRING:
        string_c(saida, expr->token->valor);
        break;
    default:
        falha("tipo desconhecido ao gerar literal");
    }
}

static void como_texto(FILE *saida, const Expr *expr)
{
    if (expr->tipo != T_STRING)
        fprintf(saida, "macs_texto_%s(", nome_tipo(expr->tipo));
    expressao(saida, expr);
    if (expr->tipo != T_STRING)
        fputc(')', saida);
}

static void binario(FILE *saida, const Expr *expr)
{
    int op = expr->token->classe;
    if (expr->tipo == T_STRING) {
        fputs("macs_juntar(", saida);
        como_texto(saida, expr->esq);
        fputs(", ", saida);
        como_texto(saida, expr->dir);
        fputc(')', saida);
    } else if ((op == TK_IGUAL || op == TK_DIF) && expr->esq->tipo == T_STRING) {
        fputs("(strcmp(", saida);
        expressao(saida, expr->esq);
        fputs(", ", saida);
        expressao(saida, expr->dir);
        fprintf(saida, ") %s 0)", expr->token->texto);
    } else {
        /* A árvore já contém a precedência. Repetimos a ordem com parênteses. */
        fputc('(', saida);
        expressao(saida, expr->esq);
        fprintf(saida, " %s ", expr->token->texto);
        expressao(saida, expr->dir);
        fputc(')', saida);
    }
}

/* static void expressao: escolhe como escrever cada tipo de nó da AST em C. */
static void expressao(FILE *saida, const Expr *expr)
{
    /* if: envolve o inteiro em conversão para double quando a semântica marcou. */
    if (expr->promover_real)
        fputs("((double)", saida);

    switch (expr->classe) {
    case E_LITERAL:
        literal(saida, expr);
        break;
    case E_VARIAVEL:
        fprintf(saida, "macs_var_%u", expr->id);
        break;
    case E_CHAMADA:
        fprintf(saida, "macs_funcao_%u(", expr->id);
        /* for: escreve todos os argumentos separados por vírgula. */
        for (const Expr *arg = expr->esq; arg; arg = arg->prox) {
            expressao(saida, arg);
            /* if: a vírgula só aparece quando ainda existe argumento seguinte. */
            if (arg->prox)
                fputs(", ", saida);
        }
        fputc(')', saida);
        break;
    case E_UNARIO:
        fprintf(saida, "(%s", expr->token->texto);
        expressao(saida, expr->esq);
        fputc(')', saida);
        break;
    case E_BINARIO:
        binario(saida, expr);
        break;
    }

    /* if: fecha o parêntese da conversão iniciada no começo desta função. */
    if (expr->promover_real)
        fputc(')', saida);
}

/* static void declaracao_ou_atribuicao: escreve uma declaração ou uma atribuição C. */
static void declaracao_ou_atribuicao(FILE *saida, const Cmd *cmd)
{
    /* if: um trecho nulo não precisa ser escrito. */
    if (!cmd)
        return;

    /* if/else: declaração inclui tipo e nome; atribuição inclui apenas o destino. */
    if (cmd->classe == C_VAR) {
        fprintf(saida, "%s macs_var_%u = ", tipos_c[cmd->tipo], cmd->id);
        /* if/else: usa o inicializador escrito ou o valor padrão do tipo. */
        if (cmd->expr)
            expressao(saida, cmd->expr);
        else
            fputs(iniciais[cmd->tipo], saida);
    } else {
        fprintf(saida, "macs_var_%u = ", cmd->destino->id);
        expressao(saida, cmd->expr);
    }
}

static void imprimir(FILE *saida, const Expr *expr)
{
    switch (expr->tipo) {
    case T_BOOL: case T_STRING:
        fputs("puts(", saida);
        como_texto(saida, expr);
        fputs(");\n", saida);
        return;
    case T_INT: fputs("printf(\"%\" PRId64 \"\\n\", ", saida); break;
    case T_FLOAT: fputs("printf(\"%.15g\\n\", ", saida); break;
    case T_CHAR: fputs("printf(\"%c\\n\", ", saida); break;
    default: falha("tipo desconhecido ao gerar print");
    }
    expressao(saida, expr);
    fputs(");\n", saida);
}

static void bloco(FILE *saida, const Cmd *cmd, unsigned nivel)
{
    recuar(saida, nivel);
    fputs("{\n", saida);
    for (const Cmd *p = cmd->corpo; p; p = p->prox)
        comando(saida, p, nivel + 1);
    recuar(saida, nivel);
    fputs("}\n", saida);
}

/* static void comando: traduz um comando da AST para uma ou mais linhas C. */
static void comando(FILE *saida, const Cmd *cmd, unsigned nivel)
{
    /* if/else: bloco já possui uma rotina própria e não entra no switch. */
    if (cmd->classe == C_BLOCO) {
        bloco(saida, cmd, nivel);
        return;
    }
    recuar(saida, nivel);

    switch (cmd->classe) {
    case C_VAR: case C_ATRIB:
        declaracao_ou_atribuicao(saida, cmd);
        fputs(";\n", saida);
        break;
    case C_PRINT:
        imprimir(saida, cmd->expr);
        break;
    case C_INPUT:
        /* Lê o tipo adequado e atribui o resultado à variável indicada. */
        fprintf(saida, "macs_var_%u = macs_ler_%s();\n",
                cmd->destino->id, nome_tipo(cmd->destino->tipo));
        break;
    case C_RETURN:
        fputs("return ", saida);
        expressao(saida, cmd->expr);
        fputs(";\n", saida);
        break;
    case C_CHAMADA:
        expressao(saida, cmd->expr);
        fputs(";\n", saida);
        break;
    case C_IF: case C_WHILE:
        /* if/else da expressão ternária escolhe if (...) ou while (...) no C. */
        fputs(cmd->classe == C_IF ? "if (" : "while (", saida);
        expressao(saida, cmd->expr);
        fputs(")\n", saida);
        bloco(saida, cmd->corpo, nivel);
        /* if: escreve else somente quando a AST possui ramo alternativo. */
        if (cmd->alternativa) {
            recuar(saida, nivel);
            fputs("else\n", saida);
            bloco(saida, cmd->alternativa, nivel);
        }
        break;
    case C_FOR:
        fputs("for (", saida);
        declaracao_ou_atribuicao(saida, cmd->inicio);
        fputs("; ", saida);
        /* if: mantém a condição vazia quando o for do MACSLang não a escreveu. */
        if (cmd->expr)
            expressao(saida, cmd->expr);
        fputs("; ", saida);
        declaracao_ou_atribuicao(saida, cmd->passo);
        fputs(")\n", saida);
        bloco(saida, cmd->corpo, nivel);
        break;
    default:
        falha("comando desconhecido no gerador");
    }
}

static void assinatura(FILE *saida, const Funcao *funcao)
{
    fprintf(saida, "%s macs_funcao_%u(", tipos_c[funcao->retorno], funcao->id);
    if (!funcao->params)
        fputs("void", saida);
    for (const Parametro *p = funcao->params; p; p = p->prox) {
        fprintf(saida, "%s macs_var_%u", tipos_c[p->tipo], p->id);
        if (p->prox)
            fputs(", ", saida);
    }
    fputc(')', saida);
}

void gerar_c(FILE *saida, const Programa *programa)
{
    fputs("/* Código C11 gerado pelo compilador MACSLang. */\n", saida);
    fputs("#include \"macs_runtime.h\"\n\n", saida);
    /* Os protótipos liberam chamadas a funções que aparecem mais abaixo. */
    for (const Funcao *f = programa->funcoes; f; f = f->prox) {
        assinatura(saida, f);
        fputs(";\n", saida);
    }
    for (const Funcao *f = programa->funcoes; f; f = f->prox) {
        fputc('\n', saida);
        assinatura(saida, f);
        fputc('\n', saida);
        bloco(saida, f->corpo, 0);
    }
    fputs("\nint main(void)\n{\n    macs_iniciar();\n", saida);
    for (const Cmd *p = programa->comandos; p; p = p->prox)
        comando(saida, p, 1);
    fputs("    return 0;\n}\n", saida);
}

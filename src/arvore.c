#include "compilador.h"

/* Esta saída ajuda a estudar a árvore antes de passar pela semântica. */
/* static void: cria recuo visual sem retornar valor. */
static void recuo(unsigned nivel)
{
    /* for: imprime dois espaços para cada nível abaixo da raiz. */
    for (unsigned i = 0; i < nivel; ++i)
        fputs("  ", stdout);
}

/* static void: visita recursivamente um nó de expressão. */
static void mostrar_expr(const Expr *expr, unsigned nivel)
{
    static const char *nomes[] = {"Literal", "Variavel", "Chamada", "Unario", "Binario"};
    /* if: não há nada para mostrar quando o ponteiro está vazio. */
    if (!expr)
        return;
    recuo(nivel);
    printf("%s ", nomes[expr->classe]);
    mostrar_texto(stdout, expr->token->texto, '\'');
    printf(" (linha %zu)\n", expr->token->linha);

    /* if/else: chamadas mostram argumentos em lista; demais nós mostram lados esquerdo e direito. */
    if (expr->classe == E_CHAMADA) {
        /* for: percorre argumentos ligados pelo campo prox. */
        for (const Expr *arg = expr->esq; arg; arg = arg->prox)
            mostrar_expr(arg, nivel + 1);
    } else {
        mostrar_expr(expr->esq, nivel + 1);
        mostrar_expr(expr->dir, nivel + 1);
    }
}

/* static void: visita recursivamente um comando da árvore. */
static void mostrar_cmd(const Cmd *cmd, unsigned nivel)
{
    static const char *nomes[] = {
        "Bloco", "Declaracao", "Atribuicao", "Saida", "Entrada", "Retorno",
        "ExecutarChamada", "Condicional", "Enquanto", "Para"
    };
    if (!cmd)
        return;
    recuo(nivel);
    printf("%s", nomes[cmd->classe]);
    if (cmd->nome)
        printf(" %s: %s", cmd->nome->texto, nome_tipo(cmd->tipo));
    printf(" (linha %zu)\n", cmd->token->linha);

    mostrar_cmd(cmd->inicio, nivel + 1);
    mostrar_expr(cmd->destino, nivel + 1);
    mostrar_expr(cmd->expr, nivel + 1);
    mostrar_cmd(cmd->passo, nivel + 1);
    for (const Cmd *p = cmd->corpo; p; p = p->prox)
        mostrar_cmd(p, nivel + 1);
    mostrar_cmd(cmd->alternativa, nivel + 1);
}

void mostrar_arvore(const Programa *programa)
{
    puts("Programa");
    for (const Funcao *f = programa->funcoes; f; f = f->prox) {
        printf("  Funcao %s: %s\n", f->nome->texto, nome_tipo(f->retorno));
        for (const Parametro *p = f->params; p; p = p->prox)
            printf("    Parametro %s: %s\n", p->nome->texto, nome_tipo(p->tipo));
        mostrar_cmd(f->corpo, 2);
    }
    for (const Cmd *p = programa->comandos; p; p = p->prox)
        mostrar_cmd(p, 1);
}

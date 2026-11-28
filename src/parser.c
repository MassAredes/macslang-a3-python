#include "compilador.h"

/* atual aponta para o próximo token ainda não consumido; profundidade limita
 * expressões e blocos excessivamente aninhados para evitar estouro da pilha. */
typedef struct {
    const Token *atual;
    unsigned profundidade;
} Parser;

static Expr *expressao(Parser *p);
static Cmd *comando(Parser *p);

static const Token *avancar(Parser *p)
{
    return p->atual++;
}

/* static bool: verifica uma categoria opcional e só avança quando ela coincide. */
static bool aceitar(Parser *p, int classe)
{
    /* if: sem correspondência, não altera o ponteiro atual. */
    if (p->atual->classe != classe)
        return false;
    avancar(p);
    return true;
}

/* static const Token *: exige uma categoria obrigatória e reporta a encontrada. */
static const Token *exigir(Parser *p, int classe)
{
    /* if: interrompe o parser quando uma parte obrigatória está ausente. */
    if (p->atual->classe != classe)
        erro("sintático", p->atual, "esperado '%s', encontrado %s",
             nome_token(classe), p->atual->classe == TK_FIM ? "fim do arquivo" : p->atual->texto);
    return avancar(p);
}

/* static void: aumenta o nível de aninhamento e impede profundidade excessiva. */
static void entrar(Parser *p)
{
    /* if: mais de 256 níveis indica entrada excessivamente profunda. */
    if (++p->profundidade > 256)
        erro("sintático", p->atual, "aninhamento além do limite (256 níveis)");
}

static Tipo ler_tipo(Parser *p)
{
    switch (p->atual->classe) {
    case TK_INT: avancar(p); return T_INT;
    case TK_FLOAT: avancar(p); return T_FLOAT;
    case TK_TIPO_CHAR: avancar(p); return T_CHAR;
    case TK_BOOL: avancar(p); return T_BOOL;
    case TK_STRING: avancar(p); return T_STRING;
    default: erro("sintático", p->atual, "esperado um tipo");
    }
}

/* Cria um nó de expressão já com seus campos básicos inicializados. "classe"
 * informa a forma do nó e "token" conserva o texto e a posição usados em erros. */
static Expr *nova_expr(ClasseExpr classe, const Token *token)
{
    Expr *no = alocar(1, sizeof *no);
    no->classe = classe;
    no->token = token;
    no->altura = 1;
    return no;
}

static Cmd *novo_cmd(ClasseCmd classe, const Token *token)
{
    Cmd *no = alocar(1, sizeof *no);
    no->classe = classe;
    no->token = token;
    return no;
}

static void medir_altura(Expr *pai, const Expr *filho)
{
    unsigned altura = filho->altura + 1;
    if (altura > 256)
        erro("sintático", pai->token, "expressão além do limite de 256 níveis");
    if (altura > pai->altura)
        pai->altura = altura;
}

static Expr *entre_parenteses(Parser *p)
{
    exigir(p, '(');
    Expr *valor = expressao(p);
    exigir(p, ')');
    return valor;
}

/* static Expr *: monta uma chamada e liga cada argumento usando o campo prox. */
static Expr *chamada(Parser *p, const Token *nome)
{
    Expr *no = nova_expr(E_CHAMADA, nome);
    Expr **cauda = &no->esq;
    exigir(p, '(');
    /* if: executa a leitura dos argumentos somente quando há algo antes de ). */
    if (p->atual->classe != ')') {
        /* do/while: lê pelo menos um argumento e repete após cada vírgula. */
        do {
            *cauda = expressao(p);
            medir_altura(no, *cauda);
            cauda = &(*cauda)->prox;
        } while (aceitar(p, ','));
    }
    exigir(p, ')');
    return no;
}

static Expr *primario(Parser *p)
{
    const Token *token = p->atual;
    switch (token->classe) {
    case TK_INTEIRO: case TK_REAL: case TK_CHAR: case TK_TEXTO:
    case TK_TRUE: case TK_FALSE:
        return nova_expr(E_LITERAL, avancar(p));
    case '(':
        return entre_parenteses(p);
    case TK_NOME:
        avancar(p);
        /* if/else: nome seguido de ( é chamada; sem ( é referência de variável. */
        if (p->atual->classe == '(')
            return chamada(p, token);
        return nova_expr(E_VARIAVEL, token);
    default:
        erro("sintático", token, "esperada uma expressão");
    }
}

/* static Expr *: trata operador unário ! ou - antes de ler um operando. */
static Expr *unario(Parser *p)
{
    /* if: retorna ao primário quando não existe operador unário. */
    if (p->atual->classe != '!' && p->atual->classe != '-')
        return primario(p);

    entrar(p);
    Expr *no = nova_expr(E_UNARIO, avancar(p));
    no->esq = unario(p);
    medir_altura(no, no->esq);
    --p->profundidade;
    return no;
}

static int precedencia(int classe)
{
    switch (classe) {
    case TK_OU: return 1;
    case TK_E: return 2;
    case TK_IGUAL: case TK_DIF: return 3;
    case '<': case '>': case TK_MENOR_IGUAL: case TK_MAIOR_IGUAL: return 4;
    case '+': case '-': return 5;
    case '*': case '/': case '%': return 6;
    default: return 0;
    }
}

/* Ao ler o lado direito, só entram operadores de prioridade maior.
 * Operadores iguais ficam no laço, o que agrupa 8 - 3 - 2 como (8 - 3) - 2. */
static Expr *ler_operadores(Parser *p, int minimo)
{
    Expr *esquerda = unario(p);
    int prioridade;
    while ((prioridade = precedencia(p->atual->classe)) >= minimo) {
        Expr *no = nova_expr(E_BINARIO, avancar(p));
        no->esq = esquerda;
        no->dir = ler_operadores(p, prioridade + 1);
        medir_altura(no, no->esq);
        medir_altura(no, no->dir);
        esquerda = no;
    }
    return esquerda;
}

static Expr *expressao(Parser *p)
{
    entrar(p);
    Expr *valor = ler_operadores(p, 1);
    --p->profundidade;
    return valor;
}

static Cmd *bloco(Parser *p)
{
    entrar(p);
    Cmd *no = novo_cmd(C_BLOCO, exigir(p, '{'));
    Cmd **cauda = &no->corpo;
    while (p->atual->classe != '}' && p->atual->classe != TK_FIM) {
        *cauda = comando(p);
        cauda = &(*cauda)->prox;
    }
    exigir(p, '}');
    --p->profundidade;
    return no;
}

/* A declaração e a atribuição também servem no cabeçalho do for.
 * O ponto e vírgula é lido pelo trecho que as chama. */
static Cmd *declaracao(Parser *p)
{
    Cmd *no = novo_cmd(C_VAR, exigir(p, TK_VAR));
    no->nome = exigir(p, TK_NOME);
    exigir(p, ':');
    no->tipo = ler_tipo(p);
    if (aceitar(p, '='))
        no->expr = expressao(p);
    return no;
}

static Cmd *atribuicao(Parser *p, const Token *nome)
{
    Cmd *no = novo_cmd(C_ATRIB, exigir(p, '='));
    no->destino = nova_expr(E_VARIAVEL, nome);
    no->expr = expressao(p);
    return no;
}

static Cmd *laco_para(Parser *p)
{
    Cmd *no = novo_cmd(C_FOR, exigir(p, TK_FOR));
    exigir(p, '(');
    if (p->atual->classe == TK_VAR)
        no->inicio = declaracao(p);
    else if (p->atual->classe != ';')
        no->inicio = atribuicao(p, exigir(p, TK_NOME));
    exigir(p, ';');
    if (p->atual->classe != ';')
        no->expr = expressao(p);
    exigir(p, ';');
    if (p->atual->classe != ')')
        no->passo = atribuicao(p, exigir(p, TK_NOME));
    exigir(p, ')');
    no->corpo = bloco(p);
    return no;
}

static Cmd *comando(Parser *p)
{
    Cmd *no;
    const Token *token = p->atual;
    switch (token->classe) {
    case '{':
        return bloco(p);
    case TK_IF:
        no = novo_cmd(C_IF, avancar(p));
        no->expr = entre_parenteses(p);
        no->corpo = bloco(p);
        if (aceitar(p, TK_ELSE))
            no->alternativa = bloco(p);
        return no;
    case TK_WHILE:
        no = novo_cmd(C_WHILE, avancar(p));
        no->expr = entre_parenteses(p);
        no->corpo = bloco(p);
        return no;
    case TK_FOR:
        return laco_para(p);
    case TK_VAR:
        no = declaracao(p);
        break;
    case TK_PRINT:
        no = novo_cmd(C_PRINT, avancar(p));
        no->expr = entre_parenteses(p);
        break;
    case TK_INPUT:
        no = novo_cmd(C_INPUT, avancar(p));
        exigir(p, '(');
        no->destino = nova_expr(E_VARIAVEL, exigir(p, TK_NOME));
        exigir(p, ')');
        break;
    case TK_RETURN:
        no = novo_cmd(C_RETURN, avancar(p));
        no->expr = expressao(p);
        break;
    case TK_NOME:
        avancar(p);
        if (p->atual->classe == '(') {
            no = novo_cmd(C_CHAMADA, token);
            no->expr = chamada(p, token);
        } else {
            no = atribuicao(p, token);
        }
        break;
    default:
        erro("sintático", token, "esperado um comando");
    }
    exigir(p, ';');
    return no;
}

static Funcao *funcao(Parser *p)
{
    Funcao *no = alocar(1, sizeof *no);
    Parametro **cauda = &no->params;
    no->nome = exigir(p, TK_NOME);
    exigir(p, '(');
    if (p->atual->classe != ')') {
        do {
            Parametro *param = alocar(1, sizeof *param);
            param->nome = exigir(p, TK_NOME);
            exigir(p, ':');
            param->tipo = ler_tipo(p);
            *cauda = param;
            cauda = &param->prox;
        } while (aceitar(p, ','));
    }
    exigir(p, ')');
    exigir(p, ':');
    no->retorno = ler_tipo(p);
    no->corpo = bloco(p);
    return no;
}

Programa analisar_sintaxe(const Token *tokens)
{
    Parser parser = {tokens, 0};
    Programa programa = {0};
    Funcao **funcoes = &programa.funcoes;
    Cmd **comandos = &programa.comandos;

    /* A cauda aponta para o campo onde o próximo nó deve entrar. */
    while (parser.atual->classe != TK_FIM) {
        if (aceitar(&parser, TK_FUNC)) {
            *funcoes = funcao(&parser);
            funcoes = &(*funcoes)->prox;
        } else {
            *comandos = comando(&parser);
            comandos = &(*comandos)->prox;
        }
    }
    return programa;
}

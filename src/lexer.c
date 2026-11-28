#include "compilador.h"

#include <string.h>

/* O leitor conserva a posição atual no texto-fonte e os números usados para
 * mostrar erros precisos. letra() e digito() são usadas apenas com ASCII. */
typedef struct {
    const char *atual;
    size_t linha, coluna;
} Leitor;

/* Palavra reserva liga o texto escrito no programa à classe interna do token.
 * Reservadas são reconhecidas depois de ler um possível nome. */
typedef struct {
    const char *texto;
    int classe;
} Palavra;

static const Palavra reservadas[] = {
    {"var", TK_VAR}, {"func", TK_FUNC}, {"return", TK_RETURN},
    {"if", TK_IF}, {"else", TK_ELSE}, {"while", TK_WHILE}, {"for", TK_FOR},
    {"print", TK_PRINT}, {"input", TK_INPUT}, {"int", TK_INT},
    {"float", TK_FLOAT}, {"char", TK_TIPO_CHAR}, {"bool", TK_BOOL},
    {"string", TK_STRING}, {"true", TK_TRUE}, {"false", TK_FALSE}
};

/* static bool: função auxiliar disponível somente neste arquivo. Ela informa se
 * um caractere é um dígito decimal usado pelo lexer. */
static bool digito(char c)
{
    return c >= '0' && c <= '9';
}

/* static bool: verifica se o caractere pode iniciar ou continuar um nome. */
static bool letra(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* static char: consome um caractere e atualiza linha ou coluna. */
static char avancar(Leitor *leitor)
{
    char c = *leitor->atual++;
    /* if: acabou uma linha, então reinicia a coluna em 1. */
    if (c == '\n') {
        ++leitor->linha;
        leitor->coluna = 1;
    /* else if: avança a coluna apenas uma vez para cada início de caractere UTF-8. */
    } else if (((unsigned char)c & 0xc0) != 0x80) {
        ++leitor->coluna;
    }
    return c;
}

/* static void: esta função avança pelo texto até encontrar o início de um token. */
static void ignorar(Leitor *leitor)
{
    /* for: repete até encontrar conteúdo que não seja espaço ou comentário. */
    for (;;) {
        const char *atual = leitor->atual;
        /* if: ignora cada caractere de espaço em branco. */
        if (*atual && strchr(" \r\t\n", *atual)) {
            avancar(leitor);
        /* else if: comentários // continuam até o fim da linha. */
        } else if (strncmp(atual, "//", 2) == 0) {
            /* while não é for/if/else, mas avança até encontrar a quebra de linha. */
            while (*leitor->atual && *leitor->atual != '\n')
                avancar(leitor);
        /* else if: comentário iniciado por barra e asterisco exige fechamento com asterisco e barra. */
        } else if (strncmp(atual, "/*", 2) == 0) {
            Token inicio = {0, "/*", NULL, leitor->linha, leitor->coluna};
            avancar(leitor);
            avancar(leitor);
            /* while: percorre o interior até encontrar o fechamento. */
            while (*leitor->atual && strncmp(leitor->atual, "*/", 2) != 0)
                avancar(leitor);
            /* if: sem fechamento é um erro léxico na abertura do comentário. */
            if (!*leitor->atual)
                erro("léxico", &inicio, "comentário sem fechamento");
            avancar(leitor);
            avancar(leitor);
        /* else: não havia espaço ou comentário, então termina o pulo. */
        } else {
            return;
        }
    }
}

/* static char: transforma uma sequência de escape, como \\n, em um caractere real. */
static char ler_escape(Leitor *leitor, const Token *token)
{
    char c = *leitor->atual;
    /* if: avança apenas quando existe um caractere após a barra. */
    if (c)
        avancar(leitor);
    switch (c) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case '\\': case '"': case '\'': return c;
    default: erro("léxico", token, "escape inválido no literal");
    }
}

/* Lê uma string ou um char delimitado por aspas. "inicio" mostra onde o conteúdo
 * começa no vetor da memória, enquanto "tamanho" conta quantos bytes ele ocupa. */
static const char *ler_literal(Leitor *leitor, const Token *token)
{
    char aspa = avancar(leitor);
    const char *inicio = leitor->atual;
    size_t tamanho = 0;
    /* Primeiro localizamos o final para reservar somente o espaço necessário. */
    for (const char *p = inicio; *p && *p != '\n' && *p != '\r' && *p != aspa; ++p) {
        if (*p == '\\' && p[1] && p[1] != '\n' && p[1] != '\r')
            ++p;
        tamanho = (size_t)(p - inicio + 1);
    }
    char *valor = alocar(tamanho + 1, sizeof *valor);
    tamanho = 0;
    while (*leitor->atual && *leitor->atual != '\n' &&
           *leitor->atual != '\r' && *leitor->atual != aspa) {
        char c = avancar(leitor);
        if (c == '\\')
            c = ler_escape(leitor, token);
        else if (aspa == '\'' && (c < ' ' || c > '~'))
            erro("léxico", token, "char deve conter um caractere ASCII");
        valor[tamanho++] = c;
    }
    if (*leitor->atual != aspa)
        erro("léxico", token, "literal sem aspas de fechamento");
    avancar(leitor);
    if (aspa == '\'' && tamanho != 1)
        erro("léxico", token, "char deve conter um único caractere");
    return valor;
}

static Token ler_token(Leitor *leitor)
{
    static const Palavra duplos[] = {
        {"==", TK_IGUAL}, {"!=", TK_DIF}, {"<=", TK_MENOR_IGUAL},
        {">=", TK_MAIOR_IGUAL}, {"&&", TK_E}, {"||", TK_OU}
    };
    const char *inicio = leitor->atual;
    char c = *inicio;
    Token token = {TK_FIM, "", NULL, leitor->linha, leitor->coluna};
    if (!c)
        return token;

    if (letra(c)) {
        do {
            avancar(leitor);
        } while (letra(*leitor->atual) || digito(*leitor->atual));
        token.classe = TK_NOME;
    } else if (digito(c)) {
        do {
            avancar(leitor);
        } while (digito(*leitor->atual));
        token.classe = TK_INTEIRO;
        if (leitor->atual[0] == '.' && digito(leitor->atual[1])) {
            token.classe = TK_REAL;
            avancar(leitor);
            do {
                avancar(leitor);
            } while (digito(*leitor->atual));
        }
    } else if (c == '"' || c == '\'') {
        token.classe = c == '"' ? TK_TEXTO : TK_CHAR;
        token.valor = ler_literal(leitor, &token);
    } else {
        /* Consultamos os duplos primeiro para ler <= como um token só. */
        for (size_t i = 0; i < sizeof duplos / sizeof duplos[0]; ++i) {
            if (strncmp(inicio, duplos[i].texto, 2) == 0) {
                token.classe = duplos[i].classe;
                avancar(leitor);
                avancar(leitor);
                break;
            }
        }
        if (token.classe == TK_FIM) {
            if ((unsigned char)c >= 128)
                erro("léxico", &token, "caractere fora do alfabeto da linguagem");
            if (!strchr("+-*/%=!<>(){}:;,", c))
                erro("léxico", &token, "símbolo inesperado '%c'", c);
            token.classe = avancar(leitor);
        }
    }

    token.texto = copiar_texto(inicio, (size_t)(leitor->atual - inicio));
    if (token.classe == TK_NOME) {
        for (size_t i = 0; i < sizeof reservadas / sizeof reservadas[0]; ++i) {
            if (strcmp(token.texto, reservadas[i].texto) == 0) {
                token.classe = reservadas[i].classe;
                break;
            }
        }
    }
    return token;
}

Token *lexar(const char *fonte)
{
    Leitor leitor = {fonte, 1, 1};
    size_t capacidade = 64, usados = 0;
    Token *tokens = alocar(capacidade, sizeof *tokens);

    for (;;) {
        ignorar(&leitor);
        if (usados == capacidade) {
            if (capacidade > SIZE_MAX / 2)
                falha("lista de tokens grande demais");
            capacidade *= 2;
            Token *maior = alocar(capacidade, sizeof *maior);
            memcpy(maior, tokens, usados * sizeof *tokens);
            tokens = maior;
        }
        Token token = ler_token(&leitor);
        tokens[usados++] = token;
        if (token.classe == TK_FIM)
            return tokens;
    }
}

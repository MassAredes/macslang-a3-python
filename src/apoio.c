#include "compilador.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef MACS_MEMORIA
#include <crtdbg.h>
#endif

/* Tokens, nós e símbolos vivem até o fim de uma execução do compilador.
 * A lista permite limpar tudo também quando um diagnóstico encerra o processo. */
typedef struct Alocacao {
    void *dados;
    struct Alocacao *proxima;
} Alocacao;

static Alocacao *alocacoes;

/* static void: libera toda memória registrada sem devolver um valor ao chamador. */
static void liberar_memoria(void)
{
    /* while: repete a liberação enquanto ainda houver registros na lista. */
    while (alocacoes) {
        Alocacao *item = alocacoes;
        alocacoes = item->proxima;
        free(item->dados);
        free(item);
    }
}

#ifdef MACS_MEMORIA
/* static void: consulta a biblioteca de depuração do MSVC após a execução. */
static void conferir_memoria(void)
{
    /* if: existe vazamento quando a biblioteca encontra blocos não liberados. */
    if (_CrtDumpMemoryLeaks()) {
        fputs("[memoria] vazamento detectado no compilador\n", stderr);
        _Exit(1);
    }
    fputs("[memoria] compilador sem vazamentos detectados\n", stderr);
}
#endif

/* void: configura a limpeza automática; não devolve informação ao chamador. */
void iniciar_memoria(void)
{
#ifdef MACS_MEMORIA
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    /* if: registra a verificação somente se a biblioteca aceitar a função. */
    if (atexit(conferir_memoria) != 0)
        falha("não foi possível iniciar a verificação de memória");
#endif
    /* if: registra a limpeza normal antes que o processo termine. */
    if (atexit(liberar_memoria) != 0)
        falha("não foi possível iniciar a limpeza de memória");
}

/* void *: reserva memória zerada e devolve o endereço dos dados. */
void *alocar(size_t quantidade, size_t tamanho)
{
    Alocacao *item;
    void *dados;

    /* if: evita uma multiplicação que ultrapasse o limite de size_t. */
    if (tamanho && quantidade > SIZE_MAX / tamanho)
        falha("tamanho de alocação inválido");

    dados = calloc(quantidade, tamanho);
    /* if: o endereço NULL significa que o sistema não conseguiu reservar memória. */
    if (!dados)
        falha("memória insuficiente");

    item = malloc(sizeof *item);
    /* if/else: se não houver registro, libera os dados antes de encerrar. */
    if (!item) {
        free(dados);
        falha("memória insuficiente");
    }

    item->dados = dados;
    item->proxima = alocacoes;
    alocacoes = item;
    return dados;
}

char *copiar_texto(const char *inicio, size_t tamanho)
{
    char *copia = alocar(tamanho + 1, sizeof *copia);
    memcpy(copia, inicio, tamanho);
    return copia;
}

void falha(const char *mensagem)
{
    fprintf(stderr, "Erro: %s\n", mensagem);
    exit(EXIT_FAILURE);
}

void erro(const char *fase, const Token *token, const char *formato, ...)
{
    va_list args;
    fprintf(stderr, "Erro %s na linha %zu, coluna %zu: ", fase, token->linha, token->coluna);
    va_start(args, formato);
    vfprintf(stderr, formato, args);
    va_end(args);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

const char *nome_tipo(Tipo tipo)
{
    static const char *nomes[] = {"?", "int", "float", "char", "bool", "string"};
    return nomes[tipo];
}

const char *nome_token(int classe)
{
    static const char *nomes[] = {
        "IDENT", "INTEIRO", "REAL", "CARACTERE", "TEXTO", "var", "func", "return",
        "if", "else", "while", "for", "print", "input", "int", "float", "char",
        "bool", "string", "true", "false", "==", "!=", "<=", ">=", "&&", "||"
    };
    static char simbolo[2];
    if (classe == TK_FIM)
        return "EOF";
    if (classe >= TK_NOME)
        return nomes[classe - TK_NOME];
    simbolo[0] = (char)classe;
    return simbolo;
}

/* void: escreve texto com aspas, protegendo quebra de linha e barras. */
void mostrar_texto(FILE *saida, const char *texto, char aspas)
{
    fputc(aspas, saida);
    /* for: percorre todos os caracteres até encontrar o terminador '\\0'. */
    for (; *texto; ++texto) {
        switch (*texto) {
        case '\n': fputs("\\n", saida); break;
        case '\r': fputs("\\r", saida); break;
        case '\t': fputs("\\t", saida); break;
        default:
            /* if: escape necessário antes de uma aspas ou barra no texto exibido. */
            if (*texto == aspas || *texto == '\\')
                fputc('\\', saida);
            fputc((unsigned char)*texto, saida);
        }
    }
    fputc(aspas, saida);
}

#include "macs_runtime.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#ifdef MACS_MEMORIA
#include <crtdbg.h>
#endif

#pragma warning(push)
#pragma warning(disable: 5105)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning(pop)

/* As strings criadas em execução podem continuar em uso após sair de um
 * bloco ou retornar de uma função. Guardamos cada endereço até o programa
 * terminar, quando liberamos toda a lista de uma vez. */
struct TextoGuardado {
    char *valor;
    struct TextoGuardado *proximo;
};

static struct TextoGuardado *textos = NULL;

/* static void: percorre a lista de textos alocados e libera todos ao terminar o programa. */
static void liberar_textos(void)
{
    /* while: continua enquanto existe pelo menos um texto registrado. */
    while (textos != NULL) {
        struct TextoGuardado *item = textos;
        textos = item->proximo;
        free(item->valor);
        free(item);
    }
}

#ifdef MACS_MEMORIA
/* static void: verifica vazamentos no executável em modo de depuração. */
static void conferir_memoria(void)
{
    /* if: relatório diferente é usado quando existem vazamentos. */
    if (_CrtDumpMemoryLeaks()) {
        fputs("[memoria] vazamento detectado no executável\n", stderr);
        _Exit(1);
    }
    fputs("[memoria] executável sem vazamentos detectados\n", stderr);
}
#endif

/* void: prepara entrada e saída do console e registra limpezas automáticas. */
void macs_iniciar(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#ifdef MACS_MEMORIA
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    atexit(conferir_memoria);
#endif
    atexit(liberar_textos);
}

/* static void: informa um erro de execução e encerra o programa. */
static void falhar(const char *mensagem)
{
    fprintf(stderr, "Erro de execução: %s\n", mensagem);
    exit(EXIT_FAILURE);
}

/* static char *: aloca memória ou chama falhar quando o espaço não está disponível. */
static char *alocar(size_t tamanho)
{
    char *memoria = malloc(tamanho);
    /* if: NULL informa que a alocação de memória falhou. */
    if (memoria == NULL) {
        falhar("memória insuficiente");
    }
    return memoria;
}

/* static const char *: registra uma string alocada para ser liberada no final. */
static const char *guardar(char *valor)
{
    struct TextoGuardado *item = malloc(sizeof *item);
    /* if: remove o texto já criado e encerra se não houver registro disponível. */
    if (item == NULL) {
        free(valor);
        falhar("memória insuficiente");
    }
    item->valor = valor;
    item->proximo = textos;
    textos = item;
    return valor;
}

static const char *copiar(const char *texto)
{
    size_t tamanho = strlen(texto) + 1;
    char *copia = alocar(tamanho);
    memcpy(copia, texto, tamanho);
    return guardar(copia);
}

static char *ler_linha(void)
{
    size_t capacidade = 64;
    size_t tamanho = 0;
    char *linha = alocar(capacidade);
    int caractere;

    /* Mostra o último print antes de esperar a entrada do usuário. */
    fflush(stdout);
    while ((caractere = getchar()) != '\n' && caractere != EOF) {
        if (caractere == '\0') {
            free(linha);
            falhar("entrada contém byte nulo");
        }
        /* Deixa espaço para o terminador '\0' e cresce quando necessário. */
        if (tamanho == capacidade - 1) {
            if (capacidade > SIZE_MAX / 2) {
                free(linha);
                falhar("linha de entrada longa demais");
            }
            capacidade *= 2;
            char *maior = realloc(linha, capacidade);
            if (maior == NULL) {
                free(linha);
                falhar("memória insuficiente");
            }
            linha = maior;
        }
        linha[tamanho++] = (char)caractere;
    }
    if (caractere == EOF && (tamanho == 0 || ferror(stdin))) {
        free(linha);
        falhar("fim da entrada antes de ler um valor");
    }
    if (tamanho > 0 && linha[tamanho - 1] == '\r') {
        tamanho--;
    }
    linha[tamanho] = '\0';
    return linha;
}

static char *pular_espacos(char *texto)
{
    while (isspace((unsigned char)*texto)) {
        texto++;
    }
    return texto;
}

int64_t macs_ler_int(void)
{
    char *linha = ler_linha();
    char *fim;
    errno = 0;
    intmax_t valor = strtoimax(linha, &fim, 10);
    bool valido = fim != linha && errno != ERANGE && *pular_espacos(fim) == '\0';
    free(linha);
    if (!valido || valor < INT64_MIN || valor > INT64_MAX) {
        falhar("entrada inválida para int");
    }
    return (int64_t)valor;
}

double macs_ler_float(void)
{
    char *linha = ler_linha();
    char *fim;
    errno = 0;
    double valor = strtod(linha, &fim);
    bool valido = fim != linha && errno != ERANGE && *pular_espacos(fim) == '\0';
    free(linha);
    if (!valido || !isfinite(valor)) {
        falhar("entrada inválida para float");
    }
    return valor;
}

char macs_ler_char(void)
{
    char *linha = ler_linha();
    bool valido = strlen(linha) == 1 && (unsigned char)linha[0] <= 127;
    char valor = linha[0];
    free(linha);
    if (!valido) {
        falhar("entrada inválida para char (digite um caractere ASCII)");
    }
    return valor;
}

bool macs_ler_bool(void)
{
    char *linha = ler_linha();
    char *inicio = pular_espacos(linha);
    char *fim = inicio + strlen(inicio);
    while (fim > inicio && isspace((unsigned char)fim[-1])) {
        *--fim = '\0';
    }
    bool verdadeiro = strcmp(inicio, "true") == 0 || strcmp(inicio, "1") == 0;
    bool falso = strcmp(inicio, "false") == 0 || strcmp(inicio, "0") == 0;
    free(linha);
    if (!verdadeiro && !falso) {
        falhar("entrada inválida para bool (digite true, false, 1 ou 0)");
    }
    return verdadeiro;
}

const char *macs_ler_string(void)
{
    return guardar(ler_linha());
}

/* Estas conversões são usadas quando um valor entra numa soma com string. */
const char *macs_texto_int(int64_t valor)
{
    char texto[32];
    snprintf(texto, sizeof texto, "%" PRId64, valor);
    return copiar(texto);
}

const char *macs_texto_float(double valor)
{
    char texto[64];
    snprintf(texto, sizeof texto, "%.15g", valor);
    return copiar(texto);
}

const char *macs_texto_char(char valor)
{
    char texto[] = {valor, '\0'};
    return copiar(texto);
}

const char *macs_texto_bool(bool valor)
{
    return valor ? "true" : "false";
}

const char *macs_juntar(const char *esquerda, const char *direita)
{
    size_t primeiro = strlen(esquerda);
    size_t segundo = strlen(direita);
    if (primeiro >= SIZE_MAX - segundo) {
        falhar("string resultante longa demais");
    }
    char *resultado = alocar(primeiro + segundo + 1);
    memcpy(resultado, esquerda, primeiro);
    memcpy(resultado + primeiro, direita, segundo + 1);
    return guardar(resultado);
}

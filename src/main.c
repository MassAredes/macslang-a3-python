#include "plataforma.h"

#include <stdlib.h>
#include <wchar.h>

/* Enumeração com os modos em que a execução pode terminar: um modo de compilação normal
 * ou uma opção que serve apenas para mostrar uma fase intermediária. */
typedef enum { NORMAL, TOKENS, ARVORE, CHECAR, EMITIR_C, SAIDA } Modo;

/* Guarda o único arquivo-fonte informado pelo usuário, o caminho opcional de saída
 * e o modo escolhido. Uma opção ausente fica como NULL ou como NORMAL. */
typedef struct {
    const wchar_t *fonte;
    const wchar_t *saida;
    Modo modo;
} Opcoes;

/* static void: mostra as opções, mas não devolve um valor ao chamador. */
static void ajuda(void)
{
    puts("Uso: macsc arquivo.macs [opção]\n"
         "  --tokens           mostra a análise léxica\n"
         "  --ast              mostra a árvore sintática\n"
         "  --check            confere o programa até a semântica\n"
         "  --emit-c arquivo.c grava apenas a tradução C11\n"
         "  -o arquivo.exe     escolhe o caminho do executável\n"
         "Sem opção, grava build\\<nome>.c e build\\<nome>.exe.");
}

/* Percorre os argumentos em busca de um arquivo, de uma opção e, em alguns casos,
 * do caminho que vem logo depois da opção. */
static Opcoes ler_opcoes(int argc, wchar_t **argv)
{
    Opcoes opcoes = {0};
    /* for: percorre cada argumento passado na linha de comando. */
    for (int i = 1; i < argc; ++i) {
        const wchar_t *arg = argv[i];
        /* if/else if: reconhece ajuda, arquivo-fonte e opções iniciadas por -. */
        if (!wcscmp(arg, L"--help") || !wcscmp(arg, L"-h")) {
            ajuda();
            exit(EXIT_SUCCESS);
        }
        if (arg[0] != L'-') {
            /* if: não permite informar dois arquivos-fonte. */
            if (opcoes.fonte)
                falha("informe um único arquivo-fonte");
            opcoes.fonte = arg;
            continue;
        }

        /* if: permite apenas uma opção de modo por execução. */
        if (opcoes.modo != NORMAL)
            falha("use apenas uma opção por execução");

        if (!wcscmp(arg, L"--tokens"))
            opcoes.modo = TOKENS;
        else if (!wcscmp(arg, L"--ast"))
            opcoes.modo = ARVORE;
        else if (!wcscmp(arg, L"--check"))
            opcoes.modo = CHECAR;
        else if (!wcscmp(arg, L"--emit-c") || !wcscmp(arg, L"-o")) {
            opcoes.modo = !wcscmp(arg, L"-o") ? SAIDA : EMITIR_C;
            /* if: a opção que escolhe saída precisa do argumento seguinte. */
            if (++i == argc)
                falha("falta o caminho de saída");
            opcoes.saida = argv[i];
        } else {
            /* else: qualquer texto iniciado por - não reconhecido gera erro. */
            falha("opção desconhecida. Use --help.");
        }
    }
    /* if final: encerra se o usuário não informou o arquivo MACSLang. */
    if (!opcoes.fonte)
        falha("informe o arquivo-fonte .macs");
    return opcoes;
}

/* Procura da direita para a esquerda a última barra ou contrabarra. O ponteiro
 * devolvido fica apontando para o nome após essa barra. */
static const wchar_t *nome_arquivo(const wchar_t *caminho)
{
    const wchar_t *nome = caminho;
    for (const wchar_t *p = caminho; *p; ++p)
        if (*p == L'\\' || *p == L'/')
            nome = p + 1;
    return nome;
}

static bool extensao(const wchar_t *caminho, const wchar_t *esperada)
{
    const wchar_t *ponto = wcsrchr(nome_arquivo(caminho), L'.');
    return ponto && _wcsicmp(ponto, esperada) == 0;
}

static wchar_t *saida_padrao(const wchar_t *fonte)
{
    const wchar_t *nome = nome_arquivo(fonte);
    size_t comprimento = wcslen(nome) - wcslen(L".macs");
    size_t tamanho = comprimento + wcslen(L"build\\.exe") + 1;
    wchar_t *saida = alocar(tamanho, sizeof *saida);
    swprintf(saida, tamanho, L"build\\%.*ls.exe", (int)comprimento, nome);
    return saida;
}

static const wchar_t *completar_saida(const wchar_t *saida)
{
    if (wcsrchr(nome_arquivo(saida), L'.'))
        return saida;
    size_t tamanho = wcslen(saida) + wcslen(L".exe") + 1;
    wchar_t *resultado = alocar(tamanho, sizeof *resultado);
    swprintf(resultado, tamanho, L"%ls.exe", saida);
    return resultado;
}

static void mostrar_tokens(const Token *tokens)
{
    for (const Token *token = tokens;; ++token) {
        printf("%zu:%zu %-12s ", token->linha, token->coluna, nome_token(token->classe));
        mostrar_texto(stdout, token->texto, '\'');
        putchar('\n');
        if (token->classe == TK_FIM)
            break;
    }
}

int wmain(int argc, wchar_t **argv)
{
    preparar_console();
    iniciar_memoria();
    Opcoes opcoes = ler_opcoes(argc, argv);
    if (!extensao(opcoes.fonte, L".macs"))
        falha("use um arquivo-fonte com extensão .macs");

    /* Cada fase consome a saída da anterior. Os modos de estudo param no ponto pedido. */
    Token *tokens = lexar(ler_fonte(opcoes.fonte));
    if (opcoes.modo == TOKENS) {
        mostrar_tokens(tokens);
        return EXIT_SUCCESS;
    }
    Programa programa = analisar_sintaxe(tokens);
    if (opcoes.modo == ARVORE) {
        mostrar_arvore(&programa);
        return EXIT_SUCCESS;
    }
    analisar_semantica(&programa);
    if (opcoes.modo == CHECAR) {
        puts("OK: análises léxica, sintática e semântica concluídas.");
    } else if (opcoes.modo == EMITIR_C) {
        if (!extensao(opcoes.saida, L".c"))
            falha("a saída de --emit-c precisa ter extensão .c");
        salvar_c(opcoes.saida, &programa);
    } else {
        const wchar_t *saida = opcoes.saida ? completar_saida(opcoes.saida) : saida_padrao(opcoes.fonte);
        if (!extensao(saida, L".exe"))
            falha("a saída de -o precisa ter extensão .exe");
        gerar_executavel(saida, &programa);
    }
    return EXIT_SUCCESS;
}

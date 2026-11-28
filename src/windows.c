#include "plataforma.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* O SDK 10.0.19041 emite C5105 em seu próprio winbase.h no modo C11. */
#pragma warning(push)
#pragma warning(disable: 5105)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning(pop)

static wchar_t *temporaria;

void preparar_console(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

char *ler_fonte(const wchar_t *caminho)
{
    FILE *arquivo = _wfopen(caminho, L"rb");
    if (!arquivo)
        falha("não foi possível abrir o arquivo-fonte");

    long tamanho = -1;
    if (fseek(arquivo, 0, SEEK_END) == 0) {
        tamanho = ftell(arquivo);
        if (fseek(arquivo, 0, SEEK_SET) != 0)
            tamanho = -1;
    }
    if (tamanho < 0) {
        fclose(arquivo);
        falha("não foi possível medir o arquivo-fonte");
    }

    char *fonte = alocar((size_t)tamanho + 1, sizeof *fonte);
    size_t lidos = fread(fonte, 1, (size_t)tamanho, arquivo);
    int fechou = fclose(arquivo);
    if (lidos != (size_t)tamanho || fechou != 0)
        falha("não foi possível ler o arquivo-fonte");

    if (memchr(fonte, 0, lidos) ||
        (lidos && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fonte, tamanho, NULL, 0)))
        falha("o fonte deve ser texto UTF-8 sem bytes nulos");

    /* Aceita o BOM e converte as quebras de linha do Windows para uma só forma. */
    size_t ler = 0, gravar = 0;
    if (lidos >= 3 && memcmp(fonte, "\xef\xbb\xbf", 3) == 0)
        ler = 3;
    while (ler < lidos) {
        char c = fonte[ler++];
        if (c == '\r') {
            if (fonte[ler] == '\n')
                ++ler;
            c = '\n';
        }
        fonte[gravar++] = c;
    }
    fonte[gravar] = '\0';
    return fonte;
}

static wchar_t *juntar(const wchar_t *a, const wchar_t *b)
{
    size_t tamanho_a = wcslen(a), tamanho_b = wcslen(b);
    wchar_t *resultado = alocar(tamanho_a + tamanho_b + 1, sizeof *resultado);
    wmemcpy(resultado, a, tamanho_a);
    wmemcpy(resultado + tamanho_a, b, tamanho_b + 1);
    return resultado;
}

static wchar_t *absoluto(const wchar_t *caminho)
{
    DWORD tamanho = GetFullPathNameW(caminho, 0, NULL, NULL);
    if (!tamanho)
        falha("caminho inválido");
    wchar_t *resultado = alocar(tamanho, sizeof *resultado);
    DWORD lidos = GetFullPathNameW(caminho, tamanho, resultado, NULL);
    if (!lidos || lidos >= tamanho)
        falha("não foi possível resolver o caminho");
    return resultado;
}

static wchar_t *pasta_de(const wchar_t *caminho)
{
    const wchar_t *barra = wcsrchr(caminho, L'\\');
    if (!barra)
        falha("caminho sem pasta");
    size_t tamanho = (size_t)(barra - caminho);
    if (tamanho == 2 && caminho[1] == L':')
        ++tamanho;
    wchar_t *pasta = alocar(tamanho + 1, sizeof *pasta);
    wmemcpy(pasta, caminho, tamanho);
    return pasta;
}

static void criar_pastas(const wchar_t *caminho)
{
    DWORD atributos = GetFileAttributesW(caminho);
    if (atributos != INVALID_FILE_ATTRIBUTES) {
        if (!(atributos & FILE_ATTRIBUTE_DIRECTORY))
            falha("o caminho da pasta de saída aponta para um arquivo");
        return;
    }

    wchar_t *pai = pasta_de(caminho);
    if (wcscmp(pai, caminho) == 0)
        falha("a unidade de saída não está acessível");
    criar_pastas(pai);
    if (!CreateDirectoryW(caminho, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        falha("não foi possível criar a pasta de saída");
}

static wchar_t *pasta_runtime(void)
{
    wchar_t executavel[MAX_PATH];
    DWORD tamanho = GetModuleFileNameW(NULL, executavel, MAX_PATH);
    if (!tamanho || tamanho >= MAX_PATH)
        falha("não foi possível localizar a pasta do compilador");
    return absoluto(juntar(pasta_de(executavel), L"\\..\\runtime"));
}

static void proteger_runtime(const wchar_t *destino, const wchar_t *runtime)
{
    if (_wcsicmp(destino, juntar(runtime, L"\\macs_runtime.c")) == 0)
        falha("escolha uma saída diferente de macs_runtime.c");
}

static void gravar_c(const wchar_t *caminho, const Programa *programa)
{
    FILE *arquivo = _wfopen(caminho, L"wb");
    if (!arquivo)
        falha("não foi possível abrir a saída C");
    gerar_c(arquivo, programa);
    int erro_escrita = ferror(arquivo);
    if (fclose(arquivo) != 0 || erro_escrita)
        falha("não foi possível gravar a saída C");
}

void salvar_c(const wchar_t *caminho, const Programa *programa)
{
    wchar_t *destino = absoluto(caminho);
    proteger_runtime(destino, pasta_runtime());
    criar_pastas(pasta_de(destino));
    gravar_c(destino, programa);
    puts("Código C11 gravado no caminho informado.");
}

static void limpar_temporaria(void)
{
    static const wchar_t *arquivos[] = {
        L"gerado.c", L"gerado.obj", L"macs_runtime.obj", L"programa.exe", L"anterior.c"
    };
    if (!temporaria)
        return;
    wchar_t caminho[MAX_PATH];
    for (size_t i = 0; i < sizeof arquivos / sizeof arquivos[0]; ++i) {
        int resultado = swprintf(caminho, MAX_PATH, L"%ls\\%ls", temporaria, arquivos[i]);
        if (resultado > 0)
            DeleteFileW(caminho);
    }
    RemoveDirectoryW(temporaria);
}

static void preparar_temporaria(const wchar_t *pasta)
{
    if (wcslen(pasta) > MAX_PATH - 65)
        falha("pasta de saída longa demais para as ferramentas usadas");
    if (atexit(limpar_temporaria) != 0)
        falha("não foi possível registrar a limpeza dos temporários");

    for (unsigned i = 0; i < 100; ++i) {
        wchar_t sufixo[40];
        swprintf(sufixo, sizeof sufixo / sizeof sufixo[0], L"\\macs-%lu-%u",
                 (unsigned long)GetCurrentProcessId(), i);
        wchar_t *candidata = juntar(pasta, sufixo);
        if (CreateDirectoryW(candidata, NULL)) {
            temporaria = candidata;
            return;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            break;
    }
    falha("não foi possível criar uma pasta temporária");
}

static void chamar_cl(const wchar_t *runtime)
{
    wchar_t cl[MAX_PATH];
    DWORD tamanho_cl = SearchPathW(NULL, L"cl.exe", NULL, MAX_PATH, cl, NULL);
    if (!tamanho_cl || tamanho_cl >= MAX_PATH)
        falha("cl.exe indisponível. Abra o x86 Native Tools Command Prompt.");

#ifdef MACS_MEMORIA
    const wchar_t *modo = L"/MTd /DMACS_MEMORIA";
#else
    const wchar_t *modo = L"/MT";
#endif
    const wchar_t *fonte_runtime = juntar(runtime, L"\\macs_runtime.c");
    size_t tamanho = wcslen(runtime) + wcslen(fonte_runtime) + 250;
    wchar_t *linha = alocar(tamanho, sizeof *linha);
    if (swprintf(linha, tamanho,
                 L"cl.exe /nologo /TC /std:c11 /utf-8 /W3 /Od %ls /I\"%ls\" "
                 L"gerado.c \"%ls\" /Fe:programa.exe /link /MACHINE:X86 /INCREMENTAL:NO",
                 modo, runtime, fonte_runtime) < 0)
        falha("não foi possível montar o comando do cl.exe");

    /* CreateProcess recebe os caminhos entre aspas e não passa pelo cmd.exe. */
    STARTUPINFOW inicio = {0};
    PROCESS_INFORMATION processo = {0};
    inicio.cb = sizeof inicio;
    if (!CreateProcessW(cl, linha, NULL, NULL, TRUE, 0, NULL, temporaria, &inicio, &processo))
        falha("não foi possível iniciar cl.exe");

    DWORD retorno = 1;
    bool terminou = WaitForSingleObject(processo.hProcess, INFINITE) == WAIT_OBJECT_0 &&
                    GetExitCodeProcess(processo.hProcess, &retorno);
    CloseHandle(processo.hThread);
    CloseHandle(processo.hProcess);
    if (!terminou || retorno != 0)
        falha("a compilação C falhou. Confira as mensagens do cl.exe e o ambiente x86.");
}

static void verificar_x86(const wchar_t *caminho)
{
    unsigned char inicio[64], pe[6];
    FILE *arquivo = _wfopen(caminho, L"rb");
    if (!arquivo)
        falha("o cl.exe não produziu o executável");
    bool validou = false;
    if (fread(inicio, 1, sizeof inicio, arquivo) == sizeof inicio &&
        inicio[0] == 'M' && inicio[1] == 'Z') {
        /* Os bytes 60 a 63 apontam para a assinatura PE e o campo Machine. */
        uint32_t posicao = (uint32_t)inicio[60] | ((uint32_t)inicio[61] << 8) |
                           ((uint32_t)inicio[62] << 16) | ((uint32_t)inicio[63] << 24);
        if (posicao <= INT32_MAX && fseek(arquivo, (long)posicao, SEEK_SET) == 0 &&
            fread(pe, 1, sizeof pe, arquivo) == sizeof pe)
            validou = memcmp(pe, "PE\0\0\x4c\x01", sizeof pe) == 0;
    }
    fclose(arquivo);
    if (!validou)
        falha("o resultado não é um executável Windows x86 (PE 0x014C)");
}

static void publicar(const wchar_t *c, const wchar_t *exe,
                     const wchar_t *destino_c, const wchar_t *destino_exe)
{
    const wchar_t *anterior = juntar(temporaria, L"\\anterior.c");
    DWORD atributos = GetFileAttributesW(destino_c);
    bool tinha_c = atributos != INVALID_FILE_ATTRIBUTES;
    if (tinha_c && ((atributos & FILE_ATTRIBUTE_DIRECTORY) ||
                    !MoveFileW(destino_c, anterior)))
        falha("não foi possível substituir a saída C");
    bool publicou_c = MoveFileW(c, destino_c) != 0;
    if (publicou_c &&
        MoveFileExW(exe, destino_exe, MOVEFILE_REPLACE_EXISTING))
        return;

    /* Se o executável estiver em uso, restauramos o C da compilação anterior. */
    if (tinha_c) {
        if (!MoveFileExW(anterior, destino_c, MOVEFILE_REPLACE_EXISTING)) {
            /* Mantém a cópia na pasta temporária se o destino bloqueou a restauração. */
            temporaria = NULL;
            falha("a publicação falhou e a saída C anterior está na pasta macs-*");
        }
    } else if (publicou_c) {
        DeleteFileW(destino_c);
    }
    falha("não foi possível publicar o resultado. Confira se a saída está em uso.");
}

void gerar_executavel(const wchar_t *caminho, const Programa *programa)
{
    wchar_t *exe = absoluto(caminho);
    wchar_t *c = juntar(exe, L"");
    wcscpy(wcsrchr(c, L'.'), L".c");
    wchar_t *runtime = pasta_runtime();
    proteger_runtime(c, runtime);
    wchar_t *pasta = pasta_de(exe);
    criar_pastas(pasta);
    preparar_temporaria(pasta);

    wchar_t *c_temporario = juntar(temporaria, L"\\gerado.c");
    wchar_t *exe_temporario = juntar(temporaria, L"\\programa.exe");
    gravar_c(c_temporario, programa);
    chamar_cl(runtime);
    verificar_x86(exe_temporario);
    publicar(c_temporario, exe_temporario, c, exe);
    puts("Código C11 e executável Windows x86 gerados (PE 0x014C).");
}

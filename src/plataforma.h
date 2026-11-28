#ifndef PLATAFORMA_H
#define PLATAFORMA_H

#include "compilador.h"

void preparar_console(void);
char *ler_fonte(const wchar_t *caminho);
void salvar_c(const wchar_t *caminho, const Programa *programa);
void gerar_executavel(const wchar_t *caminho, const Programa *programa);

#endif

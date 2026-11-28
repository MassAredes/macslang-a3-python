#ifndef MACS_RUNTIME_H
#define MACS_RUNTIME_H

/* Tipos e funções auxiliares usados pelo C gerado. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void macs_iniciar(void);

int64_t macs_ler_int(void);
double macs_ler_float(void);
char macs_ler_char(void);
bool macs_ler_bool(void);
const char *macs_ler_string(void);

const char *macs_texto_int(int64_t valor);
const char *macs_texto_float(double valor);
const char *macs_texto_char(char valor);
const char *macs_texto_bool(bool valor);
const char *macs_juntar(const char *esquerda, const char *direita);

#endif

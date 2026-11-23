"""Fase 3: verifica tipos, escopos e funções antes de gerar código."""

from dataclasses import dataclass
import math

import nos
from erros import ErroCompilacao


NUMERICOS = {"int", "float"}
MAIOR_INTEIRO = 2**63 - 1


@dataclass
class Simbolo:
    tipo: str
    nome_c: str


class Semantico:
    def __init__(self):
        self.funcoes = {}
        # Cada dicionário representa um nível de chaves. O último é o atual.
        self.escopos = []
        self.total_variaveis = 0
        self.retorno_esperado = None

    def analisar(self, programa):
        # A lista completa de funções precisa existir antes de analisar qualquer
        # corpo. Assim são possíveis recursão e chamadas a funções mais adiante.
        for numero, funcao in enumerate(programa.funcoes, 1):
            nome = funcao.token.texto
            if nome in self.funcoes:
                self.erro(funcao.token, f"função {nome!r} já declarada")
            funcao.nome_c = f"macs_funcao_{numero}"
            self.funcoes[nome] = funcao

        for funcao in programa.funcoes:
            self.escopos = [{}]
            self.retorno_esperado = funcao.retorno
            for parametro in funcao.parametros:
                parametro.nome_c = self.declarar(parametro.token, parametro.tipo)
            if not self.verificar_comando(funcao.corpo):
                self.erro(funcao.token, "há caminho na função sem return")

        # O código solto do fonte vai dentro do main do C e tem escopo próprio.
        self.retorno_esperado = None
        self.escopos = [{}]
        self.verificar_lista(programa.comandos)

    def erro(self, token, mensagem):
        raise ErroCompilacao("semântico", token, mensagem)

    def declarar(self, token, tipo):
        escopo = self.escopos[-1]
        if token.texto in escopo:
            self.erro(token, f"variável {token.texto!r} já declarada neste escopo")
        self.total_variaveis += 1
        # Um nome próprio evita conflitos com palavras do C e distingue dois
        # nomes iguais declarados em blocos diferentes da MACSLang.
        nome_c = f"macs_var_{self.total_variaveis}"
        escopo[token.texto] = Simbolo(tipo, nome_c)
        return nome_c

    def resolver_variavel(self, variavel):
        nome = variavel.token.texto
        for escopo in reversed(self.escopos):
            if nome in escopo:
                simbolo = escopo[nome]
                variavel.tipo = simbolo.tipo
                variavel.nome_c = simbolo.nome_c
                return simbolo.tipo
        self.erro(variavel.token, f"variável {nome!r} não declarada")

    def conferir_atribuicao(self, destino, valor):
        origem = self.verificar_expressao(valor)
        if destino == origem or (destino == "float" and origem == "int"):
            return
        self.erro(valor.token, f"não é possível passar um valor {origem} para {destino}")

    def conferir_condicao(self, expressao):
        if self.verificar_expressao(expressao) != "bool":
            self.erro(expressao.token, "a condição precisa ser bool")

    def verificar_lista(self, comandos):
        retorna = False
        for comando in comandos:
            # Verificamos inclusive comandos após um return, para apontar erros.
            if self.verificar_comando(comando):
                retorna = True
        return retorna

    def verificar_comando(self, comando):
        """Valida o comando. Devolve True quando ele garante um retorno."""
        if isinstance(comando, nos.Bloco):
            self.escopos.append({})
            retorna = self.verificar_lista(comando.comandos)
            self.escopos.pop()
            return retorna
        if isinstance(comando, nos.Declaracao):
            # O inicializador só enxerga variáveis já disponíveis no escopo.
            if comando.valor is not None:
                self.conferir_atribuicao(comando.tipo, comando.valor)
            comando.nome_c = self.declarar(comando.nome, comando.tipo)
        elif isinstance(comando, nos.Atribuicao):
            tipo = self.resolver_variavel(comando.destino)
            self.conferir_atribuicao(tipo, comando.valor)
        elif isinstance(comando, nos.Entrada):
            self.resolver_variavel(comando.destino)
        elif isinstance(comando, nos.Saida):
            self.verificar_expressao(comando.valor)
        elif isinstance(comando, nos.ExecutarChamada):
            self.verificar_expressao(comando.chamada)
        elif isinstance(comando, nos.Retorno):
            if self.retorno_esperado is None:
                self.erro(comando.token, "return precisa estar dentro de uma função")
            self.conferir_atribuicao(self.retorno_esperado, comando.valor)
            return True
        elif isinstance(comando, nos.Condicional):
            self.conferir_condicao(comando.condicao)
            retorna_no_if = self.verificar_comando(comando.corpo)
            if comando.alternativa is not None:
                retorna_no_else = self.verificar_comando(comando.alternativa)
                return retorna_no_if and retorna_no_else
        elif isinstance(comando, nos.Enquanto):
            self.conferir_condicao(comando.condicao)
            self.verificar_comando(comando.corpo)
        elif isinstance(comando, nos.Para):
            # A variável declarada no cabeçalho deixa de existir após o for.
            self.escopos.append({})
            if comando.inicio is not None:
                self.verificar_comando(comando.inicio)
            if comando.condicao is not None:
                self.conferir_condicao(comando.condicao)
            if comando.atualizacao is not None:
                self.verificar_comando(comando.atualizacao)
            self.verificar_comando(comando.corpo)
            self.escopos.pop()
        else:
            raise ValueError(f"comando desconhecido: {comando!r}")

        # Para garantir um retorno não presumimos que um laço vai executar.
        return False

    def verificar_expressao(self, expressao):
        if isinstance(expressao, nos.Literal):
            tipo = self.verificar_literal(expressao)
        elif isinstance(expressao, nos.Variavel):
            tipo = self.resolver_variavel(expressao)
        elif isinstance(expressao, nos.Chamada):
            tipo = self.verificar_chamada(expressao)
        elif isinstance(expressao, nos.Unario):
            operador = expressao.token.tipo
            tipo = self.verificar_expressao(expressao.operando)
            if not ((operador == "!" and tipo == "bool") or (operador == "-" and tipo in NUMERICOS)):
                self.erro(expressao.token, f"operador {operador!r} não aceita {tipo}")
        elif isinstance(expressao, nos.Binario):
            tipo = self.verificar_binario(expressao)
        else:
            raise ValueError(f"expressão desconhecida: {expressao!r}")
        expressao.tipo = tipo
        return tipo

    def verificar_literal(self, literal):
        tipo_token = literal.token.tipo
        if tipo_token == "INTEIRO":
            texto = literal.token.texto.lstrip("0") or "0"
            # Verificar o comprimento primeiro evita converter milhares de dígitos.
            if len(texto) > 19 or int(texto) > MAIOR_INTEIRO:
                self.erro(literal.token, "literal int fora da faixa de 64 bits")
            literal.valor = int(texto)
            return "int"
        if tipo_token == "REAL":
            literal.valor = float(literal.token.texto)
            if not math.isfinite(literal.valor):
                self.erro(literal.token, "literal float fora da faixa")
            return "float"
        if tipo_token == "CARACTERE":
            return "char"
        if tipo_token == "TEXTO":
            return "string"
        literal.valor = tipo_token == "true"
        return "bool"

    def verificar_chamada(self, chamada):
        nome = chamada.token.texto
        funcao = self.funcoes.get(nome)
        if funcao is None:
            self.erro(chamada.token, f"função {nome!r} não declarada")
        esperados = len(funcao.parametros)
        recebidos = len(chamada.argumentos)
        if esperados != recebidos:
            self.erro(chamada.token, f"{nome!r} espera {esperados} argumento(s), recebeu {recebidos}")
        for parametro, argumento in zip(funcao.parametros, chamada.argumentos):
            self.conferir_atribuicao(parametro.tipo, argumento)
        chamada.nome_c = funcao.nome_c
        return funcao.retorno

    def verificar_binario(self, expressao):
        op = expressao.token.tipo
        esquerdo = self.verificar_expressao(expressao.esquerda)
        direito = self.verificar_expressao(expressao.direita)
        ambos_numericos = esquerdo in NUMERICOS and direito in NUMERICOS

        if op == "+" and "string" in (esquerdo, direito):
            return "string"
        if op in ("+", "-", "*", "/") and ambos_numericos:
            return "float" if "float" in (esquerdo, direito) else "int"
        if op == "%" and esquerdo == direito == "int":
            return "int"
        if op in ("<", "<=", ">", ">=") and ambos_numericos:
            return "bool"
        if op in ("==", "!=") and (esquerdo == direito or ambos_numericos):
            return "bool"
        if op in ("&&", "||") and esquerdo == direito == "bool":
            return "bool"
        self.erro(expressao.token, f"operador {op!r} não aceita {esquerdo} e {direito}")

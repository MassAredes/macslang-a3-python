"""Fase 2: reconhece a gramática e monta a árvore do programa."""

import nos
from erros import ErroCompilacao
from lexer import TIPOS


# Cada linha tem operadores de mesma prioridade. Os últimos ligam mais forte.
PRECEDENCIA = (
    ("||",), ("&&",), ("==", "!="), ("<", "<=", ">", ">="),
    ("+", "-"), ("*", "/", "%"),
)


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.indice = 0

    def atual(self):
        return self.tokens[self.indice]

    def avancar(self):
        token = self.atual()
        self.indice += 1
        return token

    def aceitar(self, tipo):
        if self.atual().tipo == tipo:
            return self.avancar()
        return None

    def exigir(self, tipo):
        token = self.aceitar(tipo)
        if token is None:
            self.erro(f"esperado {tipo!r}")
        return token

    def erro(self, mensagem):
        token = self.atual()
        encontrado = repr(token.texto) if token.texto else "fim do arquivo"
        raise ErroCompilacao("sintático", token, f"{mensagem}; encontrado {encontrado}")

    def analisar(self):
        funcoes = []
        comandos = []
        while self.atual().tipo != "EOF":
            if self.aceitar("func"):
                funcoes.append(self.funcao())
            else:
                comandos.append(self.comando())
        return nos.Programa(funcoes, comandos)

    def ler_tipo(self):
        if self.atual().tipo not in TIPOS:
            self.erro("esperado um tipo (int, float, char, bool ou string)")
        return self.avancar().tipo

    def funcao(self):
        nome = self.exigir("IDENT")
        self.exigir("(")
        parametros = []
        if self.atual().tipo != ")":
            while True:
                parametro = self.exigir("IDENT")
                self.exigir(":")
                parametros.append(nos.Parametro(parametro, self.ler_tipo()))
                if not self.aceitar(","):
                    break
        self.exigir(")")
        self.exigir(":")
        retorno = self.ler_tipo()
        return nos.Funcao(nome, parametros, retorno, self.bloco())

    def bloco(self):
        abertura = self.exigir("{")
        comandos = []
        while self.atual().tipo not in ("}", "EOF"):
            comandos.append(self.comando())
        self.exigir("}")
        return nos.Bloco(abertura, comandos)

    def comando(self):
        token = self.atual()
        tipo = token.tipo
        if tipo == "{":
            return self.bloco()

        # Estes três comandos terminam em um bloco com chaves.
        if tipo == "if":
            self.avancar()
            condicao = self.expressao_entre_parenteses()
            corpo = self.bloco()
            alternativa = self.bloco() if self.aceitar("else") else None
            return nos.Condicional(token, condicao, corpo, alternativa)
        if tipo == "while":
            self.avancar()
            condicao = self.expressao_entre_parenteses()
            return nos.Enquanto(token, condicao, self.bloco())
        if tipo == "for":
            return self.laco_para()

        # Os demais comandos precisam de um ponto e vírgula ao final.
        if tipo == "var":
            resultado = self.declaracao()
        elif tipo == "print":
            self.avancar()
            resultado = nos.Saida(token, self.expressao_entre_parenteses())
        elif tipo == "input":
            self.avancar()
            self.exigir("(")
            resultado = nos.Entrada(token, nos.Variavel(self.exigir("IDENT")))
            self.exigir(")")
        elif tipo == "return":
            self.avancar()
            resultado = nos.Retorno(token, self.expressao())
        elif tipo == "IDENT":
            nome = self.avancar()
            if self.atual().tipo == "(":
                resultado = nos.ExecutarChamada(nome, self.chamada(nome))
            else:
                resultado = self.atribuicao(nome)
        else:
            self.erro("esperado um comando")
        self.exigir(";")
        return resultado

    def declaracao(self):
        token = self.exigir("var")
        nome = self.exigir("IDENT")
        self.exigir(":")
        tipo = self.ler_tipo()
        valor = self.expressao() if self.aceitar("=") else None
        return nos.Declaracao(token, nome, tipo, valor)

    def atribuicao(self, nome):
        sinal = self.exigir("=")
        return nos.Atribuicao(sinal, nos.Variavel(nome), self.expressao())

    def laco_para(self):
        token = self.exigir("for")
        self.exigir("(")

        # No cabeçalho do for, os separadores pertencem ao próprio laço.
        # Assim podemos reutilizar declaracao e atribuicao sem consumir ';' nelas.
        inicio = None
        if self.atual().tipo == "var":
            inicio = self.declaracao()
        elif self.atual().tipo != ";":
            inicio = self.atribuicao(self.exigir("IDENT"))
        self.exigir(";")
        condicao = self.expressao() if self.atual().tipo != ";" else None
        self.exigir(";")
        atualizacao = None
        if self.atual().tipo != ")":
            atualizacao = self.atribuicao(self.exigir("IDENT"))
        self.exigir(")")
        return nos.Para(token, inicio, condicao, atualizacao, self.bloco())

    def expressao_entre_parenteses(self):
        self.exigir("(")
        resultado = self.expressao()
        self.exigir(")")
        return resultado

    def expressao(self, nivel=0):
        if nivel == len(PRECEDENCIA):
            return self.unario()

        # Primeiro resolvemos operadores mais fortes. Em 2 + 3 * 4,
        # o lado direito da soma chega aqui como a subárvore de 3 * 4.
        esquerda = self.expressao(nivel + 1)
        while self.atual().tipo in PRECEDENCIA[nivel]:
            operador = self.avancar()
            direita = self.expressao(nivel + 1)
            esquerda = nos.Binario(operador, esquerda, direita)
        return esquerda

    def unario(self):
        if self.atual().tipo in ("!", "-"):
            operador = self.avancar()
            return nos.Unario(operador, self.unario())
        return self.primario()

    def primario(self):
        token = self.atual()
        if token.tipo in ("INTEIRO", "REAL", "CARACTERE", "TEXTO", "true", "false"):
            self.avancar()
            valor = token.texto if token.valor is None else token.valor
            return nos.Literal(token, valor)
        if token.tipo == "(":
            return self.expressao_entre_parenteses()
        if self.aceitar("IDENT"):
            if self.atual().tipo == "(":
                return self.chamada(token)
            return nos.Variavel(token)
        self.erro("esperado um número, texto, variável, chamada ou outra expressão")

    def chamada(self, nome):
        self.exigir("(")
        argumentos = []
        if self.atual().tipo != ")":
            while True:
                argumentos.append(self.expressao())
                if not self.aceitar(","):
                    break
        self.exigir(")")
        return nos.Chamada(nome, argumentos)

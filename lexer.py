"""Fase 1: transforma os caracteres do fonte em uma lista de tokens."""

from erros import ErroCompilacao, Token


TIPOS = {"int", "float", "char", "bool", "string"}
RESERVADAS = TIPOS | {
    "var", "func", "return", "if", "else", "while", "for", "print", "input",
    "true", "false",
}
OPERADORES_DUPLOS = {"==", "!=", "<=", ">=", "&&", "||"}
SIMBOLOS = "+-*/%=!<>(){}:;,"
ESCAPES = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'"}


def digito(caractere):
    return "0" <= caractere <= "9"


def letra(caractere):
    return caractere == "_" or "a" <= caractere <= "z" or "A" <= caractere <= "Z"


class Lexer:
    def __init__(self, fonte):
        self.fonte = fonte
        self.indice = 0
        self.linha = 1
        self.coluna = 1

    def olhar(self, deslocamento=0):
        indice = self.indice + deslocamento
        return self.fonte[indice:indice + 1]

    def avancar(self):
        caractere = self.olhar()
        self.indice += 1
        if caractere == "\n":
            self.linha += 1
            self.coluna = 1
        else:
            self.coluna += 1
        return caractere

    def analisar(self):
        tokens = []
        while self.olhar():
            caractere = self.olhar()
            inicio = Token("", caractere, self.linha, self.coluna)

            if caractere in " \t\r\n":
                self.avancar()
            elif self.fonte.startswith("//", self.indice):
                while self.olhar() and self.olhar() != "\n":
                    self.avancar()
            elif self.fonte.startswith("/*", self.indice):
                self.pular_comentario(inicio)
            else:
                tokens.append(self.ler_token(inicio))

        # O parser sempre encontra um token de fim, mesmo para um arquivo vazio.
        tokens.append(Token("EOF", "", self.linha, self.coluna))
        return tokens

    def pular_comentario(self, inicio):
        self.avancar()
        self.avancar()
        while self.olhar() and not self.fonte.startswith("*/", self.indice):
            self.avancar()
        if not self.olhar():
            raise ErroCompilacao("léxico", inicio, "comentário de bloco sem fechamento")
        self.avancar()
        self.avancar()

    def ler_token(self, inicio):
        primeiro = self.olhar()
        posicao = self.indice
        if letra(primeiro):
            while letra(self.olhar()) or digito(self.olhar()):
                self.avancar()
            texto = self.fonte[posicao:self.indice]
            inicio.tipo = texto if texto in RESERVADAS else "IDENT"
        elif digito(primeiro):
            inicio.tipo = "INTEIRO"
            while digito(self.olhar()):
                self.avancar()
            # O ponto só faz parte do número se houver um dígito depois dele.
            if self.olhar() == "." and digito(self.olhar(1)):
                inicio.tipo = "REAL"
                self.avancar()
                while digito(self.olhar()):
                    self.avancar()
        elif primeiro in ('"', "'"):
            inicio.tipo = "TEXTO" if primeiro == '"' else "CARACTERE"
            inicio.valor = self.ler_literal(inicio, primeiro)
        else:
            par = primeiro + self.olhar(1)
            if par in OPERADORES_DUPLOS:
                inicio.tipo = par
                self.avancar()
                self.avancar()
            elif primeiro in SIMBOLOS:
                inicio.tipo = self.avancar()
            else:
                raise ErroCompilacao("léxico", inicio, f"símbolo inesperado {primeiro!r}")

        inicio.texto = self.fonte[posicao:self.indice]
        return inicio

    def ler_literal(self, inicio, aspas):
        self.avancar()  # Pula a primeira aspa.
        conteudo = []
        while self.olhar() not in ("", "\r", "\n", aspas):
            caractere = self.avancar()
            if caractere == "\0":
                raise ErroCompilacao("léxico", inicio, "literal contém byte nulo")
            if caractere == "\\":
                escape = self.olhar()
                if escape not in ESCAPES:
                    raise ErroCompilacao("léxico", inicio, f"escape inválido: {escape!r}")
                self.avancar()
                caractere = ESCAPES[escape]
            elif aspas == "'" and not " " <= caractere <= "~":
                raise ErroCompilacao("léxico", inicio, "char deve ser um caractere ASCII")
            conteudo.append(caractere)

        if self.olhar() != aspas:
            raise ErroCompilacao("léxico", inicio, "literal sem aspas de fechamento")
        self.avancar()
        if aspas == "'" and len(conteudo) != 1:
            raise ErroCompilacao("léxico", inicio, "char deve ter um único caractere")
        return "".join(conteudo)

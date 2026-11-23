"""Nós da árvore sintática abstrata (AST).

A árvore guarda a estrutura do programa. Uma soma, por exemplo, guarda um
operador e duas expressões. A semântica preenche os campos tipo e nome_c.
"""

from __future__ import annotations

from dataclasses import dataclass, field, fields

from erros import Token


@dataclass
class No:
    token: Token  # Indica a posição original para mostrar erros.


@dataclass
class Expressao(No):
    tipo: str = field(default="", init=False)


@dataclass
class Literal(Expressao):
    valor: str | int | float | bool


@dataclass
class Variavel(Expressao):
    nome_c: str = field(default="", init=False)


@dataclass
class Chamada(Expressao):
    argumentos: list[Expressao]
    nome_c: str = field(default="", init=False)


@dataclass
class Unario(Expressao):
    operando: Expressao


@dataclass
class Binario(Expressao):
    esquerda: Expressao
    direita: Expressao


@dataclass
class Comando(No):
    pass


@dataclass
class Bloco(Comando):
    comandos: list[Comando]


@dataclass
class Declaracao(Comando):
    nome: Token
    tipo: str
    valor: Expressao | None
    nome_c: str = field(default="", init=False)


@dataclass
class Atribuicao(Comando):
    destino: Variavel
    valor: Expressao


@dataclass
class Entrada(Comando):
    destino: Variavel


@dataclass
class Saida(Comando):
    valor: Expressao


@dataclass
class Retorno(Comando):
    valor: Expressao


@dataclass
class ExecutarChamada(Comando):
    chamada: Chamada


@dataclass
class Condicional(Comando):
    condicao: Expressao
    corpo: Bloco
    alternativa: Bloco | None


@dataclass
class Enquanto(Comando):
    condicao: Expressao
    corpo: Bloco


@dataclass
class Para(Comando):
    inicio: Declaracao | Atribuicao | None
    condicao: Expressao | None
    atualizacao: Atribuicao | None
    corpo: Bloco


@dataclass
class Parametro(No):
    tipo: str
    nome_c: str = field(default="", init=False)


@dataclass
class Funcao(No):
    parametros: list[Parametro]
    retorno: str
    corpo: Bloco
    nome_c: str = field(default="", init=False)


@dataclass
class Programa:
    funcoes: list[Funcao]
    comandos: list[Comando]


def mostrar_arvore(no, nivel=0):
    """Mostra um nó e depois seus filhos, com recuo para cada nível."""
    recuo = "  " * nivel
    rotulo = type(no).__name__
    if isinstance(no, No):
        rotulo += f" {no.token.texto!r} (linha {no.token.linha})"
    print(recuo + rotulo)

    for campo in fields(no):
        valor = getattr(no, campo.name)
        if campo.name == "token" or valor is None or valor == "" or valor == []:
            continue
        if isinstance(valor, No):
            print(f"{recuo}  {campo.name}:")
            mostrar_arvore(valor, nivel + 2)
        elif isinstance(valor, list):
            print(f"{recuo}  {campo.name}:")
            for filho in valor:
                mostrar_arvore(filho, nivel + 2)
        else:
            print(f"{recuo}  {campo.name}: {valor!r}")

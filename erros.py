"""Posição no fonte e diagnósticos compartilhados pelas fases."""

from dataclasses import dataclass


@dataclass
class Token:
    tipo: str
    texto: str
    linha: int
    coluna: int
    # Para strings e chars, guarda o conteúdo com os escapes já interpretados.
    valor: str | None = None


class ErroCompilacao(Exception):
    def __init__(self, fase, token, mensagem):
        super().__init__(
            f"Erro {fase} na linha {token.linha}, coluna {token.coluna}: {mensagem}"
        )

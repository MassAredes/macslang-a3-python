"""Entrada do compilador. Coordena as quatro fases e chama o cl.exe."""

import argparse
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory

from erros import ErroCompilacao
from gerador_c import GeradorC
from lexer import Lexer
from nos import mostrar_arvore
from parser import Parser
from semantico import Semantico


RUNTIME = Path(__file__).resolve().parent / "runtime"


def ler_argumentos():
    parser_cli = argparse.ArgumentParser(
        description="Compilador didático MACSLang para Windows x86."
    )
    parser_cli.add_argument("fonte", type=Path, help="arquivo .macs")
    opcoes = parser_cli.add_mutually_exclusive_group()
    opcoes.add_argument("--tokens", action="store_true", help="mostra a análise léxica")
    opcoes.add_argument("--ast", action="store_true", help="mostra a árvore sintática")
    opcoes.add_argument("--check", action="store_true", help="verifica sintaxe e semântica")
    opcoes.add_argument("--emit-c", type=Path, metavar="ARQUIVO.c", help="grava somente o C11")
    opcoes.add_argument("-o", type=Path, metavar="ARQUIVO.exe", help="escolhe a saída executável")
    return parser_cli.parse_args()


def verificar_x86(executavel):
    """Lê a assinatura e o campo Machine do cabeçalho PE do Windows."""
    with executavel.open("rb") as arquivo:
        cabecalho = arquivo.read(64)
        if len(cabecalho) < 64 or cabecalho[:2] != b"MZ":
            raise RuntimeError("o linker criou um arquivo sem cabeçalho PE")

        # Os bytes 60 a 63 guardam a posição do cabeçalho PE.
        posicao_pe = struct.unpack_from("<I", cabecalho, 60)[0]
        arquivo.seek(posicao_pe)
        if arquivo.read(6) != b"PE\x00\x00\x4c\x01":
            raise RuntimeError("cabeçalho diferente de PE x86 (Machine 0x014C)")


def gerar_executavel(codigo, executavel):
    destino_c = executavel.with_suffix(".c")
    conferir_destino_c(destino_c)
    executavel.parent.mkdir(parents=True, exist_ok=True)

    # Trabalhamos numa pasta temporária. Isso mantém os .obj fora da entrega
    # e preserva a última compilação válida se o cl.exe falhar.
    with TemporaryDirectory(prefix="macs-", dir=executavel.parent) as pasta:
        arquivo_c = Path(pasta) / "gerado.c"
        arquivo_c.write_text(codigo, encoding="utf-8")
        temporario = Path(pasta) / "programa.exe"
        comando = [
            "cl.exe", "/nologo", "/TC", "/std:c11", "/utf-8", "/Od",
            "/I", str(RUNTIME), str(arquivo_c), str(RUNTIME / "macs_runtime.c"),
            f"/Fe:{temporario}", "/link", "/MACHINE:X86",
        ]
        try:
            subprocess.run(comando, cwd=pasta, check=True)
        except FileNotFoundError as erro:
            raise RuntimeError("cl.exe indisponível. Abra o x86 Native Tools Command Prompt.") from erro
        except subprocess.CalledProcessError as erro:
            raise RuntimeError(
                f"cl.exe encerrou com código {erro.returncode}. Confira as mensagens acima "
                "e se o terminal está configurado para x86."
            ) from erro

        verificar_x86(temporario)
        temporario.replace(executavel)
        arquivo_c.replace(destino_c)
    print(f"C11 gerado em {destino_c}")


def conferir_destino_c(destino):
    if destino.resolve() == RUNTIME / "macs_runtime.c":
        raise RuntimeError("escolha um destino diferente do arquivo de runtime")


def salvar_c(destino, codigo):
    conferir_destino_c(destino)
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_text(codigo, encoding="utf-8")
    print(f"C11 gerado em {destino}", flush=True)


def compilar(opcoes):
    if opcoes.fonte.suffix.lower() != ".macs":
        raise RuntimeError("use um arquivo-fonte com extensão .macs")

    # Fase 1: separa caracteres em tokens. utf-8-sig também aceita um BOM.
    fonte = opcoes.fonte.read_text(encoding="utf-8-sig")
    tokens = Lexer(fonte).analisar()
    if opcoes.tokens:
        for token in tokens:
            print(f"{token.linha}:{token.coluna:<4} {token.tipo:<12} {token.texto!r}")
        return

    # Fase 2: identifica a estrutura do texto.
    programa = Parser(tokens).analisar()
    if opcoes.ast:
        mostrar_arvore(programa)
        return

    # Fase 3: confere as regras de tipos e anota a árvore para o gerador.
    Semantico().analisar(programa)
    if opcoes.check:
        print("OK: análises léxica, sintática e semântica concluídas.")
        return

    # Fase 4: gera C. O cl.exe só entra na etapa que cria o executável.
    codigo = GeradorC().gerar(programa)
    if opcoes.emit_c is not None:
        if opcoes.emit_c.suffix.lower() != ".c":
            raise RuntimeError("a saída de --emit-c precisa ter extensão .c")
        salvar_c(opcoes.emit_c, codigo)
        return

    executavel = opcoes.o or Path("build") / (opcoes.fonte.stem + ".exe")
    if executavel.suffix == "":
        executavel = executavel.with_suffix(".exe")
    if executavel.suffix.lower() != ".exe":
        raise RuntimeError("a saída de -o precisa ter extensão .exe")
    executavel = executavel.resolve()
    gerar_executavel(codigo, executavel)
    print(f"Executável Windows x86 gerado em {executavel}")


def main():
    opcoes = ler_argumentos()
    try:
        compilar(opcoes)
    except ErroCompilacao as erro:
        print(erro, file=sys.stderr)
        return 1
    except UnicodeDecodeError:
        print("Erro: o arquivo-fonte precisa estar em UTF-8.", file=sys.stderr)
        return 1
    except RecursionError:
        print("Erro: a expressão ou o bloco está aninhado além do limite.", file=sys.stderr)
        return 1
    except (OSError, RuntimeError) as erro:
        print(f"Erro: {erro}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

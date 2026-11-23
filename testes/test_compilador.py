"""Testes das fases em Python e do fluxo até o executável Windows x86."""

from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest

from erros import ErroCompilacao
from gerador_c import GeradorC
from lexer import Lexer
from main import verificar_x86
from nos import Binario
from parser import Parser
from semantico import Semantico


PROJETO = Path(__file__).resolve().parents[1]
EXEMPLOS = PROJETO / "exemplos"


def analisar(fonte):
    tokens = Lexer(fonte).analisar()
    programa = Parser(tokens).analisar()
    Semantico().analisar(programa)
    return programa


def chamar_compilador(fonte, *opcoes, pasta=PROJETO):
    return subprocess.run(
        [sys.executable, "-B", "-X", "utf8", str(PROJETO / "main.py"), str(fonte), *opcoes],
        cwd=pasta, capture_output=True, encoding="utf-8",
    )


def rodar_programa(executavel, entrada):
    return subprocess.run(
        [str(executavel)], input=entrada, capture_output=True, encoding="utf-8", timeout=10,
    )


class TesteFases(unittest.TestCase):
    def test_tokens_comentarios_e_escapes(self):
        fonte = r'/* ignora */ print("olá\n");' + "\r\n// ignora\r\nvar n: int = 5;"
        tokens = Lexer(fonte).analisar()
        self.assertEqual([token.tipo for token in tokens[:5]], ["print", "(", "TEXTO", ")", ";"])
        self.assertEqual(tokens[2].valor, "olá\n")
        self.assertEqual((tokens[5].linha, tokens[5].coluna), (3, 1))

    def test_precedencia_da_multiplicacao(self):
        programa = analisar("print(2 + 3 * 4);")
        expressao = programa.comandos[0].valor
        self.assertIsInstance(expressao, Binario)
        self.assertEqual(expressao.token.tipo, "+")
        self.assertEqual(expressao.direita.token.tipo, "*")
        self.assertEqual(expressao.tipo, "int")
        self.assertIn("(INT64_C(2) + (INT64_C(3) * INT64_C(4)))", GeradorC().gerar(programa))

    def test_erros_lexicos_e_sintaticos(self):
        casos = [
            ('print("texto);', "léxico"),
            ("/* sem fechar", "léxico"),
            ("print('ab');", "léxico"),
            ("if (true) { print(1);", "sintático"),
            ("var numero int;", "sintático"),
        ]
        for fonte, fase in casos:
            with self.subTest(fonte=fonte):
                with self.assertRaisesRegex(ErroCompilacao, f"Erro {fase} na linha"):
                    analisar(fonte)

    def test_erros_semanticos(self):
        casos = [
            'var valor: int = "sete";',
            "var valor: int = 1.5;",
            "var valor: int; var valor: int;",
            "print(inexistente);",
            "if (12) { print(1); }",
            "for (var i: int = 0; i < 2; i = i + 1) {} print(i);",
            "func soma(x: int): int { return x; } print(soma());",
            "func valor(): int { if (true) { return 1; } }",
            "var valor: int = 5; func ler(): int { return valor; }",
            "return 1;",
            "print(9223372036854775808);",
        ]
        for fonte in casos:
            with self.subTest(fonte=fonte):
                with self.assertRaisesRegex(ErroCompilacao, "Erro semântico na linha"):
                    analisar(fonte)

    def test_comandos_para_inspecao(self):
        fonte = EXEMPLOS / "fatorial.macs"
        for opcao, esperado in [("--tokens", "IDENT"), ("--ast", "Para 'for'"), ("--check", "OK:")]:
            with self.subTest(opcao=opcao):
                resultado = chamar_compilador(fonte, opcao)
                self.assertEqual(resultado.returncode, 0, resultado.stderr)
                self.assertIn(esperado, resultado.stdout)

        with TemporaryDirectory(prefix="macslang testes ") as pasta:
            caminho_c = Path(pasta) / "saida em C.c"
            resultado = chamar_compilador(fonte, "--emit-c", str(caminho_c))
            self.assertEqual(resultado.returncode, 0, resultado.stderr)
            self.assertIn("int main(void)", caminho_c.read_text(encoding="utf-8"))
            self.assertFalse(caminho_c.with_suffix(".exe").exists())

    def test_diagnosticos_dos_exemplos_invalidos(self):
        casos = [("simbolo", "léxico"), ("pontuacao", "sintático"), ("tipo", "semântico")]
        for nome, fase in casos:
            with self.subTest(nome=nome):
                resultado = chamar_compilador(EXEMPLOS / "invalidos" / f"{nome}.macs", "--check")
                self.assertEqual(resultado.returncode, 1)
                self.assertIn(f"Erro {fase} na linha", resultado.stderr)


@unittest.skipUnless(shutil.which("cl.exe"), "abra o terminal x86 do Visual Studio")
class TesteWindowsX86(unittest.TestCase):
    def compilar(self, fonte, pasta):
        executavel = pasta / "saida de teste.exe"
        resultado = chamar_compilador(fonte, "-o", str(executavel))
        self.assertEqual(resultado.returncode, 0, resultado.stdout + resultado.stderr)
        self.assertTrue(executavel.with_suffix(".c").exists())
        verificar_x86(executavel)
        return executavel

    def test_fatorial_saida_padrao_e_entrada_invalida(self):
        with TemporaryDirectory(prefix="macslang teste x86 ") as pasta:
            resultado = chamar_compilador(EXEMPLOS / "fatorial.macs", pasta=pasta)
            self.assertEqual(resultado.returncode, 0, resultado.stdout + resultado.stderr)
            executavel = Path(pasta) / "build" / "fatorial.exe"
            self.assertTrue(executavel.with_suffix(".c").exists())
            verificar_x86(executavel)
            for numero, esperado in [(0, 1), (1, 1), (5, 120), (20, 2432902008176640000)]:
                with self.subTest(numero=numero):
                    teste = rodar_programa(executavel, f"{numero}\n")
                    self.assertEqual(teste.returncode, 0, teste.stderr)
                    self.assertIn(f"O fatorial de {numero} é {esperado}\n", teste.stdout)

            invalido = rodar_programa(executavel, "abc\n")
            self.assertNotEqual(invalido.returncode, 0)
            self.assertIn("entrada inválida para int", invalido.stderr)

    def test_calculadora_e_media(self):
        with TemporaryDirectory(prefix="macslang teste x86 ") as pasta:
            pasta = Path(pasta)
            executavel = self.compilar(EXEMPLOS / "calculadora.macs", pasta)
            for entrada, esperado in [("7\n", "Dobro: 14"), ("-3\n", "Número negativo. Dobro: -6")]:
                resultado = rodar_programa(executavel, entrada)
                self.assertEqual(resultado.returncode, 0, resultado.stderr)
                self.assertIn(esperado, resultado.stdout)

            executavel = self.compilar(EXEMPLOS / "media.macs", pasta)
            resultado = rodar_programa(executavel, "Ana Maria\n6\n7\n8\n")
            self.assertEqual(resultado.returncode, 0, resultado.stderr)
            self.assertIn("Ana Maria teve média 7\nSituação: A\nAprovado: true", resultado.stdout)

    def test_escopo_recursao_e_entradas(self):
        with TemporaryDirectory(prefix="macslang teste x86 ") as pasta:
            pasta = Path(pasta)
            fonte = pasta / "programa de teste.macs"
            fonte.write_text(
                '''print(fib(10));
func fib(n: int): int {
    if (n < 2) { return n; } else { return fib(n - 1) + fib(n - 2); }
}
var letra: char;
var ligado: bool;
var texto: string;
input(letra);
input(ligado);
input(texto);
if (ligado && texto == "Café com leite") { print(letra); }
var n: int = 0;
for (; n < 1; n = n + 1) {
    var n: int = 5;
    print(n);
}
print(n);
''',
                encoding="utf-8",
            )
            executavel = self.compilar(fonte, pasta)
            resultado = rodar_programa(executavel, "A\ntrue\nCafé com leite\n")
            self.assertEqual(resultado.returncode, 0, resultado.stderr)
            self.assertEqual(resultado.stdout, "55\nA\n5\n1\n")

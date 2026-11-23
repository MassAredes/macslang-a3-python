"""Fase 4: percorre a árvore verificada e escreve o programa em C11."""

import nos


TIPOS_C = {
    "int": "int64_t", "float": "double", "char": "char",
    "bool": "bool", "string": "const char *",
}
VALORES_INICIAIS = {
    "int": "0", "float": "0.0", "char": "0", "bool": "false", "string": '""',
}
ESCAPES_C = {
    "\\": "\\\\", '"': '\\"', "\n": "\\n", "\r": "\\r", "\t": "\\t", "?": "\\?",
}


def string_c(texto):
    """Coloca aspas e protege caracteres que teriam significado no C."""
    partes = ['"']
    for caractere in texto:
        if caractere in ESCAPES_C:
            partes.append(ESCAPES_C[caractere])
        elif ord(caractere) < 32 or ord(caractere) == 127:
            partes.append(f"\\{ord(caractere):03o}")
        else:
            partes.append(caractere)
    partes.append('"')
    return "".join(partes)


class GeradorC:
    def __init__(self):
        self.linhas = []
        self.nivel = 0

    def linha(self, texto):
        self.linhas.append("    " * self.nivel + texto)

    def gerar(self, programa):
        self.linha("/* Código C11 gerado pelo compilador MACSLang. */")
        self.linha('#include "macs_runtime.h"')
        self.linha("")

        # Os protótipos deixam o C conhecer as funções antes de seus corpos.
        for funcao in programa.funcoes:
            self.linha(self.assinatura(funcao) + ";")
        if programa.funcoes:
            self.linha("")
        for funcao in programa.funcoes:
            self.linha(self.assinatura(funcao))
            self.bloco(funcao.corpo)
            self.linha("")

        # Comandos fora de uma função entram no ponto de entrada do executável.
        self.linha("int main(void)")
        self.linha("{")
        self.nivel += 1
        self.linha("macs_iniciar();")
        for comando in programa.comandos:
            self.comando(comando)
        self.linha("return 0;")
        self.nivel -= 1
        self.linha("}")
        return "\n".join(self.linhas) + "\n"

    def assinatura(self, funcao):
        parametros = []
        for parametro in funcao.parametros:
            parametros.append(f"{TIPOS_C[parametro.tipo]} {parametro.nome_c}")
        argumentos = ", ".join(parametros) or "void"
        return f"{TIPOS_C[funcao.retorno]} {funcao.nome_c}({argumentos})"

    def bloco(self, bloco):
        self.linha("{")
        self.nivel += 1
        for comando in bloco.comandos:
            self.comando(comando)
        self.nivel -= 1
        self.linha("}")

    def comando(self, comando):
        if isinstance(comando, nos.Bloco):
            self.bloco(comando)
        elif isinstance(comando, (nos.Declaracao, nos.Atribuicao)):
            self.linha(self.declaracao_ou_atribuicao(comando) + ";")
        elif isinstance(comando, nos.Entrada):
            destino = comando.destino
            self.linha(f"{destino.nome_c} = macs_ler_{destino.tipo}();")
        elif isinstance(comando, nos.Saida):
            self.imprimir(comando.valor)
        elif isinstance(comando, nos.Retorno):
            self.linha(f"return {self.expressao(comando.valor)};")
        elif isinstance(comando, nos.ExecutarChamada):
            self.linha(self.expressao(comando.chamada) + ";")
        elif isinstance(comando, nos.Condicional):
            self.linha(f"if ({self.expressao(comando.condicao)})")
            self.bloco(comando.corpo)
            if comando.alternativa is not None:
                self.linha("else")
                self.bloco(comando.alternativa)
        elif isinstance(comando, nos.Enquanto):
            self.linha(f"while ({self.expressao(comando.condicao)})")
            self.bloco(comando.corpo)
        elif isinstance(comando, nos.Para):
            inicio = self.declaracao_ou_atribuicao(comando.inicio)
            atualizacao = self.declaracao_ou_atribuicao(comando.atualizacao)
            condicao = self.expressao(comando.condicao) if comando.condicao is not None else ""
            self.linha(f"for ({inicio}; {condicao}; {atualizacao})")
            self.bloco(comando.corpo)
        else:
            raise ValueError(f"comando desconhecido: {comando!r}")

    def declaracao_ou_atribuicao(self, comando):
        """Gera o trecho sem ';' para poder usá-lo também no cabeçalho do for."""
        if comando is None:
            return ""
        if isinstance(comando, nos.Declaracao):
            if comando.valor is None:
                valor = VALORES_INICIAIS[comando.tipo]
            else:
                valor = self.expressao(comando.valor)
            return f"{TIPOS_C[comando.tipo]} {comando.nome_c} = {valor}"
        return f"{comando.destino.nome_c} = {self.expressao(comando.valor)}"

    def imprimir(self, expressao):
        codigo = self.expressao(expressao)
        if expressao.tipo in ("string", "bool"):
            self.linha(f"puts({self.como_texto(codigo, expressao.tipo)});")
        elif expressao.tipo == "int":
            self.linha(f'printf("%" PRId64 "\\n", {codigo});')
        elif expressao.tipo == "float":
            self.linha(f'printf("%.15g\\n", {codigo});')
        else:
            self.linha(f'printf("%c\\n", {codigo});')

    def como_texto(self, codigo, tipo):
        if tipo == "string":
            return codigo
        return f"macs_texto_{tipo}({codigo})"

    def expressao(self, expressao):
        if isinstance(expressao, nos.Literal):
            return self.literal(expressao)
        if isinstance(expressao, nos.Variavel):
            return expressao.nome_c
        if isinstance(expressao, nos.Chamada):
            argumentos = ", ".join(self.expressao(item) for item in expressao.argumentos)
            return f"{expressao.nome_c}({argumentos})"
        if isinstance(expressao, nos.Unario):
            return f"({expressao.token.tipo}{self.expressao(expressao.operando)})"
        if isinstance(expressao, nos.Binario):
            return self.binario(expressao)
        raise ValueError(f"expressão desconhecida: {expressao!r}")

    def literal(self, literal):
        if literal.tipo == "string":
            return string_c(literal.valor)
        if literal.tipo == "char":
            return f"((char){ord(literal.valor)})"
        if literal.tipo == "bool":
            return "true" if literal.valor else "false"
        if literal.tipo == "int":
            # Mesmo no Windows x86, o int da MACSLang é de 64 bits.
            return f"INT64_C({literal.valor})"
        return repr(literal.valor)

    def binario(self, expressao):
        op = expressao.token.tipo
        esquerda = self.expressao(expressao.esquerda)
        direita = self.expressao(expressao.direita)
        if expressao.tipo == "string":
            esquerda = self.como_texto(esquerda, expressao.esquerda.tipo)
            direita = self.como_texto(direita, expressao.direita.tipo)
            return f"macs_juntar({esquerda}, {direita})"
        if op in ("==", "!=") and expressao.esquerda.tipo == "string":
            # strcmp devolve zero quando o conteúdo das duas strings é igual.
            return f"(strcmp({esquerda}, {direita}) {op} 0)"
        # Os parênteses reproduzem a ordem que o parser registrou na árvore.
        return f"({esquerda} {op} {direita})"

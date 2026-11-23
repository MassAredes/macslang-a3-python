# Compilador MACSLang em Python

Projeto da atividade A3 de Teoria da Computação e Compiladores. O compilador lê um programa `.macs`, verifica suas regras e gera código C11. O `cl.exe` compila o C e liga as funções auxiliares para criar um executável **Windows x86 de 32 bits**.

```text
fonte .macs -> tokens -> árvore -> árvore verificada -> código C11 -> .exe x86
               lexer     parser    semântico          gerador       cl.exe
```

## 1. Preparar o terminal

Você precisa do Python 3.10 ou superior e das ferramentas de compilação em C do Visual Studio com suporte a x86. O ambiente usado para testar foi **Python 3.12 e Visual Studio Build Tools 2019**. O Python usa somente a biblioteca padrão.

1. No menu Iniciar, procure **x86 Native Tools Command Prompt for VS 2019** e abra esse terminal. Se você tem Visual Studio 2022, abra o atalho x86 da sua versão.
2. Entre na pasta onde está o `main.py`. Neste computador, use o comando abaixo.
3. Confira que `python --version` mostra a versão do Python e `where cl` encontra o compilador C.

```bat
cd /d "D:\Trabalho Compiladores\macslang-a3-python"
python --version
where cl
```

## 2. Compilar e rodar o fatorial

```bat
python main.py exemplos\fatorial.macs
build\fatorial.exe
```

Digite `5` e aperte Enter. A última linha deve ser `O fatorial de 5 é 120`. Teste também `0` e `1`, que dão `1`. O programa aceita entradas de `0` a `20`, faixa em que o resultado cabe em `int`.

O comando cria `build\fatorial.c` e `build\fatorial.exe`. O `.c` fica disponível para estudo. Para conferir a arquitetura, execute o comando a seguir e procure `14C machine (x86)`.

```bat
dumpbin /headers build\fatorial.exe | findstr /i machine
```

Por padrão, o nome dos arquivos de saída vem do nome do `.macs`. Para escolher outro caminho, use `-o`.

```bat
python main.py exemplos\fatorial.macs -o build\demo.exe
build\demo.exe
```

## 3. Ver cada fase em separado

Os comandos abaixo funcionam com Python em um terminal comum. O `cl.exe` é usado ao pedir o executável.

```bat
python main.py exemplos\fatorial.macs --tokens
python main.py exemplos\fatorial.macs --ast
python main.py exemplos\fatorial.macs --check
python main.py exemplos\fatorial.macs --emit-c build\inspecao.c
```

| Opção | O que procurar |
| --- | --- |
| `--tokens` | Uma linha por token, com posição, categoria e texto, como `func` e `IDENT`. |
| `--ast` | A árvore criada pelo parser. Encontre os nós `Funcao`, `Para` e `Retorno`. |
| `--check` | O `OK` indica que as três análises concluíram. Em caso de erro, aparece a fase, a linha e a coluna. |
| `--emit-c` | O arquivo C11. Procure `macs_funcao_1`, o `for` e o `main` gerado. |

## 4. Testar

| Programa | Entradas, uma por linha | Resultado esperado |
| --- | --- | --- |
| `exemplos/fatorial.macs` | `5` | `O fatorial de 5 é 120` |
| `exemplos/calculadora.macs` | `7` ou `-3` em execuções separadas | `Dobro: 14` ou `Número negativo. Dobro: -6` |
| `exemplos/media.macs` | `Ana Maria`, `6`, `7`, `8` | `Ana Maria teve média 7`, `Situação: A` e `Aprovado: true` |

Use `--check` para demonstrar as rejeições. Cada arquivo tem um único erro intencional.

```bat
python main.py exemplos\invalidos\simbolo.macs --check
python main.py exemplos\invalidos\pontuacao.macs --check
python main.py exemplos\invalidos\tipo.macs --check
```

O primeiro falha no **léxico** por causa de `@`. O segundo falha no **sintático** por causa do `;` ausente. O terceiro falha na **semântica** porque tenta inicializar um `int` com texto. Esses erros encerram a compilação antes de chamar o `cl.exe`.

Para executar os testes automáticos, use o comando abaixo. No terminal x86, a suíte também compila os exemplos, executa os binários e verifica o cabeçalho x86. Em um terminal sem `cl.exe`, os testes do compilador em Python rodam e a parte nativa é marcada como pulada.

```bat
python -m unittest discover -s testes -v
```

## 5. Encontrar o código

| Arquivo | Responsabilidade |
| --- | --- |
| `main.py` | Lê os argumentos, chama as fases na ordem e executa o compilador C. |
| `lexer.py` | Fase 1. Lê caracteres, reconhece tokens e acompanha linha e coluna. |
| `parser.py` | Fase 2. Organiza os tokens nos nós da árvore sintática. |
| `semantico.py` | Fase 3. Verifica tipos e escopos e associa cada uso à sua declaração. |
| `gerador_c.py` | Fase 4. Escreve C11 a partir da árvore verificada. |
| `nos.py` e `erros.py` | Definem a árvore, o token e as mensagens compartilhadas. |
| `runtime/` | Rotinas C para ler dados e trabalhar com texto enquanto o executável roda. |

O [guia de leitura](docs/COMO_FUNCIONA.md) acompanha um programa pequeno pelas quatro fases e detalha as regras da linguagem. A [explicação do código, bloco por bloco](docs/EXPLICACAO_DO_CODIGO.md) descreve os métodos, funções, exemplos e testes na ordem em que aparecem em cada arquivo.

## Se um comando falhar

| Mensagem ou sintoma | O que conferir |
| --- | --- |
| `cl.exe indisponível` | Abra **x86 Native Tools Command Prompt** e execute `where cl`. |
| `LNK1112` ou conflito de arquitetura | O terminal provavelmente foi inicializado para x64. Abra o atalho **x86**. |
| `C5105` em `winbase.h` | Aviso do SDK 10.0.19041 usado com VS 2019. Confira se o comando terminou com `Executável Windows x86 gerado`. |
| Arquivo `.macs` não encontrado | Use `dir` e confira se o terminal está na pasta do projeto e se o caminho do exemplo está correto. |
| `Erro de execução: entrada inválida` | Confira o tipo esperado. Para `float`, digite `7.5` com ponto. Para `bool`, use `true`, `false`, `1` ou `0`. |

## Material de referência

O PDF do Trabalho A3 do UniBH define o objetivo, as quatro fases e o alvo Windows x86. A gramática, os exemplos e a documentação do [projeto de Marco Antônio Calijorne Soares](https://github.com/ma-calijorne/macsLang-copiler-multiplatform) foram usados como referência da linguagem. Os módulos deste projeto são uma implementação em Python, com um runtime C próprio.

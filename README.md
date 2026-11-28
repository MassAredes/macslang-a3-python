# Compilador MACSLang em C

Projeto A3 da disciplina Teoria da Computação e Compiladores do UniBH. O compilador é escrito em C11 e gera programas executáveis para **Windows x86 de 32 bits**.

```text
fonte .macs → análise léxica → análise sintática → análise semântica → geração C11
                                                                            ↓
                                                                  cl.exe → .exe x86
```

## Compilar no Windows

Use o **x86 Native Tools Command Prompt** da sua instalação do Visual Studio. É preciso ter o componente de compilação C/C++ para x86 e o SDK do Windows. A versão conferida neste projeto é o Visual Studio Build Tools 2019, com MSVC 19.29 e Windows SDK 10.0.19041. O projeto usa bibliotecas da própria ferramenta e do Windows. [Ferramentas de linha de comando do Visual Studio](https://learn.microsoft.com/pt-br/cpp/build/building-on-the-command-line?view=msvc-170)

No computador em que este trabalho foi desenvolvido, use:

```bat
cd /d "D:\Trabalho Compiladores\macslang-a3-c"
where cl
compilar.bat
```

Se você copiar a pasta para outro computador, ajuste o `cd`. O script compila os arquivos `.c` e cria `build\macsc.exe`. `cl.exe` também é chamado quando o compilador MACSLang precisa transformar o C gerado em um `.exe`.

## Compilar e executar um programa

```bat
build\macsc.exe exemplos\fatorial.macs
build\fatorial.exe
```

Digite `5` e pressione Enter. A última linha será `O fatorial de 5 é 120`. A compilação grava `build\fatorial.c` e `build\fatorial.exe`. Para conferir a arquitetura:

```bat
dumpbin /headers build\fatorial.exe | findstr /i machine
```

Procure `14C machine (x86)`. Para selecionar outro nome de saída:

```bat
build\macsc.exe exemplos\fatorial.macs -o "build\meu fatorial.exe"
```

## Acompanhar as quatro fases

Execute os comandos um por vez. Os modos de inspeção podem ser usados com o compilador já construído, inclusive no terminal comum. A geração de executáveis usa o prompt x86 do Visual Studio.

```bat
build\macsc.exe exemplos\fatorial.macs --tokens
build\macsc.exe exemplos\fatorial.macs --ast
build\macsc.exe exemplos\fatorial.macs --check
build\macsc.exe exemplos\fatorial.macs --emit-c build\inspecao.c
```

| Opção | Resultado |
| --- | --- |
| `--tokens` | Mostra o texto, categoria, linha e coluna de cada token. |
| `--ast` | Mostra a árvore que o parser criou. |
| `--check` | Verifica as regras léxicas, sintáticas e semânticas. |
| `--emit-c arquivo.c` | Grava o C11 e encerra nessa fase. |
| `-o arquivo.exe` | Seleciona onde gravar o executável e o C de mesmo nome. |
| `--help` | Mostra as opções aceitas. |

## Exemplos e testes

| Fonte | Entrada, uma por linha | Saída esperada |
| --- | --- | --- |
| `exemplos\fatorial.macs` | `0`, `1` ou `5`, em execuções separadas | Fatorial igual a `1`, `1` ou `120`. |
| `exemplos\calculadora.macs` | `7` ou `-3` | `Dobro: 14` ou `Número negativo. Dobro: -6`. |
| `exemplos\media.macs` | `Ana Maria`, `6`, `7`, `8` | Média `7`, situação `A` e aprovação `true`. |
| `exemplos\invalidos\simbolo.macs` | Use `--check`. | Erro léxico no `@`. |
| `exemplos\invalidos\pontuacao.macs` | Use `--check`. | Erro sintático pela falta de `;`. |
| `exemplos\invalidos\tipo.macs` | Use `--check`. | Erro semântico ao atribuir texto a um inteiro. |

A suíte automatizada usa o PowerShell do Windows para chamar os executáveis e comparar as saídas. Execute no mesmo prompt x86:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File testes\testar.ps1
```

Para repetir a suíte com a verificação de vazamentos da biblioteca de depuração do MSVC:

```bat
compilar.bat memoria
powershell.exe -NoProfile -ExecutionPolicy Bypass -File testes\testar.ps1 -Memoria
```

O modo de memória cria `build\macsc-memoria.exe` e deixa o compilador normal disponível. Os executáveis produzidos nos testes usam o mesmo modo de memória do compilador que os gerou.

## Organização e estudo

- `src/` contém o compilador. As quatro fases estão em `lexer.c`, `parser.c`, `semantico.c` e `gerador.c`.
- `runtime/` contém as rotinas em C usadas pelo programa gerado quando ele roda.
- `exemplos/` contém fontes válidos e inválidos para demonstração.
- `testes/` contém a suíte e um programa de integração.
- [Como funciona](docs/COMO_FUNCIONA.md) acompanha uma conta pelas quatro fases.
- [Explicação do código](docs/EXPLICACAO_DO_CODIGO.md) explica as funções, estruturas e etapas de cada arquivo.
- [Relatório acadêmico](docs/RELATORIO.md) registra objetivo, conceitos, metodologia, decisões e referências.
- [Conformidade e resultados](docs/CONFORMIDADE_E_TESTES.md) relaciona cada exigência do enunciado com a evidência de validação.

## Quando aparecer um erro

| Situação | O que a mensagem indica |
| --- | --- |
| `cl.exe indisponível` | Abra o prompt **x86 Native Tools** e confirme `where cl`. |
| Conflito de arquitetura no vinculador | Confira se o prompt selecionado é x86 e abra novamente o atalho correto. |
| Arquivo-fonte inacessível | Confira a pasta atual, a extensão `.macs` e as aspas quando o caminho contém espaços. |
| Erro léxico, sintático ou semântico | Leia linha e coluna. Em caso de pontuação ausente, o parser pode apontar o início do comando seguinte, onde percebeu a falta. |
| Entrada inválida ao executar o programa | Use o tipo esperado. Em `float`, digite `7.5` com ponto. Para `bool`, use `true`, `false`, `1` ou `0`. |
| Saída em uso | Feche o programa gerado antes de recompilar com o mesmo nome. |

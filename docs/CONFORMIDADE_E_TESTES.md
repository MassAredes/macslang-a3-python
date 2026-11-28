# Conformidade com o enunciado e testes executados

Validação realizada em **28/11/2026**, no Windows, com **Visual Studio Build Tools 2019**, compilador MSVC x86 da série 19.29 e Windows SDK 10.0.19041. O projeto final está em `macslang-a3-c`. Os PDFs do professor permanecem no diretório do trabalho.

## Checklist

As páginas se referem ao PDF `TrabalhoA3_TeoriaComputacaoCompiladores_20261105162423.pdf`. A data indicada no enunciado para entrega e apresentação é **25/11/2026**.

| Requisito original | Arquivo ou função responsável | Como foi validado | Status |
| --- | --- | --- | --- |
| Desenvolver um compilador para a primeira versão da MACSLang, usando linguagem compilada. Pág. 1 | `src/*.c`, `compilar.bat` | O MSVC compilou os fontes C11. O compilador resultante aceitou fontes MACSLang e produziu programas nativos. | Atendido |
| Implementar cada fase em um módulo separado. Pág. 1 | `lexer.c`, `parser.c`, `semantico.c`, `gerador.c` | A separação foi conferida na estrutura e os quatro modos de inspeção foram executados. | Atendido |
| Implementar o analisador léxico. Pág. 1 | `lexer.c`, `lexar` | `--tokens` mostrou classes e posições. O exemplo com `@` foi rejeitado nessa fase. | Atendido |
| Implementar o analisador sintático. Pág. 1 | `parser.c`, `analisar_sintaxe` | `--ast` mostrou função e `for`. A falta de `;` foi rejeitada como erro sintático. | Atendido |
| Implementar o analisador semântico. Pág. 1 | `semantico.c`, `analisar_semantica` | `--check` aceitou o fatorial e rejeitou tipo errado, escopo inválido, aridade incorreta e ausência de retorno, entre outros. | Atendido |
| Implementar o gerador de código. O enunciado dispensa fases próprias de código intermediário e otimização. Pág. 1 | `gerador.c`, `gerar_c` e `windows.c`, `gerar_executavel` | `--emit-c` gravou C11. O `cl.exe` compilou esse C com `/Od` e gerou um executável que rodou. O C11 é a saída da etapa de geração. | Atendido |
| Arquitetura de destino x86 e sistema Windows. Pág. 2 | `windows.c`, `chamar_cl` e `verificar_x86` | O compilador conferiu o cabeçalho PE. O `dumpbin` mostrou `14C machine (x86)` e `32 bit word machine`. | Atendido |
| Linguagem imperativa, estruturada e com tipagem estática. Pág. 2 | `parser.c`, `semantico.c`, `gerador.c` | Os exemplos executaram instruções em sequência, alteraram variáveis, entraram em blocos condicionais e repetiram laços. Combinações de tipos inválidas foram rejeitadas antes de gerar C. | Atendido |
| Declaração de variáveis com inicializador opcional e suporte a `int`, `float`, `char`, `bool` e `string`. Pág. 3 | `parser.c`, `declaracao`, `semantico.c` e `gerador.c` | Fatorial, média e integração declaram e usam os cinco tipos, com e sem valor inicial. A atribuição de texto a inteiro foi rejeitada. | Atendido |
| Saída `print(expressão)` e entrada `input(variável)`. Pág. 3 | `parser.c`, `gerador.c`, `runtime/macs_runtime.c` | Entradas dos cinco tipos foram lidas em programas nativos. Saídas e erro de conversão de inteiro foram conferidos. | Atendido |
| Condicional `if/else`. Pág. 3 | `parser.c`, `semantico.c`, `gerador.c` | A calculadora rodou com `7` e `-3` e executou os dois ramos. Uma condição inteira foi rejeitada. | Atendido |
| Laços `while` e `for`. Pág. 3 | `parser.c`, `laco_para` e `gerador.c` | A média leu três notas com `while`. Fatorial e integração usaram `for`. Uma referência à variável do laço após seu escopo foi rejeitada. | Atendido |
| Funções com parâmetros e retorno. Pág. 3 | `parser.c`, `funcao`, `semantico.c`, `verificar_chamada` e `verificar_cmd` | Dobro, fatorial e Fibonacci executaram. Chamadas antes da definição foram testadas. Aridade incorreta e função sem retorno garantido foram rejeitadas. | Atendido |
| Programa de referência com fatorial. Pág. 4 | `exemplos/fatorial.macs` e programa correspondente do ZIP de referência | A versão local produziu `120` para `5`. O exemplo de fatorial do ZIP também foi compilado e executado com `5`. | Atendido |
| Apresentar em sala, explicar o código e demonstrar programas de teste. Pág. 1 | `exemplos/`, `testes/`, `docs/COMO_FUNCIONA.md` e `docs/EXPLICACAO_DO_CODIGO.md` | Os materiais e a demonstração técnica estão prontos. A apresentação e a avaliação dependem do grupo e do professor. | Não validado |
| Enviar o código pelo canal indicado no enunciado. Pág. 1 | Pasta `macslang-a3-c` | O código e a documentação foram preparados. O envio pertence ao grupo. | Não validado |

## Resultados da versão final

| Validação executada | Evidência observada |
| --- | --- |
| Construção do compilador normal | `compilar.bat` terminou com sucesso sob `/W4 /WX` e produziu `build\macsc.exe`. |
| Geração e execução de Windows x86 | `dumpbin` identificou `14C machine (x86)`. Com entrada `5`, o console mostrou `O fatorial de 5 é 120`. |
| Compilação e execução sem Python no `PATH` | `where python.exe` falhou no ambiente reduzido. O mesmo ambiente compilou o compilador, gerou e rodou o fatorial. `dumpbin /dependents` mostrou `KERNEL32.dll` como dependência direta de cada binário normal conferido. |
| Saída personalizada | `-o "build\fatorial"` acrescentou `.exe`. A suíte gerou e rodou um programa em caminho com espaços. |
| Suíte normal | **13 grupos aprovados** com `testes\testar.ps1`. |
| Suíte com verificação de memória | **13 grupos aprovados** com `compilar.bat memoria` e `testes\testar.ps1 -Memoria`. A suíte exigiu a mensagem de verificação tanto do compilador quanto dos programas gerados nos casos aplicáveis, inclusive nas saídas com erro. |
| Compatibilidade com o ZIP de referência | Cinco fontes válidos foram compilados e rodaram. Oito fontes inválidos foram rejeitados com fase e posição. |
| Preservação da saída anterior | Com o executável de teste aberto sem permissão de substituição, a nova publicação falhou. O `.c` e o `.exe` da compilação anterior foram comparados byte a byte e permaneceram iguais. |

### O que a suíte cobre

- Listagem de tokens, árvore, análise semântica e geração de C independente.
- Fatorial para `0 → 1`, `1 → 1`, `5 → 120` e `20 → 2432902008176640000`. Foram conferidos também `-1` fora da faixa e `abc` como entrada inválida.
- Calculadora com `7 → 14` e `-3 → -6`, exercitando `if/else`.
- Média com `Ana Maria` e notas `6`, `7` e `8`. Foram conferidos média `7`, situação `A` e `true`.
- Integração com Fibonacci, leitura de tipos diferentes, nomes em escopos distintos, comparação textual, precedência, operadores lógicos e `for` com início omitido.
- Três diagnósticos de exemplo e oito rejeições semânticas adicionais.
- Preservação dos arquivos anteriores quando o executável está em uso.

Na verificação de memória, `_CrtDumpMemoryLeaks` roda após as rotinas de liberação. O registro de ausência de vazamentos se refere às execuções acima. [Heap de depuração do MSVC](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/crtdumpmemoryleaks?view=msvc-170)

### Conferência complementar do material de referência

| Fonte do ZIP | Resultado conferido |
| --- | --- |
| `01_fatorial.macs` | `O fatorial de 5 é 120` |
| `02_fibonacci_recursivo.macs` | `Fibonacci(10) = 55` |
| `03_numero_primo.macs` | `29 é primo` |
| `04_temperaturas.macs` | `Kelvin = 298.15` para entrada `25` |
| `05_tipos_strings_e_blocos.macs` | `Flag ativa: true`, após execução do exemplo de tipos e escopos |
| Oito arquivos de `tests/invalid/` | Código de falha e diagnóstico com posição em todos os casos |

## Resumo da reestruturação

A versão anterior foi arquivada em `backup/macslang-a3-python-original.zip`. Foram comparados os bytes dos **75 arquivos** originais e dos itens correspondentes no ZIP, incluindo o histórico local do Git. Os módulos do compilador foram substituídos por fontes C no novo projeto. O runtime que já era C foi preservado como base e recebeu ajustes de compilação e verificação de memória. A documentação e os testes foram atualizados para a versão C.

A geração de executáveis usa o ambiente MSVC x86 descrito no [README](../README.md). As regras de aritmética e os limites do projeto estão no [relatório](RELATORIO.md). A demonstração e o envio são as etapas acadêmicas registradas no checklist.

# Trabalho A3 · Compilador para MACSLang

**Centro Universitário de Belo Horizonte (UniBH)**  
**Disciplina** Teoria da Computação e Compiladores  
**Professor** Otaviano Martins Monteiro

## Introdução

O trabalho implementa um compilador para a primeira versão da MACSLang, uma linguagem imperativa, estruturada e com tipos definidos antes da execução. Um programa escrito nessa linguagem pode declarar variáveis, receber valores, fazer cálculos, controlar a execução com condicionais e laços e chamar funções.

O compilador lê o fonte, verifica as regras da linguagem e gera um programa que pode rodar no Windows x86. A implementação separa cada uma das quatro fases em seu próprio módulo C.

## Objetivos

O objetivo geral é transformar um programa MACSLang válido em um executável Windows x86 e mostrar diagnósticos localizados quando o fonte viola as regras da linguagem.

Os objetivos específicos são:

- Reconhecer as partes do texto e registrar a posição de cada uma.
- Conferir a ordem exigida pela gramática e representar o resultado em uma árvore.
- Validar tipos, usos de variáveis e chamadas de função.
- Produzir código C11 equivalente e gerar um executável de 32 bits.
- Demonstrar as fases com fontes válidos, inválidos e resultados reproduzíveis.

## Fundamentação teórica

| Conceito | Aplicação |
| --- | --- |
| Análise léxica | Sequências de caracteres formam tokens, como nomes, números, palavras reservadas e símbolos. |
| Análise sintática | A gramática define como os tokens podem formar uma expressão ou instrução. O parser produz uma árvore que registra essa estrutura. |
| Análise semântica | A tabela de símbolos registra declarações e tipos. A análise verifica se cada nome está disponível e se o uso combina com seu tipo. |
| Geração de código | A árvore verificada é percorrida para escrever o C11 correspondente. O compilador nativo e o vinculador produzem o executável de destino. |
| Ambiente de execução | Rotinas incluídas no executável cuidam de entrada, texto e memória usada pelas strings. |

Um exemplo da diferença entre as fases é `var n: int = "cinco";`. Todos os tokens podem ser lidos e a ordem atende à forma de uma declaração. A terceira fase percebe que o texto não pode ser usado para iniciar uma variável inteira. A posição do token do valor permite indicar a coluna do problema.

O material didático de Soares descreve a passagem da árvore gramatical para o código da máquina de destino. Neste projeto, o gerador próprio emite C11 e o MSVC termina a compilação. O alvo do vinculador é configurado com `/MACHINE:X86` e conferido pelo campo `Machine` do PE. [Opção /MACHINE do MSVC](https://learn.microsoft.com/en-us/cpp/build/reference/machine-specify-target-platform?view=msvc-170) · [Formato PE do Windows](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)

## Metodologia

1. O enunciado foi lido antes da reestruturação. Foram levantados os recursos obrigatórios, os módulos, a plataforma e a demonstração pedida.
2. A estrutura, os exemplos e o comportamento da versão anterior foram revisados. Uma cópia integral foi arquivada antes de iniciar o novo projeto.
3. O compilador foi reimplementado em C11. Os exemplos MACSLang e o runtime que já era escrito em C serviram de base para a nova versão. A inicialização e os diagnósticos de memória do runtime foram atualizados.
4. Os modos de inspeção foram usados para conferir a saída de cada módulo. Programas válidos foram compilados e executados. Fontes inválidos foram verificados quanto à fase e à posição do erro.
5. O resultado em Windows x86 foi conferido pelo cabeçalho. Os testes normais e o modo de memória foram documentados ao fim.

A linguagem usa `int`, `float`, `char`, `bool` e `string`. A interpretação dos casos que o enunciado apresenta de forma resumida segue a gramática do material de referência, por exemplo, a precedência dos operadores e as partes opcionais de `for`. A menção a `switch` no enunciado faz parte da explicação geral sobre paradigmas. As estruturas descritas para a MACSLang são `if/else`, `while` e `for`.

## Implementação e decisões

| Módulo | Resultado entregue à próxima etapa |
| --- | --- |
| `lexer.c` | Vetor de `Token` com texto, classe, linha, coluna e um marcador de fim. |
| `parser.c` | `Programa` com árvores de expressões, comandos e declarações de funções. |
| `semantico.c` | Mesma árvore, agora com tipos, identificadores internos e promoções numéricas válidas registrados. |
| `gerador.c` | Código C11 com as operações e chamadas correspondentes ao fonte. |
| `windows.c` | Arquivos C e executável PE após a compilação nativa e a conferência de arquitetura. |

O parser usa descida recursiva. A tradução de expressões mantém os parênteses necessários para preservar a ordem representada na árvore. A semântica armazena símbolos em listas por escopo. A busca começa pelo bloco atual e segue para os blocos externos. Os nós guardam os números dos símbolos resolvidos, o que permite gerar nomes C únicos como `macs_var_1`.

O `int` da linguagem usa 64 bits, seguindo o material de referência, e `float` é representado por `double`. As condições exigem `bool`. O gerador usa operações numéricas de C, compara strings pelo conteúdo e chama funções auxiliares quando precisa ler ou concatenar valores.

O PDF dispensa a implementação de fases de código intermediário e otimização. O C11 aparece como a saída do módulo obrigatório de geração de código, usada para chegar ao `.exe`. O projeto segue a estrutura das quatro fases solicitadas. A compilação nativa é feita com a otimização desativada.

## Execução, diagnósticos e limites

O [README](../README.md) descreve a instalação, a compilação no prompt x86, os modos do executável e os exemplos de entrada e saída. O [guia do código](EXPLICACAO_DO_CODIGO.md) explica as funções e estruturas usadas em cada etapa.

Um diagnóstico do compilador aponta a fase e a posição no fonte quando o problema pertence à linguagem. Erros de leitura, de arquivo de saída ou do `cl.exe` são reportados pela plataforma. Se uma leitura falhar durante a execução, o runtime explica o tipo de valor esperado.

As operações inteiras e a divisão seguem os limites do C gerado. Use valores dentro da faixa de 64 bits e divisores diferentes de zero. As strings criadas em execução são guardadas até o programa terminar. O limite de aninhamento protege a pilha na análise de expressões e blocos. Caminhos normais do Windows, incluindo espaços, foram usados na validação. O MSVC instalado e o ambiente x86 são requisitos para gerar binários.

Os resultados e o checklist estão em [Conformidade e testes](CONFORMIDADE_E_TESTES.md).

## Conclusão

O projeto mostra como o mesmo fonte passa de caracteres a tokens, de tokens a uma árvore verificada e da árvore ao programa executável. A separação em módulos permite acompanhar o que cada fase recebe e localizar a causa de falhas de estrutura e tipos. A validação com programas nativos permite conferir tanto a saída no console quanto a arquitetura que o enunciado pede.

## Referências

- MONTEIRO, Otaviano Martins. *Trabalho A3 de Teoria da Computação e Compiladores*. UniBH. Enunciado disponível no arquivo `TrabalhoA3_TeoriaComputacaoCompiladores_20261105162423.pdf`, páginas 1 a 4.
- SOARES, Marco Antônio Calijorne. *Teoria da Computação e Compiladores*. Material didático disponível no arquivo `DesenvolvimentodeCompiladoreseGeracaodeCodigoTCC_20261112212500.pdf`, páginas 2 a 3 e 9 a 18.
- SOARES, Marco Antônio Calijorne. *MACSLang Compiler 2.0*. Gramática e referência da linguagem consultadas no ZIP local `macsLang-copiler-multiplatform-main.zip`, nos arquivos `docs/GRAMATICA.md` e `docs/REFERENCIA_LINGUAGEM.md`.
- MICROSOFT. *Usar as ferramentas de build do Microsoft C++ na linha de comando*. [Microsoft Learn](https://learn.microsoft.com/pt-br/cpp/build/building-on-the-command-line?view=msvc-170)
- MICROSOFT. */MACHINE (Specify Target Platform)*. [Microsoft Learn](https://learn.microsoft.com/en-us/cpp/build/reference/machine-specify-target-platform?view=msvc-170)
- MICROSOFT. *PE Format*. [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- MICROSOFT. *_CrtDumpMemoryLeaks*. [Microsoft Learn](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/crtdumpmemoryleaks?view=msvc-170)

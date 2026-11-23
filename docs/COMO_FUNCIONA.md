# Como o compilador funciona

Comece com este programa de uma linha. Salve-o como `conta.macs` em UTF-8, na pasta do projeto.

```text
print(2 + 3 * 4);
```

O esperado é `14`. A multiplicação acontece primeiro, porque `*` tem precedência maior que `+`.

## 1. Léxico | `lexer.py`

Rode `python main.py conta.macs --tokens`. O lexer anda pelo arquivo com um índice. Ele transforma cada pedaço reconhecido em um `Token`, que guarda a categoria, o texto, a linha e a coluna. Em `conta.macs`, os tokens correspondem a `print`, `(`, `2`, `+`, `3`, `*`, `4`, `)`, `;` e fim do arquivo.

O lexer ignora espaços e comentários. Ele procura operadores de dois caracteres antes dos de um caractere, para reconhecer `<=` inteiro. Se encontrar `@` fora de um literal ou comentário, informa a posição e encerra essa fase com erro.

## 2. Sintaxe | `parser.py`

Rode `python main.py conta.macs --ast`. O parser anda pelos tokens com outro índice. `aceitar` consome um token se ele tiver a categoria esperada. `exigir` sinaliza erro quando o token esperado está ausente.

O código usa uma lista de prioridades para construir expressões. Para essa conta, ele monta uma árvore com a soma acima da multiplicação.

```text
Saida
└─ Binario +
   ├─ Literal 2
   └─ Binario *
      ├─ Literal 3
      └─ Literal 4
```

A árvore se chama **AST**, sigla em inglês para árvore sintática abstrata. Os nós de `nos.py` guardam a estrutura, como o valor de um `print` ou o corpo de um `while`. O parser já verificou o ponto e vírgula e os parênteses, então esses símbolos podem ficar fora da árvore.

## 3. Semântica | `semantico.py`

Rode `python main.py conta.macs --check`. O analisador percorre a árvore. Em `3 * 4`, encontra dois `int` e conclui que o resultado também é `int`. O mesmo acontece com a soma. Se você trocar `4` por `true`, a estrutura ainda é reconhecida pelo parser, mas a semântica rejeita a multiplicação entre `int` e `bool`.

Para resolver nomes, há um dicionário para cada escopo. Ao entrar em um bloco, o analisador acrescenta um dicionário no fim da lista. Ao sair, remove esse dicionário. Para procurar um nome, anda do último para o primeiro. Por isso um bloco interno pode ter uma variável com o mesmo nome de uma variável externa.

Antes de verificar os corpos das funções, o analisador registra todos os nomes e assinaturas. Assim uma função pode chamar outra que aparece mais abaixo no arquivo, ou pode chamar a si mesma. Um `return` direto garante retorno. Em um `if/else`, os dois ramos precisam retornar para o comando garantir retorno. Em laços, o compilador usa a regra conservadora de que o corpo pode deixar de executar.

## 4. Código C e executável | `gerador_c.py`, `runtime/` e `main.py`

Rode `python main.py conta.macs --emit-c build\conta.c`. O gerador percorre a árvore já verificada. A declaração de uma variável vira uma declaração C. Um `for` vira um `for` C. A conta vira uma expressão com parênteses que preservam a ordem da árvore. Como o resultado é `int`, o gerador usa `printf` com o formato correto para um inteiro de 64 bits.

As variáveis recebem nomes como `macs_var_1` e as funções recebem nomes como `macs_funcao_1`. A semântica escolhe esses nomes ao resolver os símbolos. Assim, um nome válido em MACSLang como `main` ou `printf` pode ser usado sem entrar em conflito com o C gerado.

O `runtime` fornece as funções que o C gerado chama para ler números, texto, caracteres e booleanos, além de concatenar strings. Ele cuida da memória dessas strings até o programa terminar. As funções auxiliares são compiladas junto com o C gerado.

No terminal x86, rode `python main.py conta.macs` e depois `build\conta.exe`. O `main.py` passa `/MACHINE:X86` ao linker e lê o campo `Machine` do executável recém-criado. O valor `0x014C` comprova o destino Windows x86. O tamanho de um `int` da MACSLang continua sendo 64 bits, pois é uma regra da linguagem.

## Regras para escrever novos exemplos

| Tema | Regra |
| --- | --- |
| Declaração | `var nome: tipo;` ou `var nome: tipo = expressão;`. Sem inicializador, os valores são `0`, `0.0`, caractere nulo, `false` e texto vazio, respectivamente. O inicializador só consulta variáveis já disponíveis. |
| Tipos | `int` tem 64 bits. `float` vira `double` no C. `char` é um caractere ASCII. `bool` guarda `true` ou `false`. `string` é texto UTF-8. |
| Atribuição | Os tipos devem coincidir. Um `int` pode entrar em um `float`, inclusive como argumento ou retorno. |
| Operadores | `+`, `-`, `*`, `/` trabalham com números. `%` exige dois `int`. Dividir dois inteiros dá um inteiro. `-` também pode negar um número. |
| Comparações | `<`, `<=`, `>` e `>=` comparam números. `==` e `!=` comparam tipos iguais ou dois números. A igualdade entre strings é pelo conteúdo. |
| Lógica | `&&`, `\|\|` e `!` trabalham com `bool`. As condições de `if`, `while` e `for` também exigem `bool`. |
| Texto | Somar uma `string` a qualquer tipo junta os valores como texto. `print` acrescenta uma nova linha. `input` lê uma linha por variável e preserva os espaços em strings. |
| Blocos e `for` | Use chaves nos corpos de condicionais e laços. Os três trechos do cabeçalho do `for` podem ser omitidos. Uma variável declarada nesse cabeçalho deixa de existir após o laço. |
| Funções | Use `func nome(parâmetro: tipo): tipo { ... }`. Passe os valores por parâmetros e retorne um valor com `return`. As variáveis do programa principal pertencem ao `main` gerado. |
| Comentários | Use `//` para uma linha e `/* ... */` para um bloco de comentário. Em strings e chars, são aceitos os escapes `\n`, `\r`, `\t`, `\\`, `\"` e `\'`. |

## Limites para os testes

- Use apenas valores inteiros de 64 bits. As operações no executável seguem as regras do C em caso de estouro ou divisão por zero, portanto use dados que respeitem essa faixa e divisores diferentes de zero. Para formar o menor inteiro, escreva `-9223372036854775807 - 1`, pois o literal positivo tem como limite `9223372036854775807`.
- Chamadas com efeitos colaterais, como uma função que imprime, ficam mais previsíveis em comandos separados. A ordem de avaliação de argumentos e operandos segue a do C gerado.
- A lista de strings criadas durante a execução é liberada quando o executável termina. O uso de memória cresce se o programa criar novas strings em um laço longo.

## Roteiro curto para a apresentação

1. Mostre o `conta.macs` e rode `--tokens` e `--ast` para explicar de onde sai a ordem das operações.
2. Rode `--check` com `exemplos\invalidos\tipo.macs` e explique por que a declaração passou pela sintaxe e foi rejeitada pela semântica.
3. Compile o `fatorial.macs`, execute com `5` e abra o `.c` produzido. Localize o `for` e a rotina de entrada.
4. Use `dumpbin` para mostrar o `14C machine (x86)`. Indique onde o Python chama o MSVC e onde confere o cabeçalho.

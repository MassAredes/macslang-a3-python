# Entenda o compilador, arquivo por arquivo

Imagine que alguém escreve `print(5);` e salva a linha num arquivo `.macs`. O computador precisa transformar esse texto num programa que o Windows consiga executar. O projeto faz isso em duas partes. Primeiro, o código em Python lê o texto, confere as regras da MACSLang e escreve a versão equivalente em C. Depois, o `cl.exe` compila esse C para criar um executável Windows x86.

O `.macs` é o **código-fonte**, ou seja, o que a pessoa escreveu. O `.c` é a tradução que o compilador produz. O `.exe` é o programa que você abre para ver o resultado. Por exemplo, ao compilar `print(5);`, você cria um executável. É quando o executável roda que o `5` aparece na tela.

Se este for seu primeiro contato com Python, comece pelo exemplo abaixo. Depois, abra o arquivo indicado em cada seção e use `Ctrl+F` para encontrar o trecho pelo nome.

- [Um programa para acompanhar](#um-programa-para-acompanhar)
- [Entrada do compilador](#entrada-do-compilador)
- [Dados compartilhados](#dados-compartilhados)
- [Quatro fases](#quatro-fases)
- [Runtime em C](#runtime-em-c)
- [Programas de exemplo](#programas-de-exemplo)
- [Testes automáticos](#testes-automáticos)
- [Arquivos de apoio](#arquivos-de-apoio)

## Um programa para acompanhar

Salve estas duas linhas como `primeiro.macs` na pasta do projeto. A primeira cria a variável `n`, que guarda o inteiro `5`. A segunda pede para mostrar a soma de `n` com `2 * 3`. Como a multiplicação vem primeiro, o programa deve mostrar `11`.

```text
var n: int = 5;
print(n + 2 * 3);
```

Na pasta do projeto, experimente os comandos abaixo, um por vez. Eles usam o mesmo arquivo e param em pontos diferentes do trabalho.

```bat
python main.py primeiro.macs --tokens
python main.py primeiro.macs --ast
python main.py primeiro.macs --check
python main.py primeiro.macs --emit-c build\primeiro.c
```

| Comando | O que você verá e por quê |
| --- | --- |
| `--tokens` | Uma lista com o texto, a categoria e a posição de cada parte. Por exemplo, `n` aparece como `IDENT`, a categoria usada para nomes. |
| `--ast` | A declaração e o `print` ficam em uma árvore. O ramo da multiplicação aparece dentro da soma, registrando a ordem da conta. |
| `--check` | Aparece `OK` depois que o compilador confirma a estrutura e os tipos. Entre outras coisas, ele verifica que `n` foi declarada e contém um inteiro. |
| `--emit-c` | É criado `build\primeiro.c`. Você pode abri-lo para ver a variável e a expressão já traduzidas. |

Para ver o `11` na prática, abra o **x86 Native Tools Command Prompt**, entre na pasta do projeto e execute os comandos abaixo. O primeiro cria o binário. O segundo roda o programa que você escreveu.

```bat
python main.py primeiro.macs
build\primeiro.exe
```

## Entrada do compilador

### [main.py](../main.py)

A função desse arquivo é receber o comando do terminal e organizar o trabalho. Quando você digita `python main.py primeiro.macs --tokens`, é nele que o Python descobre qual arquivo abrir e até onde seguir.

#### O que acontece ao abrir o arquivo

Os `import` dão acesso a funções que já existem. `argparse` lê opções como `--tokens`, `Path` trabalha com caminhos de arquivos e `sys` permite indicar ao terminal se houve sucesso ou erro. `subprocess` roda outro programa, `TemporaryDirectory` cria uma pasta temporária e `struct` interpreta os números gravados no cabeçalho do executável. Os imports locais trazem os quatro módulos do compilador, a função que mostra a árvore e a classe usada nos diagnósticos.

`RUNTIME` guarda o endereço da pasta `runtime` a partir da posição do próprio `main.py`. Esse endereço é calculado dessa forma para o compilador localizar os arquivos em C mesmo quando o comando for iniciado de outra pasta.

O bloco `if __name__ == "__main__"` verifica se o arquivo foi aberto como programa. Nesse caso, executa `sys.exit(main())`. Quando um teste importa `verificar_x86`, por exemplo, o Python disponibiliza a função para o teste. A chamada que inicia o compilador acontece na execução direta de `main.py`.

#### Como o comando vira uma sequência de etapas

`ler_argumentos()` recebe os argumentos que vieram depois de `main.py`. O caminho do `.macs` é obrigatório. As opções `--tokens`, `--ast`, `--check`, `--emit-c` e `-o` fazem parte de um grupo que permite escolher uma por execução. O retorno é um objeto com essas escolhas, que `main()` passa para `compilar(opcoes)`.

Dentro de `compilar`, siga a execução de cima para baixo.

1. **Ler o texto.** Confere se o nome termina em `.macs` e abre o fonte com `utf-8-sig`. Essa leitura aceita a marca de início que alguns editores gravam em arquivos UTF-8. A partir daí, `fonte` é uma string Python com as linhas do programa.
2. **Separar as partes do texto.** `Lexer(fonte).analisar()` devolve a lista `tokens`. Quando a opção é `--tokens`, o laço imprime essa lista e o `return` encerra a função. Assim dá para estudar a primeira fase por conta própria.
3. **Montar a estrutura.** `Parser(tokens).analisar()` devolve `programa`, que contém a árvore de comandos e funções. Com `--ast`, chama `mostrar_arvore` nesse ponto. O que aparece na tela é a estrutura criada pelo parser.
4. **Conferir as regras.** `Semantico().analisar(programa)` verifica o que está na árvore e acrescenta os tipos descobertos e os nomes que serão usados no C. Com `--check`, o compilador mostra `OK` e encerra depois dessas verificações.
5. **Traduzir e decidir onde gravar.** `GeradorC().gerar(programa)` devolve o C em texto. `--emit-c` grava esse texto no caminho escolhido, que deve terminar em `.c`. Quando é para criar o executável, `-o` escolhe o nome do `.exe`. Sem `-o`, o nome vem do fonte e a pasta é `build`. Por exemplo, `primeiro.macs` produz `build\primeiro.exe` e `build\primeiro.c`.

`main()` envolve a chamada de `compilar` em um `try/except`. Se uma das fases detectar um erro programado, como uma variável ausente, o aviso vai para `stderr`, o canal de mensagens de erro do terminal. O tratamento também mostra mensagens para falhas de leitura, codificação, aninhamento e ferramentas. Os erros tratados por `main` devolvem `1`. Chegar ao fim com sucesso devolve `0`. Os testes usam esses números para conferir o resultado.

#### Como o texto C vira um executável

`conferir_destino_c(destino)` compara o caminho escolhido com o arquivo original `macs_runtime.c`. Essa checagem preserva as funções auxiliares que a próxima compilação também vai precisar. No modo `--emit-c`, `salvar_c(destino, codigo)` usa essa checagem, cria a pasta se necessário e grava o texto em UTF-8.

`gerar_executavel(codigo, executavel)` recebe o mesmo texto C e o endereço de saída do executável. Ele grava `gerado.c` em uma pasta temporária e define um caminho para `programa.exe` ali dentro. O `cl.exe` também produz arquivos intermediários `.obj`. Usar essa pasta faz com que eles sejam removidos ao final do bloco.

Na lista `comando`, o primeiro item é `cl.exe`. Os dois arquivos que ele compila são `gerado.c` e `macs_runtime.c`. O primeiro contém a tradução do `.macs`. O segundo fornece as funções chamadas por essa tradução. Se quiser entender a lista de opções, procure estes trechos no código.

| Trecho | Efeito |
| --- | --- |
| `/nologo` | Encurta a saída de abertura do MSVC. |
| `/TC` e `/std:c11` | Mandam compilar como C usando o padrão C11. |
| `/utf-8` | Lê os arquivos em UTF-8 e usa essa codificação nas strings do programa. |
| `/Od` | Desativa otimizações, deixando a compilação adequada para estudo. |
| `/I` | Indica onde encontrar o cabeçalho `macs_runtime.h`. |
| `/Fe` | Escolhe o caminho do executável temporário. |
| `/link /MACHINE:X86` | Passa ao vinculador o destino x86, para unir os arquivos compilados num executável de 32 bits. |

`subprocess.run(comando, cwd=pasta, check=True)` inicia o `cl.exe` na pasta temporária e espera ele terminar. A opção `check=True` transforma um código de falha em exceção. O tratamento distingue o caso de `cl.exe` ausente do caso em que ele rodou e reportou um problema. Por isso, abrir o terminal **x86 Native Tools** faz diferença. Esse terminal disponibiliza a ferramenta e as bibliotecas para a arquitetura pedida.

Antes de publicar a saída, `verificar_x86(executavel)` abre o binário como bytes. Ele confere `MZ` no começo e lê os bytes 60 a 63 para descobrir a posição do cabeçalho PE, que é uma parte do formato de executável Windows. Nessa posição, procura a marca `PE` e o campo `Machine` com o valor `0x014C`. Os bytes desse valor aparecem na ordem `4C 01` dentro do arquivo. Se a conferência passar, `replace` move o `.exe` e o `.c` temporários para os caminhos finais.

## Dados compartilhados

### [erros.py](../erros.py)

Este arquivo define dois objetos que as fases usam em conjunto. Um carrega a informação sobre o que foi lido. O outro permite apontar com precisão onde houve um problema.

**`Token`.** Ao ler o `n` de `var n: int = 5;`, o lexer guarda `IDENT` em `tipo`, `n` em `texto`, `1` em `linha` e `5` em `coluna`. Essa posição continua disponível nas fases seguintes. Em textos e caracteres, há também o campo `valor`. Para `"oi\n"`, `texto` é o pedaço escrito com aspas e barra. `valor` guarda `oi` seguido de uma quebra de linha real. O `@dataclass` pede ao Python que crie a forma de inicializar esses campos quando alguém chama `Token(...)`.

**`ErroCompilacao.__init__()`.** Recebe o nome da fase, um token e um texto que descreve o problema. O construtor pega a linha e a coluna do token e monta a mensagem. Depois chama `Exception` com essa mensagem, para que `main.py` possa capturar e exibir o erro. O formato fica igual nas três fases e cada uma indica a causa que descobriu.

### [nos.py](../nos.py)

Este arquivo descreve os dados guardados na **árvore sintática abstrata**, ou **AST**. Ela representa quais partes estão dentro de quais. Em `n + 2 * 3`, o ramo `2 * 3` fica inteiro de um lado da soma. Essa organização permite conferir os tipos e depois gerar o C na ordem certa.

Cada `@dataclass` define os campos de um tipo de nó, que é uma parte dessa árvore. `from __future__ import annotations` permite citar nas anotações um tipo que aparece mais adiante no arquivo. O trecho `field(default="", init=False)` cria campos inicialmente vazios, como o tipo de uma expressão ou seu nome em C. A semântica pode preencher esses campos depois de verificar o programa.

| Classes | O que guardam e como são usadas |
| --- | --- |
| `No`, `Expressao`, `Comando` | `No` fornece o token de origem para saber a posição no fonte. `Expressao` acrescenta o tipo do valor a ser produzido. `Comando` reúne as instruções que a árvore manda executar. |
| `Literal`, `Variavel`, `Chamada` | Um `Literal` guarda algo escrito diretamente, como `5`. Uma `Variavel` guarda a referência a um nome, como o `n` lido dentro do `print`. Uma `Chamada` guarda a função e os argumentos, como `dobro(7)`. Variáveis e chamadas ganham um `nome_c` quando a semântica descobre a qual declaração correspondem. |
| `Unario`, `Binario` | Guardam o token do operador. `Unario` tem um operando, como o `7` em `-7`. `Binario` tem dois lados, como o `2` e o `3` em `2 * 3`. Cada lado pode conter outra expressão. |
| `Bloco`, `Declaracao`, `Atribuicao` | Um bloco guarda os comandos entre chaves. A declaração guarda o nome, o tipo prometido e o valor inicial quando foi escrito. A atribuição aponta para uma variável e guarda a expressão que vai substituir seu valor. |
| `Entrada`, `Saida`, `Retorno`, `ExecutarChamada` | `Entrada` guarda a variável que vai receber o dado de `input`. `Saida` guarda o que `print` vai mostrar. `Retorno` aponta para a expressão que será entregue a quem chamou a função. `ExecutarChamada` registra uma chamada usada como instrução por si só, como `dobro(5);`. |
| `Condicional`, `Enquanto`, `Para` | `Condicional` guarda o teste, o corpo do `if` e um possível `else`. `Enquanto` guarda o teste e o corpo do `while`. `Para` registra o que executar no início, a condição, o que atualizar a cada volta e o corpo. |
| `Parametro`, `Funcao`, `Programa` | `Parametro` guarda o nome e o tipo de uma entrada da função. `Funcao` agrupa parâmetros, retorno e corpo. `Programa` reúne as funções e os comandos escritos fora delas. Essa divisão ajuda o gerador a colocar os comandos de nível superior dentro do `main` em C. |

**`mostrar_arvore(no, nivel=0)`.** É a função chamada por `--ast`. Ela imprime a classe do nó, o trecho de origem e a linha quando estão disponíveis. Usa `fields(no)` para percorrer os campos. Se o campo contém outro nó ou uma lista de nós, chama a si mesma com recuo maior. Por isso você vê `Binario '*'` mais à direita do que `Binario '+'` na saída. Campos vazios são pulados para facilitar a leitura.

## Quatro fases

### [lexer.py](../lexer.py) | Fase 1 · Análise léxica

A função deste arquivo é separar o texto em partes reconhecíveis, chamadas **tokens**. Pense na primeira linha do exemplo. A lista que chega ao parser tem esta forma, com as posições omitidas para caber em uma linha.

```text
var  IDENT(n)  :  int  =  INTEIRO(5)  ;
```

Essa separação permite ao parser pedir «agora quero um nome» ou «agora quero um ponto e vírgula», usando categorias que o lexer já encontrou.

#### As regras que o lexer consulta

`TIPOS` contém os cinco tipos da linguagem. `RESERVADAS` inclui esses tipos e nomes que têm significado próprio, como `var`, `print` e `true`. Assim, `n` recebe a categoria `IDENT`, enquanto `var` mantém a sua própria categoria. `OPERADORES_DUPLOS` contém combinações como `<=` e `!=`. `SIMBOLOS` contém caracteres usados sozinhos, como `+` e `(`. `ESCAPES` mapeia a letra `n` depois de uma barra para a quebra de linha que ela representa.

`digito(caractere)` confere se o caractere está de `0` a `9`. `letra(caractere)` aceita letras ASCII e sublinhado. O código usa essas duas funções para permitir `nota2` como nome e reconhecer `2` sozinho como número.

#### Como o código anda pelo texto

Em `Lexer.__init__()`, a posição começa no índice `0` do texto, que corresponde à linha `1` e à coluna `1`. `olhar()` consulta o caractere dessa posição. `olhar(1)` consulta o próximo sem avançar. O recorte de string devolve texto vazio no fim do arquivo, o que vira um sinal para parar os laços.

`avancar()` devolve o caractere atual e move o índice. Para uma quebra de linha, aumenta a linha e volta a coluna para `1`. Para os outros caracteres, aumenta apenas a coluna. É assim que, mesmo pulando espaços e comentários, a lista mantém a posição de cada parte.

`analisar()` repete essas ações até o fim. Se vê espaço, só avança. Se vê `//`, ignora o restante daquela linha. Se vê `/*`, chama `pular_comentario()`, que procura o fechamento `*/` e sinaliza erro se ele faltar. Nos outros casos, guarda a posição inicial e chama `ler_token()`. No final, acrescenta um token `EOF`. O parser usa esse token para saber quando terminar.

#### Como `ler_token()` decide o que encontrou

- Se começa com letra ou sublinhado, lê o restante do nome, incluindo dígitos. Busca o resultado em `RESERVADAS` para decidir se é uma palavra da linguagem ou um `IDENT`.
- Se começa com dígito, lê os dígitos consecutivos. Quando há ponto seguido de dígito, continua a leitura e marca `REAL`. Dessa forma, `5` vira `INTEIRO` e `5.2` vira `REAL`. O sinal de `-5` é um token separado e a fase seguinte monta a negação.
- Se começa com aspas, escolhe `TEXTO` ou `CARACTERE` e chama `ler_literal()`. Se começa com um símbolo, tenta dois caracteres juntos antes de testar o caractere sozinho. Isso garante que `<=` tenha um único token.
- Para um símbolo como `@`, cria um erro léxico na posição guardada. Depois de qualquer leitura bem-sucedida, guarda em `texto` o trecho exato do fonte e devolve o token preenchido.

`ler_literal()` consome a aspa inicial e junta os caracteres até a aspa de fechamento. Ao encontrar uma barra, consulta `ESCAPES` e acrescenta o caractere interpretado. Se o literal for `char`, aceita caracteres ASCII imprimíveis e os escapes previstos, e exige que o resultado tenha um único caractere. Byte nulo, escape desconhecido e aspas abertas sem fechamento causam erros nessa função.

### [parser.py](../parser.py) | Fase 2 · Análise sintática

A função deste arquivo é verificar se os tokens estão em uma ordem que a MACSLang permite e montar a árvore. Por exemplo, para declarar `n`, ele espera `var`, um nome, `:`, um tipo e uma possível inicialização. Ao encontrar essa sequência, cria um nó `Declaracao` com os dados necessários para as próximas fases.

#### Como o parser lê os tokens

`Parser.__init__()` guarda a lista recebida do lexer e começa no índice `0`. `atual()` consulta o token desse índice. `avancar()` devolve o token atual e passa para o próximo. Neste arquivo, **consumir** um token quer dizer usar o token atual e avançar a posição.

`aceitar(tipo)` consome o token se sua categoria for a pedida. Se a categoria for outra, devolve `None` e deixa a posição onde estava. É útil para um trecho opcional, como `else`. Já `exigir(tipo)` chama `aceitar` e, se o resultado for `None`, chama `erro()`. O erro mostra o que era esperado, o que foi encontrado e de qual posição veio. Quando falta `;` depois de uma declaração, é comum a mensagem apontar para o primeiro token do comando seguinte, pois foi ali que o parser descobriu a falta.

#### Como ele reconhece funções e instruções

`analisar()` percorre a lista até `EOF`. Se o início de um trecho é `func`, consome essa palavra e chama `funcao()`. Caso contrário, chama `comando()`. O resultado é um `Programa` com uma lista de funções e uma lista de comandos.

`funcao()` lê o nome, os parênteses, os parâmetros no formato `nome: tipo` e o tipo de retorno. Quando encontra um tipo, chama `ler_tipo()`, que confere a categoria na lista `TIPOS`. Por fim, `funcao` usa `bloco()` para ler o corpo entre chaves. `bloco` agrupa os comandos e exige a chave de fechamento. Se o arquivo acabar antes, a mensagem aponta que falta `}`.

`comando()` consulta o primeiro token para escolher o caminho.

- `{` inicia um bloco. `if` inicia uma condição entre parênteses e um corpo entre chaves, seguidos de um `else` opcional. `while` tem condição e corpo. `for` encaminha a leitura para `laco_para()`.
- `var` chama `declaracao()`. Essa função lê nome, `:`, tipo e uma expressão de inicialização se veio `=`. `print` lê uma expressão entre parênteses. `input` lê o nome da variável entre parênteses. `return` lê o valor a devolver.
- Um `IDENT` no começo do comando pode ser o nome de uma chamada ou o destino de uma atribuição. Se o próximo token é `(`, lê a chamada. Se for outro, chama `atribuicao(nome)`, que exige `=` e lê a expressão. Esses comandos, junto com declaração, saída, entrada e retorno, chegam a `exigir(";")` antes de voltar.

`laco_para()` lê os três trechos dentro dos parênteses do `for`. O primeiro aceita declaração, atribuição ou vazio. O segundo aceita uma expressão ou vazio. O terceiro aceita atribuição ou vazio. Os dois `;` são consumidos pelo próprio `laco_para`, então ele consegue reutilizar `declaracao` e `atribuicao` sem que essas funções fechem o comando cedo demais. Depois lê o corpo com `bloco`.

#### Como ele decide a ordem da conta

`PRECEDENCIA` agrupa operadores em linhas, da prioridade menor para a maior. `expressao(nivel=0)` começa no primeiro nível. Ela chama a si mesma em níveis mais fortes para ler primeiro o que deve ficar mais perto dos valores. Quando volta ao nível do `+` em `n + 2 * 3`, a parte `2 * 3` já virou um nó `Binario`. Assim, consegue guardá-la como lado direito da soma. Se os operadores tiverem a mesma prioridade, o laço os junta da esquerda para a direita. Por isso `8 - 3 - 2` fica `(8 - 3) - 2`.

`expressao_entre_parenteses()` exige a abertura, lê uma expressão inteira e exige o fechamento. Isso permite que `(n + 2) * 3` tenha outra árvore e outro resultado. No nível mais forte, `unario()` lê `!` ou `-` antes de um operando. `primario()` lê o que pode ocupar o início desse pedaço da expressão, como literal, variável, chamada ou outro grupo entre parênteses. Quando há uma chamada, `chamada()` lê zero ou mais argumentos separados por vírgulas. O resultado é um nó com a lista na ordem escrita, para a semântica conferir depois.

### [semantico.py](../semantico.py) | Fase 3 · Análise semântica

A função deste arquivo é conferir se o que está na árvore faz sentido pelas regras da MACSLang. Considere uma declaração com a mesma forma da primeira linha do exemplo, mas com um texto como valor.

```text
var n: int = "cinco";
```

O parser monta a declaração porque ela tem nome, tipo, sinal de atribuição, expressão e ponto e vírgula. Quando `conferir_atribuicao()` recebe `int` como destino e descobre `string` como tipo do valor, a comparação falha. A mensagem aponta para a coluna onde o valor começa.

```text
Erro semântico na linha 1, coluna 14: não é possível passar um valor string para int
```

#### Como o arquivo lembra os nomes

`Simbolo` guarda dois dados de uma variável, o tipo e o nome que ela terá no C. `Semantico.__init__()` prepara um dicionário para as funções, uma lista de dicionários para as variáveis, um contador e o campo `retorno_esperado`. Cada dicionário da lista guarda os nomes disponíveis em um **escopo**, que é uma região do programa. `NUMERICOS` agrupa `int` e `float` para as verificações. `MAIOR_INTEIRO` guarda o limite de um literal inteiro positivo. O import de `math` fornece `isfinite` para verificar valores reais.

`declarar(token, tipo)` usa o último dicionário da lista, que representa o escopo atual. Se o nome já aparece ali, chama `erro()`. Se estiver disponível, incrementa o contador, cria um nome como `macs_var_1` e guarda o símbolo. O nome diferente no C evita conflitos com nomes como `main` e permite distinguir variáveis de mesmo nome em blocos diferentes.

`resolver_variavel(variavel)` faz o caminho da consulta. Procura o nome em ordem inversa, do dicionário mais interno ao mais externo. Quando encontra o símbolo, anota o tipo e o nome C no nó e devolve o tipo. Isso explica o comportamento de duas variáveis chamadas `n` em chaves diferentes. Dentro do bloco menor, a busca encontra a declaração desse bloco primeiro. Quando o bloco termina, o dicionário é removido e o nome de fora volta a ser encontrado. `erro(token, mensagem)` lança `ErroCompilacao` para que qualquer falha use a linha e a coluna do trecho.

#### Em que ordem o programa é conferido

`analisar(programa)` passa pelas funções antes dos comandos do programa principal.

1. Guarda o nome e a assinatura de cada função, ou seja, os tipos dos parâmetros e do retorno. Recusa funções repetidas e cria nomes como `macs_funcao_1`. Registrar todas primeiro permite que uma função chame a si mesma ou outra que aparece mais abaixo.
2. Para cada função, abre um conjunto de escopos, registra os parâmetros com `declarar` e define o `retorno_esperado`. Depois valida o corpo. Se faltar retorno em algum caminho que a análise reconhece, aparece um erro.
3. Cria um escopo próprio para os comandos de fora das funções e limpa o `retorno_esperado`. As variáveis desse trecho serão locais do `main` gerado. Uma função que precise de um desses valores deve recebê-lo como argumento.

#### O que acontece em cada instrução

`verificar_lista(comandos)` passa por todos os comandos, inclusive pelos que estão escritos depois de um `return`. Assim, erros em um trecho posterior ainda aparecem. Ao final, ela informa se algum comando dessa sequência garante retorno. Já `verificar_comando(comando)` escolhe a regra pelo tipo do nó.

| Nó encontrado | Como o código verifica e o que fica registrado |
| --- | --- |
| `Bloco` | Acrescenta um dicionário de variáveis, verifica os comandos entre as chaves e remove o dicionário. Devolve a garantia de retorno obtida na lista. |
| `Declaracao` | Se foi escrito um valor inicial, chama `conferir_atribuicao` antes de cadastrar a nova variável. Assim, o inicializador usa os nomes que já estavam disponíveis. A declaração recebe seu `nome_c`. |
| `Atribuicao` | Resolve o destino, busca o tipo da expressão e verifica a compatibilidade. O nó de destino passa a indicar qual variável do C será alterada. |
| `Entrada`, `Saida`, `ExecutarChamada` | Para `input`, resolve a variável para saber que tipo de dado será lido. Para `print` ou uma chamada usada como comando, verifica a expressão associada. |
| `Retorno` | Confere se há um retorno esperado, o que indica que o comando está dentro de uma função. Verifica a compatibilidade do valor e devolve `True` para indicar que esse caminho retorna. |
| `Condicional` | Chama `conferir_condicao` e verifica os ramos. Um `if/else` só dá garantia de retorno quando os dois ramos retornam. |
| `Enquanto`, `Para` | Verifica a condição e o corpo. O `for` também tem um escopo que cobre seu cabeçalho e seu corpo. A análise atribui `False` à garantia de retorno do laço, pois considera a possibilidade de o corpo executar zero vezes. |

`conferir_atribuicao(destino, valor)` permite passar valores de tipos iguais e também um `int` para um `float`. A regra serve para inicializar, atribuir, passar argumentos e retornar valores. Por exemplo, `var media: float = 7;` é válida porque `7` é um inteiro que pode ser convertido em real. `conferir_condicao(expressao)` pede que a expressão produza `bool`. Por isso, `if (1)` é rejeitado e `if (n > 1)` é aceito quando `n` é um número declarado.

#### Como uma expressão ganha tipo

`verificar_expressao(expressao)` identifica a classe do nó. Literais vão para `verificar_literal()`, variáveis vão para `resolver_variavel` e chamadas vão para `verificar_chamada()`. Operadores de dois lados usam `verificar_binario()`. Os unários são conferidos ali mesmo. `!` exige `bool` e `-` unário exige `int` ou `float`. O resultado é anotado em `expressao.tipo` e devolvido a quem chamou.

`verificar_literal()` recebe a categoria que veio do lexer. Para `INTEIRO`, retira zeros à esquerda, verifica o comprimento e compara o valor com `MAIOR_INTEIRO`. Assim, pode recusar um número enorme antes de tentar convertê-lo por inteiro. Para `REAL`, converte o texto com `float()` e confere se o número é finito. Para `TEXTO` e `CARACTERE`, usa o conteúdo interpretado pelo lexer. Para `true` e `false`, guarda o valor booleano em Python.

`verificar_chamada()` procura o nome da função no dicionário. Compara quantos argumentos a chamada trouxe com quantos parâmetros existem e verifica cada par na ordem escrita. Em `dobro(7)`, por exemplo, o único argumento é um `int` e pode alimentar um parâmetro `int`. A chamada ganha o nome C da função e passa a ter o tipo de retorno declarado.

`verificar_binario()` começa verificando os dois lados. Em `n + 2 * 3`, ela precisa conhecer o tipo de `n` e o tipo de `2 * 3`. A multiplicação recebe dois inteiros e produz `int`, então a soma também produz `int`. Se algum lado de `+` for `string`, o resultado passa a ser texto concatenado. A função também aplica as regras de resto entre inteiros, ordem entre números, igualdade entre tipos iguais ou numéricos e lógica entre booleanos. Se os tipos falharem na regra do operador, a mensagem aponta para a posição desse operador.

### [gerador_c.py](../gerador_c.py) | Fase 4 · Geração de C11

A função deste arquivo é escrever a versão em C do programa. A árvore informa a ordem da conta. Os tipos e nomes anotados pela semântica dizem como escrevê-la. Ao pedir `--emit-c` para `primeiro.macs`, você verá estas linhas dentro do `main` em C.

```c
int64_t macs_var_1 = INT64_C(5);
printf("%" PRId64 "\n", (macs_var_1 + (INT64_C(2) * INT64_C(3))));
```

A primeira linha cria a variável que correspondia a `n`. Na segunda, os parênteses mantêm `2 * 3` como um ramo inteiro da soma. `PRId64` informa o formato de impressão de um inteiro de 64 bits. Esse tamanho é uma regra da MACSLang e continua valendo no executável x86.

#### Como o arquivo de saída é montado

`TIPOS_C` é o dicionário que traduz os nomes dos tipos. Por exemplo, `int` vira `int64_t`, `float` vira `double` e `string` vira `const char *`. `VALORES_INICIAIS` escolhe o valor padrão que será escrito quando uma variável vier sem inicializador, como `0` para um inteiro. `ESCAPES_C` indica como escrever barras, aspas e outros caracteres que têm significado especial no código C.

`string_c(texto)` monta um literal de texto em C. Começa com aspas, percorre os caracteres e usa `ESCAPES_C` para proteger os especiais. Para certos caracteres de controle sem um escape na tabela, escreve um código octal, que é uma representação em base oito aceita pelo C. Fecha as aspas no fim. Esse cuidado permite que uma quebra de linha escrita num texto continue fazendo parte da string quando o C for compilado.

`GeradorC.__init__()` cria uma lista vazia para as linhas e um contador para o recuo. `linha(texto)` coloca os espaços do recuo antes de acrescentar um trecho. `bloco()` abre chaves, aumenta o recuo, chama `comando` para cada instrução e fecha as chaves com o recuo anterior. Assim a saída permanece legível para quem abrir o `.c`.

`gerar(programa)` escreve a inclusão de `macs_runtime.h` e depois emite as funções em duas passagens. Primeiro, escreve os **protótipos**, que avisam ao compilador C quais funções existem, quais parâmetros recebem e que tipo devolvem. Depois, escreve os corpos. `assinatura(funcao)` produz a parte comum às duas passagens usando `TIPOS_C` e os nomes anotados. Quando a função tem zero parâmetros, escreve `void`. O protótipo deixa o compilador C conferir chamadas a funções cujo corpo só aparece mais abaixo. Depois das funções, `gerar` abre `int main(void)`, chama `macs_iniciar()`, traduz os comandos do programa principal e acrescenta `return 0;`. O resultado é a lista de linhas unida numa string que volta para `main.py`.

#### Como cada parte da árvore vira C

`comando()` escolhe a tradução pelo tipo de instrução. Quando encontra `input(n);` e o destino é inteiro, escreve algo como `macs_var_1 = macs_ler_int();`. Quando encontra `print`, chama `imprimir()`. Um `return` leva a expressão traduzida depois de `return`. Uma chamada usada como instrução ganha ponto e vírgula. `if`, `while` e `for` são escritos com a condição e o corpo correspondentes, usando `bloco` para as chaves. Nas partes opcionais do `for`, o gerador escreve texto vazio.

`declaracao_ou_atribuicao()` monta o trecho com tipo, nome e inicializador para uma declaração, ou apenas nome e valor para uma atribuição. Ela devolve esse trecho sem ponto e vírgula. Assim, o chamador pode acrescentar `;` numa linha comum ou usar o trecho dentro de `for (início; condição; atualização)`. Se a parte inicial ou a atualização estão ausentes, a função devolve texto vazio.

`imprimir()` consulta `expressao.tipo`. Para `string` e `bool`, escreve `puts`, que já acrescenta uma nova linha. `como_texto()` mantém a expressão de uma string ou chama uma função como `macs_texto_bool()` para obter o texto de outro tipo. Para inteiros, reais e caracteres, `imprimir` gera `printf` com o formato correspondente, como `PRId64`, `%.15g` ou `%c`.

`expressao()` monta o C para cada tipo de nó. Para uma variável ou chamada, usa o `nome_c` descoberto pela semântica. Em uma chamada, também traduz os argumentos na ordem da lista. Em um operador unário, traduz o operando e escreve o operador antes dele. `literal()` decide a grafia de cada valor. Por exemplo, `5` vira `INT64_C(5)`, `'A'` usa o número ASCII convertido em `char` e uma string usa `string_c`.

`binario()` começa traduzindo os dois lados. Para uma soma de texto, usa `como_texto()` nos operandos e chama `macs_juntar()`. Para `==` ou `!=` entre strings, usa `strcmp`, pois a comparação precisa conferir os caracteres do texto. `strcmp` devolve zero quando os conteúdos são iguais. Nos demais casos, escreve a expressão com o operador C correspondente e parênteses ao redor, como na multiplicação dentro da soma do exemplo.

## Runtime em C

### [runtime/macs_runtime.h](../runtime/macs_runtime.h)

O cabeçalho permite que o C gerado conheça as funções de apoio antes de chamá-las. Por exemplo, a linha `int64_t macs_ler_int(void);` informa que a função recebe zero argumentos e devolve um inteiro de 64 bits. A execução da leitura está no outro arquivo, `macs_runtime.c`.

- **Proteção com `#ifndef`, `#define` e `#endif`.** Na primeira inclusão, define `MACS_RUNTIME_H` e lê as declarações. Se o arquivo for incluído outra vez durante a compilação do mesmo fonte C, a marca definida faz com que as declarações entrem apenas uma vez.
- **Bloco de `#include`.** `inttypes.h` oferece tipos inteiros e formatos de impressão. `stdbool.h` fornece `bool`, `true` e `false`. `stdio.h` fornece a entrada e a saída. `string.h` fornece operações sobre texto, como `strcmp`.
- **Bloco de protótipos.** `macs_iniciar` prepara o runtime. As funções `macs_ler_*` recebem uma linha do usuário e devolvem um valor do tipo indicado. As `macs_texto_*` recebem um valor e devolvem texto. `macs_juntar` recebe dois textos e devolve a junção. `const char *` indica que a função entrega um endereço que permite ler os caracteres.

### [runtime/macs_runtime.c](../runtime/macs_runtime.c)

Este arquivo fornece ações que o executável usa depois que ele é aberto. Imagine que o `.macs` peça `input(numero);`. Na compilação, o gerador escreve uma chamada de leitura. Durante a execução, `macs_ler_int()` lê o que a pessoa digitou e devolve o valor que será guardado em `numero`.

#### Preparação, erros e memória de texto

Os `#include` trazem o próprio cabeçalho e recursos para reconhecer espaços (`ctype.h`), conferir falhas de conversão (`errno.h`), testar números finitos (`math.h`), alocar dados e converter valores (`stdlib.h`) e configurar o console Windows (`windows.h`).

`macs_iniciar()` é chamada no começo do `main` gerado. Ela pede ao Windows a codificação UTF-8 para a entrada e a saída do console. Também chama `atexit(liberar_textos)`, que registra uma função para o encerramento do programa. `falhar()` é usada quando uma operação em execução dá problema. Ela escreve a descrição em `stderr` e encerra com status de falha. Se alguém digitar `abc` no lugar de um inteiro, por exemplo, a mensagem vem desse caminho.

Em C, uma string ocupa uma sequência de bytes e termina com o byte `\0`. Uma função pode precisar devolver o endereço de um texto que vai continuar sendo usado depois que ela acabar. O runtime coloca os endereços dos textos que criou em uma lista chamada `textos`.

- `struct TextoGuardado` descreve um item da lista. `valor` aponta para o texto e `proximo` aponta para o item seguinte. A variável `textos` aponta para o primeiro item e começa com `NULL`, que indica a lista vazia.
- `alocar(tamanho)` pede ao sistema uma quantidade de bytes com `malloc`. Se faltar memória, chama `falhar`. `guardar(valor)` cria um item novo, liga esse item ao início da lista e devolve o endereço do texto recebido.
- `copiar(texto)` mede o conteúdo, reserva bytes para a cópia mais o `\0`, copia os dados com `memcpy` e usa `guardar`. Isso é útil quando a função montou o resultado numa área temporária e precisa devolver uma cópia que continue disponível.
- `liberar_textos()` percorre a lista no encerramento. Para cada item, salva o endereço do próximo e chama `free` para liberar primeiro o texto e depois o próprio item. O próximo endereço permite continuar até a lista acabar.

#### Como o que foi digitado vira um valor

`ler_linha()` cria inicialmente espaço para 64 bytes. Chama `fflush(stdout)` para exibir a última mensagem antes de esperar que alguém digite. Depois, lê um byte por vez com `getchar()` até uma quebra de linha ou o fim da entrada. Se o espaço acabar, verifica se a capacidade pode ser dobrada e usa `realloc` para ampliá-la. Deixa sempre um byte livre para o terminador da string.

No final, a função remove um `\r` final quando existe e acrescenta `\0`. Byte nulo no meio do texto e fim da entrada antes de ser possível ler um valor causam erro. `pular_espacos()` recebe um endereço e avança sobre espaços. As funções que precisam interpretar valores usam a posição seguinte para conferir o restante da linha.

| Função | Como interpreta a linha |
| --- | --- |
| `macs_ler_int()` | Chama `strtoimax` na base 10. Confere se houve conversão, se o número cabe em 64 bits e se o trecho restante só contém espaços. Então libera a linha e devolve o inteiro. Por exemplo, ` 12 ` serve como entrada e `12a` falha porque sobra `a`. |
| `macs_ler_float()` | Chama `strtod` e testa se houve conversão, se sobrou somente espaço e se o resultado é finito. Também verifica erro de faixa e libera o texto temporário. Uma entrada como `7.5` vira um `double`. |
| `macs_ler_char()` | Exige uma linha com exatamente um byte ASCII. Copia esse byte para um `char` antes de liberar o espaço usado na leitura. |
| `macs_ler_bool()` | Desconsidera espaços nas extremidades. Reconhece `true` ou `1` como verdadeiro e `false` ou `0` como falso. Uma palavra fora desse conjunto gera mensagem de erro. |
| `macs_ler_string()` | Usa a linha como texto, preservando os espaços digitados, e registra o endereço com `guardar` para a variável poder usá-lo depois. |

#### Como um valor aparece junto de texto

Para uma expressão como `"Resultado: " + 11`, `macs_texto_int()` usa `snprintf` para escrever `11` como texto e devolve uma cópia guardada. `macs_texto_float()` faz a mesma coisa usando o formato de números reais. `macs_texto_char()` cria um texto curto com o caractere e `\0`. `macs_texto_bool()` escolhe um dos textos fixos `"true"` e `"false"`.

`macs_juntar(esquerda, direita)` mede o tamanho de cada texto em bytes com `strlen`, confere o limite e reserva espaço para os dois mais `\0`. Copia primeiro os bytes da esquerda e depois os da direita. A segunda cópia leva o `\0` junto para encerrar a string. Depois chama `guardar` e devolve o endereço de `Resultado: 11`. Esse resultado pode ir para um `print`, ser atribuído a uma variável ou entrar em outra concatenação.

## Programas de exemplo

### [exemplos/fatorial.macs](../exemplos/fatorial.macs)

Este programa mostra como a MACSLang declara uma função, lê uma entrada e repete uma conta.

1. `func fatorial(numero: int): int` recebe um número e promete devolver um inteiro. A variável `produto` começa em `1` para que a primeira multiplicação use esse valor como início. O `for` começa em `2`, repete enquanto o fator for menor ou igual a `numero` e aumenta o fator em um após cada volta.
2. Com entrada `5`, o `produto` passa por `2`, `6`, `24` e `120`. `return produto` entrega o último valor para quem chamou a função. Para `0` e `1`, o corpo executa zero vezes e o produto continua em `1`.
3. Fora da função, `var entrada: int;` cria o lugar onde o valor digitado será guardado. `print` pede um número e `input(entrada)` lê a entrada. O `if` chama `fatorial` quando o número está entre `0` e `20`. A parte de cima da faixa evita que o resultado ultrapasse o máximo de um inteiro de 64 bits. O `else` avisa quando foi digitado um inteiro fora da faixa.

### [exemplos/calculadora.macs](../exemplos/calculadora.macs)

`dobro(valor: int)` multiplica o argumento por dois e devolve esse resultado. O programa principal declara `numero`, mostra uma mensagem e lê a entrada nessa variável.

O `if` escolhe a frase que será mostrada. Se `numero` for `7`, a condição é verdadeira, e o texto de saída é `Dobro: 14`. Se for `-3`, o `else` chama a mesma função e monta `Número negativo. Dobro: -6`. A diferença entre os ramos está na mensagem. A conta vem da função `dobro` em ambos.

### [exemplos/media.macs](../exemplos/media.macs)

O primeiro trecho lê o nome completo para uma `string`, então valores como `Ana Maria` cabem numa única entrada. O trecho seguinte cria `total` como `float` e `indice` como `int`, começando em `1`.

O `while` usa `indice <= 3`. Em cada volta, lê uma nota, acrescenta ao total e aumenta o índice. Com `6`, `7` e `8`, o total passa por `6`, `13` e `21`. Quando o índice chega a `4`, a condição fica falsa e o comando seguinte divide o total por três.

A média `7` faz com que `aprovado` receba `true`. A variável `situacao` começa em `'R'` e muda para `'A'` no `if`. Os três `print` mostram o nome com a média, a situação e o valor booleano. Esse exemplo passa pelos cinco tipos da linguagem em um programa só.

### [exemplos/invalidos/simbolo.macs](../exemplos/invalidos/simbolo.macs)

Em `print(2 @ 3);`, `ler_token` chega a `@` e procura esse caractere entre os operadores. Como nenhuma categoria se aplica, o lexer emite um erro na posição do símbolo. Use esse arquivo para ver uma falha na leitura dos caracteres.

### [exemplos/invalidos/pontuacao.macs](../exemplos/invalidos/pontuacao.macs)

A declaração de `nota` termina após o número `7`. Quando o parser chama `exigir(";")`, o próximo token disponível é `print`. Por isso o diagnóstico aponta para o segundo comando e diz que esperava um ponto e vírgula depois da declaração.

### [exemplos/invalidos/tipo.macs](../exemplos/invalidos/tipo.macs)

A linha `var idade: int = "sete";` tem a ordem de tokens de uma declaração. Chegando à semântica, o destino é `int` e o valor entre aspas tem tipo `string`. `conferir_atribuicao` rejeita essa combinação e explica os tipos envolvidos.

## Testes automáticos

### [testes/test_compilador.py](../testes/test_compilador.py)

Este arquivo confere se o compilador continua fazendo o que se espera dele. A biblioteca padrão `unittest` procura classes de teste e executa métodos cujo nome começa com `test_`. As chamadas `assertEqual`, `assertIn` e outras comparam o que saiu com o que deveria sair. Quando há várias entradas no mesmo teste, `subTest` identifica qual entrada causou uma falha.

#### Preparação e funções auxiliares

O bloco de imports carrega as fases, a verificação x86 e as ferramentas para abrir programas. `PROJETO` guarda a pasta do compilador, calculada a partir da posição do arquivo de testes. `EXEMPLOS` aponta para os fontes de demonstração.

`analisar(fonte)` roda lexer, parser e semântica diretamente com um texto. É útil em testes pequenos porque a árvore volta para o próprio teste. `chamar_compilador(fonte, *opcoes, pasta=PROJETO)` roda `main.py` como um comando real, usando o mesmo Python da suíte e capturando a saída em UTF-8. `rodar_programa(executavel, entrada)` abre o binário, envia o texto que faria o papel da entrada digitada e captura o resultado. O limite de dez segundos evita ficar esperando indefinidamente se a execução travar.

#### Testes das fases em Python

| Método de `TesteFases` | O que ele faz e por que isso importa |
| --- | --- |
| `test_tokens_comentarios_e_escapes()` | Envia um texto com comentários e `\n` dentro de uma string. Confere as categorias dos tokens, a quebra já interpretada e a linha da declaração que vem depois. Assim verifica a leitura e a atualização da posição. |
| `test_precedencia_da_multiplicacao()` | Monta a AST de `2 + 3 * 4`. Confere que `*` é um nó dentro do `+`, que o tipo é `int` e que a expressão C preserva os parênteses. |
| `test_erros_lexicos_e_sintaticos()` | Envia cinco trechos com problemas como aspas abertas, comentário sem fechamento ou chave ausente. Cada trecho precisa levantar `ErroCompilacao` com a fase prevista e uma posição. |
| `test_erros_semanticos()` | Envia trechos com tipo incorreto, nome repetido, variável fora de escopo, quantidade de argumentos errada, retorno ausente e inteiro grande demais, entre outros. Cada um deve falhar na fase semântica. |
| `test_comandos_para_inspecao()` | Executa `--tokens`, `--ast` e `--check` e busca marcas da saída esperada. Em seguida pede `--emit-c` num caminho com espaços e confirma que a saída do modo é apenas o C. |
| `test_diagnosticos_dos_exemplos_invalidos()` | Abre os três exemplos inválidos, pede `--check` e confere que todos devolvem o código `1` com a fase correta no diagnóstico. |

#### Testes com o executável do Windows

No nível da classe `TesteWindowsX86`, `@unittest.skipUnless` consulta `shutil.which("cl.exe")` para localizar o compilador C. Quando a ferramenta está ausente, a suíte pula essa classe. Com o terminal x86 configurado, `compilar()` pede um `.exe` em um caminho com espaços, confere se o `.c` correspondente existe e chama `verificar_x86` para checar a arquitetura do binário.

- **`test_fatorial_saida_padrao_e_entrada_invalida()`** compila sem `-o`, procura `build\fatorial.exe` e roda o programa com `0`, `1`, `5` e `20`. Depois envia `abc` para verificar o erro de leitura de inteiro.
- **`test_calculadora_e_media()`** executa a calculadora com valor positivo e negativo para observar os dois ramos do `if/else`. Depois roda o exemplo de média com um nome completo e três notas e confere o resultado.
- **`test_escopo_recursao_e_entradas()`** grava um fonte temporário que chama `fib(10)` antes da definição da função. A execução também lê `char`, `bool` e uma string acentuada. Um `for` com a primeira parte vazia tem uma variável `n` interna que vale `5`, enquanto a externa vale `1` depois do laço. A saída inteira precisa ser `55`, `A`, `5` e `1`, uma linha por valor. Assim, o teste cobre chamadas antecipadas, recursão, leitura e escopo em uma única execução.

## Arquivos de apoio

### [.gitignore](../.gitignore)

Se o projeto for acompanhado pelo Git, esse arquivo indica quais saídas temporárias devem ficar de fora da lista de arquivos novos. `__pycache__/` cobre os arquivos de cache do Python. `build/` cobre a pasta criada pela compilação padrão. `*.exe` e `*.obj` cobrem executáveis e arquivos intermediários produzidos pelo compilador C.

### [README.md](../README.md)

Este é o ponto de partida para usar o projeto. O diagrama mostra a ordem das fases. O primeiro bloco de comandos usa `cd /d` para entrar na pasta, `python --version` para conferir o Python e `where cl` para localizar o compilador C. O bloco seguinte cria e executa o fatorial. O exemplo com `-o` mostra como escolher outro caminho, e `dumpbin /headers ...` com `findstr /i machine` permite ver a arquitetura do executável.

O bloco de inspeção usa as quatro opções que param a compilação em diferentes etapas. As três chamadas a `--check` em `exemplos\invalidos` mostram os tipos de diagnóstico. Por fim, `python -m unittest discover -s testes -v` encontra e executa os testes, mostrando o resultado de cada caso.

### [docs/COMO_FUNCIONA.md](COMO_FUNCIONA.md)

O documento acompanha um programa menor, `print(2 + 3 * 4);`. O primeiro bloco mostra o que copiar para `conta.macs`. O segundo mostra a árvore, com a multiplicação abaixo da soma. Os comandos ao redor permitem conferir as fases e ver o resultado `14`. O arquivo também reúne as regras da linguagem e um roteiro de apresentação.

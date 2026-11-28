# Entenda o compilador em C, arquivo por arquivo

Abra o arquivo indicado ao lado deste guia. Você pode usar `Ctrl+F` para encontrar os nomes das funções. O [exemplo de duas linhas](COMO_FUNCIONA.md) mostra uma execução inteira, da variável no `.macs` até o `11` exibido pelo executável.

O compilador recebe **texto**. O lexer divide esse texto em partes. O parser organiza as partes. A semântica verifica a organização e os tipos. O gerador escreve C11. Por fim, o MSVC transforma esse C em um arquivo que roda no Windows. Os quatro primeiros passos do compilador estão em quatro arquivos `.c` separados.

## 1. Entrada e tipos compartilhados

### [src/main.c](../src/main.c)

Este arquivo recebe o comando do terminal. Se você digitar `build\macsc.exe conta.macs --tokens`, aqui são descobertos o nome `conta.macs` e o modo `--tokens`.

- **`Modo` e `Opcoes`.** `Modo` lista os pontos em que o compilador pode parar. `Opcoes` reúne o modo e os caminhos. Como a lista começa em `NORMAL`, a inicialização com `{0}` prepara a compilação normal.
- **`ajuda()` e `ler_opcoes()`.** A primeira mostra o uso no terminal. A segunda passa pelos argumentos e associa cada opção a um modo. Para `--emit-c` ou `-o`, pega também o argumento seguinte, porque é ali que vem o caminho de saída. Uma opção desconhecida ou a falta do nome do fonte gera uma mensagem antes de abrir um arquivo.
- **`nome_arquivo()`, `extensao()` e nomes de saída.** `nome_arquivo` passa pelas barras e guarda a posição depois da última, onde começa o nome. `extensao` procura o último ponto e compara o final. `saida_padrao` troca o final `.macs` por `.exe` e coloca o resultado em `build`. `completar_saida` acrescenta `.exe` ao argumento de `-o` quando ele veio sem extensão.
- **`mostrar_tokens()`.** Percorre o vetor e imprime posição, categoria e texto até `TK_FIM`. O propósito desse marcador é deixar explícito onde a lista termina, inclusive em um arquivo vazio.
- **`wmain()`.** Prepara o console e a limpeza da memória, lê as opções e verifica o nome do fonte. Usa `wchar_t` nos argumentos, que é a forma de receber caminhos Unicode no Windows. Na execução normal, os resultados passam por `lexar`, `analisar_sintaxe` e `analisar_semantica`. Cada modo de inspeção devolve o resultado no ponto correspondente. A geração segue para `salvar_c` ou `gerar_executavel`, em `windows.c`.

### [src/compilador.h](../src/compilador.h)

O cabeçalho define a forma dos dados que passam de um módulo a outro. Ele é incluído em todos os módulos para que um `Token`, por exemplo, tenha os mesmos campos em qualquer arquivo.

| Estrutura ou enumeração | Dados e finalidade |
| --- | --- |
| `ClasseToken` e `Tipo` | A classe responde «qual parte de texto foi lida?». O tipo responde «que valor esta expressão produz?». `INTEIRO` é uma classe de texto e `T_INT` é o tipo que a análise atribui ao nó. Símbolos de um caractere usam o próprio valor do caractere. |
| `Token` | Guarda classe, texto original, valor de string ou char, linha e coluna. `"oi\n"` mantém a barra em `texto` e a quebra interpretada em `valor`. A posição acompanha o token até um possível diagnóstico. |
| `ClasseExpr` e `Expr` | O campo `classe` distingue literal, variável, chamada e operadores. `esq` e `dir` são endereços de outros nós. Em uma chamada, `esq` aponta para o primeiro argumento e `prox` liga os demais. `altura` ajuda a limitar árvores profundas. `tipo`, `id` e `promover_real` são preenchidos durante a verificação. |
| `ClasseCmd` e `Cmd` | Distinguem os comandos e guardam os dados de cada um. Uma declaração usa `nome` e `tipo`. Um `print` usa `expr`. Um `if` usa `expr` para a condição e `corpo` e `alternativa` para os ramos. O `for` usa também `inicio` e `passo`. `prox` liga comandos da mesma lista. |
| `Parametro` e `Funcao` | Cada parâmetro guarda nome e tipo. A função reúne nome, parâmetros, tipo de retorno e corpo. Os campos `id` passam a identificar o parâmetro e a função no C. |
| `Programa` | Contém a lista de funções e a lista de comandos principais. Assim, o gerador sabe o que colocar antes e o que colocar dentro do `main` em C. |

O restante do cabeçalho declara as funções compartilhadas. Uma declaração permite ao C conferir a assinatura de uma função definida em outro arquivo. A proteção `#ifndef` evita repetir essas definições no mesmo fonte.

### [src/apoio.c](../src/apoio.c)

Aqui ficam a memória compartilhada e a forma das mensagens. Cada token, nó de árvore, símbolo e caminho criado durante a compilação pode ser liberado quando o processo termina.

- **`Alocacao`, `alocacoes` e `alocar()`.** `alocar` confere se a multiplicação do número de itens por seu tamanho caberia em `size_t`. Pede espaço inicializado com zeros via `calloc` e liga o endereço a uma lista de alocações. O restante do código pode assumir que campos ainda vazios começam em zero ou `NULL`. Se faltar memória ao criar o registro, devolve o espaço já obtido antes de sinalizar o erro.
- **`copiar_texto()`.** Reserva bytes para uma parte do texto, mais o byte final zero. `memcpy` copia o trecho selecionado. O lexer usa isso para guardar um nome ou símbolo separado do restante da linha.
- **`iniciar_memoria()` e `liberar_memoria()`.** `atexit` registra a limpeza. Quando o processo termina por sucesso ou erro, o programa passa pela lista, liberando os dados e o registro que os acompanha. No modo de memória, registra também `conferir_memoria()` para consultar a biblioteca de depuração depois da limpeza. As funções de `atexit` rodam na ordem inversa do registro.
- **`falha()` e `erro()`.** `falha` mostra uma mensagem geral. `erro` recebe fase, token e descrição e acrescenta linha e coluna. As duas escrevem em `stderr` e encerram com código de falha, o que permite aos testes distinguir um fonte rejeitado de um fonte aceito.
- **`nome_tipo()`, `nome_token()` e `mostrar_texto()`.** As tabelas transformam números internos em nomes legíveis. `mostrar_texto` escreve o conteúdo entre aspas e exibe caracteres especiais como `\n`, para que uma quebra dentro de uma string continue visível na mesma linha da tabela de tokens.

## 2. As quatro fases

### [src/lexer.c](../src/lexer.c) · Léxico

A função deste arquivo é dizer quais partes aparecem no texto. Por exemplo, `var n: int = 5;` vira a sequência `var`, `IDENT(n)`, `:`, `int`, `=`, `INTEIRO(5)` e `;`. O parser recebe essa sequência e pode pedir a próxima categoria sem ler letras uma a uma.

#### Como avança e mantém a posição

`Leitor` guarda um ponteiro para o caractere atual e os números de linha e coluna. `avancar()` pega o caractere e move o ponteiro. Se for quebra de linha, soma um à linha e volta a coluna para `1`. Em UTF-8, os bytes que continuam um caractere acentuado pertencem à mesma coluna.

`ignorar()` pula espaços e comentários antes de cada token. Um comentário de `//` termina na próxima linha. Um comentário aberto com `/*` precisa de `*/`. Quando o fechamento falta, a função usa a posição que guardou na abertura para indicar o problema.

#### Como decide a categoria

`ler_token()` consulta o primeiro caractere. Se `letra()` encontrar letra ASCII ou sublinhado, lê um nome, incluindo os dígitos que vierem depois. Consulta a tabela `reservadas` para separar palavras da linguagem de nomes criados pelo usuário. Se `digito()` identificar um número, lê os dígitos e verifica se há ponto seguido por mais um dígito. Com ponto, usa `TK_REAL`. Sem ponto, usa `TK_INTEIRO`. O `-` de `-5` fica separado, para o parser decidir quando é negação ou subtração.

Se encontrar aspas, escolhe a classe de texto ou caractere e chama `ler_literal()`. Essa função localiza o fim para reservar espaço, lê o conteúdo e usa `ler_escape()` quando encontra uma barra. Por exemplo, `\n` vira uma quebra de linha. A função confere se as aspas fecham e se um `char` tem um único caractere ASCII.

Para símbolos, `ler_token` consulta primeiro os de dois caracteres, como `>=`. Desse modo, `>=` entra inteiro na lista. Em outro caso, tenta um símbolo simples. Um caractere sem categoria, como `@`, gera um erro. O token que foi reconhecido guarda a posição de início e uma cópia do trecho como foi escrito.

#### O que a fase entrega

`lexar()` chama `ignorar` e `ler_token` em um laço. Começa com espaço para 64 tokens e dobra a capacidade quando o vetor fica cheio. Os tokens antigos são copiados para o vetor maior. Por fim, a função devolve o vetor que vai ser percorrido pelo parser, com `TK_FIM` na última posição.

### [src/parser.c](../src/parser.c) · Sintaxe

A função deste arquivo é conferir a ordem das partes e montar a árvore. A lista pode ter `var`, `n` e `int`, por exemplo. É o parser que exige `:` entre o nome e o tipo e guarda tudo junto em uma declaração.

#### Como o parser consome a lista

`Parser` aponta para o primeiro token ainda disponível. `avancar()` devolve esse token e passa ao próximo. `aceitar()` faz isso quando a categoria corresponde, devolvendo `false` quando o trecho opcional falta. `exigir()` faz a conferência para trechos obrigatórios e aponta o token encontrado se houver erro.

`nova_expr()` e `novo_cmd()` reservam espaço para um nó de expressão ou de comando e guardam sua classe e posição. `entrar()` conta níveis de aninhamento e `medir_altura()` confere a profundidade da árvore formada. O limite permite interromper entradas muito profundas com diagnóstico, antes de esgotar a pilha.

#### Como monta expressões

1. `primario()` lê um valor básico. Pode ser um literal, um nome, uma chamada ou um grupo entre parênteses. Quando um nome é seguido de `(`, `chamada()` lê a lista de argumentos. `entre_parenteses()` lê uma expressão inteira antes de exigir `)`.
2. `unario()` lê `!` ou `-` antes de uma expressão e guarda seu operando em `esq`. Por exemplo, `-5` vira um nó de negação com o literal `5` por baixo.
3. `precedencia()` dá uma prioridade a cada operador. `ler_operadores()` lê um lado esquerdo e, ao encontrar um operador, usa uma prioridade maior para ler o lado direito. Em `2 + 3 * 4`, o ramo `3 * 4` é formado antes de entrar na soma. Para operadores de mesma prioridade, o laço forma a árvore da esquerda para a direita. `expressao()` inicia essa leitura no nível mais baixo.

#### Como monta comandos e funções

`declaracao()` lê `var`, nome, `:`, tipo e inicializador opcional. `ler_tipo()` converte a palavra de tipo num valor como `T_INT`. `atribuicao()` usa um nome já lido, exige `=` e lê a expressão a ser guardada no destino.

`comando()` consulta o primeiro token. Escolhe entre declaração, entrada, saída, retorno, atribuição, chamada ou estrutura de controle. Instruções simples fecham em `;`. Um `if` lê condição e bloco, e lê o `else` quando ele vem depois. Um `while` lê condição e bloco. Para `for`, `laco_para()` lê as três partes do cabeçalho e permite que venham vazias. Os dois `;` pertencem ao cabeçalho, por isso `declaracao` e `atribuicao` também podem ser usadas ali. `bloco()` reúne comandos entre chaves.

`funcao()` recebe a sequência depois de `func`, lê nome, parâmetros separados por vírgula, tipo de retorno e corpo. `analisar_sintaxe()` usa essa função no nível superior e põe os outros comandos na segunda lista de `Programa`. Em ambas as listas, uma variável de cauda aponta para o campo `prox` em que o próximo item deve entrar. Dessa forma, a ordem de leitura fica registrada na árvore.

### [src/semantico.c](../src/semantico.c) · Semântica

A função deste arquivo é conferir se cada comando pode funcionar de acordo com a linguagem. `var n: int = "cinco";` tem a forma de uma declaração, então chega a esta fase. Aqui o tipo do valor é comparado ao tipo prometido, e o compilador explica por que `string` não cabe em `int`.

#### Como lembra os nomes e o lugar de cada variável

`Simbolo` guarda nome, tipo e identificador numérico. `Escopo` guarda os símbolos do bloco atual e aponta para o escopo externo. `Semantico` mantém o escopo ativo, as funções disponíveis, o retorno esperado e o contador de variáveis.

`declarar()` procura o nome no escopo atual. Se ele já existe ali, sinaliza duplicidade. Caso contrário, cria um símbolo, atribui um número e o põe na lista. `resolver_variavel()` faz o caminho de consulta, começando pelo escopo atual e seguindo para fora. Quando encontra o nome, copia identificador e tipo para o nó da referência. Assim, se duas regiões têm uma variável `n`, a busca pega primeiro a região mais interna e o gerador consegue dar nomes distintos a elas.

#### Em que ordem verifica funções e comandos

`analisar_semantica()` numera as funções antes de visitar seus corpos. Isso permite que uma função encontre a própria assinatura ou uma função declarada mais abaixo. Depois abre um escopo para os parâmetros de cada função, confere o corpo e exige que ele garanta retorno. Por fim, abre o escopo do programa principal e confere seus comandos.

`verificar_lista()` verifica os comandos de um bloco em ordem. Guarda se algum comando garante retorno e continua verificando os demais. `verificar_cmd()` escolhe a validação conforme cada comando.

- Em bloco, abre e fecha um escopo. Em declaração, confere o inicializador antes de registrar a variável. Em atribuição, consulta o destino antes de conferir o valor.
- Para `input`, resolve a variável de destino. Para `print` ou uma chamada usada como comando, verifica a expressão. Para `return`, exige uma função e compara o valor com o retorno prometido.
- Para `if` e `while`, exige condição de tipo `bool`. O `for` abre escopo para a variável do cabeçalho e seu corpo. Um `if/else` garante retorno quando os dois ramos retornam. Para um laço, a análise considera a possibilidade de zero execuções e deixa a garantia de retorno como falsa.

`conferir_valor()` aceita o mesmo tipo no destino e na origem. Também permite `int` para `float` e marca `promover_real` nesse caso. O gerador usa a marca para escrever a conversão de modo explícito. `conferir_condicao()` exige `bool`. `numero()` é a checagem usada para os tipos numéricos `int` e `float`.

#### Como descobre o tipo de uma expressão

`verificar_expr()` escolhe a regra pelo tipo de nó e escreve o resultado em `expr->tipo`. Para variáveis, usa a tabela de símbolos. Para negação, confere se `!` recebeu um booleano ou se `-` recebeu um número. Para os demais casos, chama as funções abaixo.

| Função | O que faz com os dados |
| --- | --- |
| `verificar_literal()` | Converte números com `strtoimax` ou `strtod`. Confere o limite inteiro de 64 bits e se o valor real é finito. Para textos, caracteres e booleanos, usa a classe do token para descobrir o tipo. |
| `verificar_chamada()` | Procura a função pelo nome. Percorre argumentos e parâmetros juntos, para verificar quantidade e compatibilidade. Grava o número da função e devolve o tipo de retorno. |
| `verificar_binario()` | Consulta os tipos dos dois lados. Dois números podem ser somados. Uma soma com string vira concatenação. Comparações dão `bool`. Resto exige dois inteiros e lógica exige booleanos. Se os números têm tipos diferentes, o inteiro é marcado para conversão nas operações numéricas. |

### [src/gerador.c](../src/gerador.c) · Geração

A função deste arquivo é escrever C equivalente à árvore verificada. Por exemplo, a variável `n` pode virar `macs_var_1` e `2 * 3` fica entre parênteses dentro da soma. As decisões de tipos e escopo já vieram da semântica.

- **Tabelas, `recuar()` e `string_c()`.** `tipos_c` mapeia os tipos e `iniciais` escolhe valores padrão. `recuar` escreve espaços para organizar a saída. `string_c` acrescenta aspas e protege os caracteres que precisam ser recuperados como parte de uma string C, como barra e quebra de linha.
- **`literal()`.** Inteiros usam `INT64_C(...)`. Valores reais são escritos com precisão suficiente para passar de `double` para texto e voltar, e recebem `.0` quando o texto poderia parecer inteiro. `char` usa o valor ASCII, `bool` usa `true` ou `false` e strings usam `string_c`.
- **`expressao()`, `binario()` e `como_texto()`.** `expressao` usa os identificadores descobertos para variáveis e funções. Quando `promover_real` está ativo, envolve o trecho em uma conversão para `double`. `binario` escreve os dois lados na ordem da árvore. Para comparar strings, chama `strcmp`, que devolve zero para textos iguais. Para concatenar, chama `macs_juntar`. `como_texto` chama a rotina de conversão quando um dos lados da concatenação vem de outro tipo.
- **`declaracao_ou_atribuicao()` e `imprimir()`.** A primeira escreve tipo, nome e valor para declaração, ou nome e valor para atribuição. Deixa os separadores para quem usar o trecho, inclusive no `for`. `imprimir` escolhe `puts` para textos e booleanos e `printf` para outros tipos. Para `int`, usa `PRId64`, que corresponde a um inteiro de 64 bits.
- **`bloco()` e `comando()`.** `bloco` escreve chaves e percorre as instruções. `comando` traduz cada uma. Por exemplo, `input(n)` com um inteiro anotado vira uma atribuição com `macs_ler_int()`, e um `while` leva a condição e o corpo da árvore.
- **`assinatura()` e `gerar_c()`.** A assinatura escreve tipo de retorno, nome e lista de parâmetros. `gerar_c` escreve o cabeçalho do runtime e os protótipos, depois escreve as definições. Termina com o `main`, que inicializa o runtime, executa os comandos de nível superior e devolve `0`. As linhas são escritas no `FILE *` recebido do módulo Windows.

## 3. Exibição da árvore e plataforma Windows

### [src/arvore.c](../src/arvore.c)

Este arquivo é chamado no modo `--ast`. `mostrar_arvore()` imprime funções, parâmetros e comandos. `mostrar_cmd()` visita os campos que podem guardar inicialização, condição, passo, destino e corpos. `mostrar_expr()` visita os lados de uma expressão ou percorre a lista de argumentos. `recuo()` acrescenta dois espaços por nível. Assim, a multiplicação aparece à direita da soma quando é filha dela na árvore.

### [src/plataforma.h](../src/plataforma.h) e [src/windows.c](../src/windows.c)

Essa parte cuida de arquivos, caminhos e de chamar o MSVC. O cabeçalho declara as quatro operações que `main.c` usa. Os detalhes ficam em `windows.c` para manter a lógica da linguagem nos outros módulos.

- **`preparar_console()` e `ler_fonte()`.** Configuram o console para UTF-8. A leitura abre o arquivo em modo binário, mede o tamanho e lê os bytes. O Windows valida a codificação com `MultiByteToWideChar`. Depois, o código retira o BOM quando presente e transforma as quebras de linha do Windows em `\n`. O lexer recebe uma string terminada em zero.
- **`juntar()`, `absoluto()`, `pasta_de()` e `criar_pastas()`.** Montam e resolvem os caminhos. `pasta_de` extrai o diretório contendo o arquivo. `criar_pastas` verifica se ele existe, prepara o pai quando necessário e cria a pasta pedida.
- **`pasta_runtime()`, `proteger_runtime()`, `gravar_c()` e `salvar_c()`.** Localizam o runtime a partir da localização de `build\macsc.exe`. Verificam que o `macs_runtime.c` original não seja escolhido como saída. `gravar_c` abre o arquivo e passa ao gerador. `salvar_c` usa esse fluxo no modo `--emit-c` e termina depois de gravar o texto.
- **`preparar_temporaria()` e `limpar_temporaria()`.** Criam uma pasta com identificador próprio para a compilação e registram a limpeza de seus arquivos conhecidos. Os `.obj` e as saídas incompletas ficam ali enquanto `cl.exe` trabalha.
- **`chamar_cl()`.** Localiza `cl.exe` no ambiente Windows. A linha de comando usa `/TC` e `/std:c11` para compilar em C11, `/utf-8` para o texto, `/I` para o runtime e `/MACHINE:X86` para o vinculador. `CreateProcessW` inicia a ferramenta com os argumentos e a pasta temporária. Depois o programa espera o processo, lê o código de saída e fecha seus identificadores.
- **`verificar_x86()`.** Lê `MZ` no começo, encontra a posição do cabeçalho PE e lê o campo `Machine`. `0x014C` indica o x86 solicitado. Só depois dessa conferência o executável fica pronto para ser publicado.
- **`publicar()` e `gerar_executavel()`.** A segunda função organiza o fluxo inteiro, desde os nomes dos arquivos até a conferência. A primeira guarda o C anterior temporariamente, move o C novo e publica o executável. Se o executável estiver em uso e a troca falhar, restaura o C anterior. Assim, abrir o programa de uma compilação válida deixa o par de arquivos anterior disponível.

## 4. Runtime do programa gerado

### [runtime/macs_runtime.h](../runtime/macs_runtime.h)

A proteção de cabeçalho evita declarações repetidas. Os `#include` oferecem tipos, entrada, saída e funções de texto. Os protótipos informam ao compilador C o que cada função recebe e devolve. Por exemplo, `int64_t macs_ler_int(void)` indica que a leitura recebe zero argumentos e devolve um inteiro de 64 bits.

### [runtime/macs_runtime.c](../runtime/macs_runtime.c)

Este código vai para o executável gerado. Quando você roda o fatorial e digita `5`, a função que lê esse texto está aqui.

| Bloco | Como funciona |
| --- | --- |
| `macs_iniciar()` e `falhar()` | A primeira pede o uso de UTF-8 no console e registra a limpeza ao terminar. A segunda imprime a causa e encerra a execução quando a entrada, a memória ou outra operação falha. O modo de memória registra `conferir_memoria()` para verificar os blocos após a limpeza. |
| `TextoGuardado`, `alocar()`, `guardar()`, `copiar()` e `liberar_textos()` | Cada string que precisa sobreviver a uma função fica em memória alocada. `guardar` registra o endereço numa lista. `copiar` cria uma cópia durável de um texto temporário. No fim, `liberar_textos` devolve cada endereço e seu registro. |
| `ler_linha()` e `pular_espacos()` | `ler_linha` lê até Enter, aumenta a capacidade quando necessário e acrescenta `\0`, que marca o fim do texto em C. Também trata fim prematuro de entrada e byte nulo. `pular_espacos` avança sobre espaços para ajudar nas conversões. |
| `macs_ler_int()` e `macs_ler_float()` | Leem texto e convertem com `strtoimax` ou `strtod`. Conferem faixa, conversão e se sobraram caracteres além dos espaços. Liberam a linha temporária e devolvem o valor convertido. |
| `macs_ler_char()`, `macs_ler_bool()` e `macs_ler_string()` | `char` exige um byte ASCII. `bool` aceita `true`, `false`, `1` ou `0`. `string` guarda a linha preservando os espaços. Cada rotina entrega o tipo que a variável espera. |
| `macs_texto_int()`, `macs_texto_float()`, `macs_texto_char()` e `macs_texto_bool()` | Transformam valores em strings para usá-los em expressões como `"Dobro: " + 14`. As rotinas numéricas formatam o valor, a de char monta um texto de um caractere e a de bool devolve um dos textos fixos. |
| `macs_juntar()` | Mede os dois textos, reserva espaço para a soma dos tamanhos e copia as duas partes em sequência, incluindo `\0` ao final. Guarda o resultado e devolve o endereço para impressão ou para outro uso. |

## 5. Compilação, exemplos e testes

### [compilar.bat](../compilar.bat)

O script confere `cl.exe`, entra na pasta do projeto e cria `build`. Em seguida, compila os oito fontes C e pede ao vinculador Windows x86. `/W4 /WX` trata avisos da compilação do compilador como erros. No modo `memoria`, os argumentos são alterados para ligar a biblioteca C de depuração e ativar as verificações de memória. O código de saída do `cl.exe` volta para quem chamou o script.

### Programas MACSLang

| Arquivo | Como ler o programa |
| --- | --- |
| [fatorial.macs](../exemplos/fatorial.macs) | A função começa com produto `1` e multiplica por cada fator de `2` até a entrada. O programa principal lê um inteiro, só calcula dentro da faixa `0` a `20` e mostra o valor. Para `5`, os produtos são `2`, `6`, `24` e `120`. |
| [calculadora.macs](../exemplos/calculadora.macs) | A função dobra seu argumento. O programa principal lê um inteiro, escolhe a mensagem com `if/else` e chama a função nos dois ramos. |
| [media.macs](../exemplos/media.macs) | Lê nome, soma três notas num `while` e divide por três. `bool` indica aprovação e `char` guarda a letra da situação. Os três comandos de saída juntam esses valores ao texto. |
| [simbolo.macs](../exemplos/invalidos/simbolo.macs) | Põe `@` numa expressão. O lexer encontra um caractere fora das categorias da linguagem. |
| [pontuacao.macs](../exemplos/invalidos/pontuacao.macs) | Declara uma variável sem ponto e vírgula. O parser descobre a falta quando encontra o `print` da linha seguinte. |
| [tipo.macs](../exemplos/invalidos/tipo.macs) | Tenta inicializar inteiro com texto. A semântica confere os tipos e rejeita a declaração. |
| [integracao.macs](../testes/integracao.macs) | Chama Fibonacci antes da definição, faz leituras de tipos diferentes e usa sombreamento de variáveis. Também cobre precedência, operadores lógicos, resto e comparação de strings. |

### [testes/testar.ps1](../testes/testar.ps1)

Este script de teste inicia os executáveis do projeto e compara o que eles devolvem. `Invocar` monta os argumentos, envia uma entrada em UTF-8 e captura saída, erro e código de retorno. `Exigir` para o teste se a condição esperada falhar. `Passou` conta os grupos aprovados. No modo adequado, `ConferirMemoria` verifica a mensagem emitida pela biblioteca de depuração.

`Compilar` verifica a geração do C e confere o tipo do binário diretamente pelos bytes. `Rodar` inicia o programa compilado e devolve o texto que ele mostrou. O corpo da suíte usa essas funções para testar as quatro fases, executar os exemplos, confirmar diagnósticos e verificar que uma tentativa de substituir um executável em uso preserva a compilação anterior. No `finally`, remove sua pasta de teste criada dentro de `build` após conferir o caminho.

O arquivo [CONFORMIDADE_E_TESTES.md](CONFORMIDADE_E_TESTES.md) registra o que foi executado. O [README](../README.md) tem os comandos para repetir a validação.

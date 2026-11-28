# Como o compilador funciona

Salve o seguinte arquivo como `conta.macs` na pasta do projeto.

```text
var n: int = 5;
print(n + 2 * 3);
```

O primeiro comando guarda `5` em `n`. O segundo soma `n` ao produto de `2` por `3`. O valor esperado é `11`.

1. Com `build\macsc.exe conta.macs --tokens`, veja que `n` é classificado como `IDENT`, enquanto `var` é uma palavra da linguagem. Cada item traz linha e coluna. `lexer.c` produz essa lista.
2. Com `build\macsc.exe conta.macs --ast`, veja a árvore de `parser.c`. O nó `Binario '+'` contém `Variavel 'n'` e `Binario '*'`. O ramo do `*` contém os números `2` e `3`. A árvore registra que a multiplicação acontece primeiro.
3. Com `build\macsc.exe conta.macs --check`, `semantico.c` confirma que `n` foi declarada, que é `int` e que todos os operandos da conta são inteiros. Se você trocar o `5` por `"cinco"` e repetir, a mensagem indicará o conflito entre `string` e `int`.
4. Com `build\macsc.exe conta.macs --emit-c build\conta.c`, abra o C produzido por `gerador.c`. `n` aparece como `macs_var_1`, e a multiplicação aparece entre parênteses dentro da soma. `printf` usa o formato do inteiro de 64 bits.
5. No prompt x86, rodar `build\macsc.exe conta.macs` chama o `cl.exe`. Em seguida, `build\conta.exe` mostra `11`. O campo `Machine` do executável é conferido antes da publicação.

Para apresentar, faça a conta acima, rode um exemplo inválido com `--check` e compile o fatorial com `5`. Abra o C do fatorial para mostrar onde saíram o laço, a leitura e a impressão. Por fim, mostre o destino x86 com `dumpbin /headers build\fatorial.exe | findstr /i machine`.

O [guia de leitura](EXPLICACAO_DO_CODIGO.md) mostra como as funções em C produzem cada um desses resultados.

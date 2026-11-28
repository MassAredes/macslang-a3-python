param([switch]$Memoria)

$ErrorActionPreference = 'Stop'
$projeto = Split-Path -Parent $PSScriptRoot
$build = Join-Path $projeto 'build'
$nome = if ($Memoria) { 'macsc-memoria.exe' } else { 'macsc.exe' }
$macsc = Join-Path $build $nome
if (-not (Test-Path -LiteralPath $macsc -PathType Leaf)) {
    throw 'Compile o projeto com compilar.bat antes de testar.'
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
# O StreamWriter do PowerShell 5.1 usa a codificacao de entrada do console.
# UTF-8 sem BOM garante que o primeiro byte enviado pertence aos dados do teste.
[Console]::InputEncoding = $utf8
$pasta = Join-Path $build ('testes-' + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $pasta)
$total = 0

function Invocar($arquivo, [string[]]$argumentos, [string]$entrada = '') {
    $config = New-Object System.Diagnostics.ProcessStartInfo
    $config.FileName = $arquivo
    $config.Arguments = ($argumentos | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $config.WorkingDirectory = $pasta
    $config.UseShellExecute = $false
    $config.CreateNoWindow = $true
    $config.RedirectStandardInput = $true
    $config.RedirectStandardOutput = $true
    $config.RedirectStandardError = $true
    $config.StandardOutputEncoding = $utf8
    $config.StandardErrorEncoding = $utf8

    $processo = New-Object System.Diagnostics.Process
    $processo.StartInfo = $config
    try {
        [void]$processo.Start()
        $saida = $processo.StandardOutput.ReadToEndAsync()
        $erro = $processo.StandardError.ReadToEndAsync()
        $processo.StandardInput.Write($entrada)
        $processo.StandardInput.Close()
        if (-not $processo.WaitForExit(30000)) {
            $processo.Kill()
            throw "Tempo limite: $arquivo"
        }
        return @{
            Codigo = $processo.ExitCode
            Saida = $saida.Result.Replace("`r`n", "`n")
            Erro = $erro.Result
        }
    } finally {
        $processo.Dispose()
    }
}

function Exigir($condicao, $mensagem) {
    if (-not $condicao) { throw $mensagem }
}

function ConferirMemoria($resultado, $tipo) {
    if ($Memoria) {
        Exigir ($resultado.Erro -match ("\[memoria\] $tipo sem vazamentos detectados")) $resultado.Erro
    }
}

function Passou($descricao) {
    $script:total++
    Write-Host "PASS $descricao"
}

function Compilar($fonte, $nomeSaida) {
    $exe = Join-Path $pasta $nomeSaida
    $r = Invocar $macsc @($fonte, '-o', $exe)
    Exigir ($r.Codigo -eq 0) ($r.Saida + $r.Erro)
    Exigir (($r.Saida + $r.Erro) -notmatch 'warning [A-Z]+[0-9]+') ($r.Saida + $r.Erro)
    ConferirMemoria $r 'compilador'
    $c = [System.IO.Path]::ChangeExtension($exe, '.c')
    Exigir (Test-Path -LiteralPath $c -PathType Leaf) 'A saida C esta ausente.'
    $bytes = [System.IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 60)
    Exigir ($bytes[0] -eq 77 -and $bytes[1] -eq 90 -and
            [BitConverter]::ToUInt32($bytes, $pe) -eq 0x4550 -and
            [BitConverter]::ToUInt16($bytes, $pe + 4) -eq 0x014c) 'Cabecalho fora do alvo x86.'
    return $exe
}

function Rodar($exe, $dados) {
    $r = Invocar $exe @() $dados
    Exigir ($r.Codigo -eq 0) ($r.Saida + $r.Erro)
    ConferirMemoria $r 'execut.vel'
    return $r.Saida
}

try {
    $fatorial = Join-Path $projeto 'exemplos\fatorial.macs'
    $r = Invocar $macsc @($fatorial, '--tokens')
    Exigir ($r.Codigo -eq 0 -and $r.Saida.Contains('IDENT') -and $r.Saida.Contains('EOF')) ($r.Saida + $r.Erro)
    ConferirMemoria $r 'compilador'
    Passou 'analise lexica'

    $r = Invocar $macsc @($fatorial, '--ast')
    Exigir ($r.Codigo -eq 0 -and $r.Saida.Contains('Funcao fatorial') -and $r.Saida.Contains('Para')) ($r.Saida + $r.Erro)
    ConferirMemoria $r 'compilador'
    Passou 'arvore sintatica'

    $r = Invocar $macsc @($fatorial, '--check')
    Exigir ($r.Codigo -eq 0 -and $r.Saida.StartsWith('OK:')) ($r.Saida + $r.Erro)
    ConferirMemoria $r 'compilador'
    Passou 'checagem semantica'

    $c = Join-Path $pasta 'somente C.c'
    $r = Invocar $macsc @($fatorial, '--emit-c', $c)
    Exigir ($r.Codigo -eq 0 -and (Test-Path -LiteralPath $c)) ($r.Saida + $r.Erro)
    Exigir ([System.IO.File]::ReadAllText($c).Contains('macs_ler_int()')) 'Chamada ausente no C.'
    Exigir (-not (Test-Path -LiteralPath (Join-Path $pasta 'somente C.exe'))) 'O modo C criou um .exe.'
    ConferirMemoria $r 'compilador'
    Passou 'geracao C11 sem vinculacao'

    $exe = Compilar $fatorial 'fatorial com espaco.exe'
    foreach ($caso in @(@('0', '1'), @('1', '1'), @('5', '120'), @('20', '2432902008176640000'))) {
        $texto = Rodar $exe ($caso[0] + "`n")
        Exigir ($texto -match ('O fatorial de ' + $caso[0] + ' . ' + $caso[1] + "`n$")) $texto
    }
    Exigir ((Rodar $exe "-1`n").Contains('fora da faixa')) 'Valor fora da faixa nao foi tratado.'
    $r = Invocar $exe @() "abc`n"
    Exigir ($r.Codigo -ne 0 -and $r.Erro -match 'entrada inv.lida para int') $r.Erro
    if ($Memoria) {
        Exigir ($r.Erro -match '\[memoria\] execut.vel sem vazamentos detectados') $r.Erro
    }
    Passou 'fatorial x86, faixa e erro de entrada'

    $cAnterior = [System.IO.Path]::ChangeExtension($exe, '.c')
    $cAntes = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($cAnterior))
    $exeAntes = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($exe))
    $alternativo = Join-Path $pasta 'outro.macs'
    [System.IO.File]::WriteAllText($alternativo, 'print(123);', $utf8)
    $bloqueio = [System.IO.File]::Open($exe, 'Open', 'Read', 'Read')
    try {
        $r = Invocar $macsc @($alternativo, '-o', $exe)
        Exigir ($r.Codigo -eq 1 -and $r.Erro -match 'publicar o resultado') ($r.Saida + $r.Erro)
        ConferirMemoria $r 'compilador'
    } finally {
        $bloqueio.Dispose()
    }
    Exigir ([Convert]::ToBase64String([System.IO.File]::ReadAllBytes($cAnterior)) -ceq $cAntes) 'O C anterior foi alterado.'
    Exigir ([Convert]::ToBase64String([System.IO.File]::ReadAllBytes($exe)) -ceq $exeAntes) 'O exe anterior foi alterado.'
    Passou 'saida anterior preservada quando o executavel esta em uso'

    $exe = Compilar (Join-Path $projeto 'exemplos\calculadora.macs') 'calculadora.exe'
    Exigir ((Rodar $exe "7`n").Contains('Dobro: 14')) 'Falha no if.'
    Exigir ((Rodar $exe "-3`n").Contains('Dobro: -6')) 'Falha no else.'
    Passou 'condicional if/else nos dois ramos'

    $exe = Compilar (Join-Path $projeto 'exemplos\media.macs') 'media.exe'
    $texto = Rodar $exe "Ana Maria`n6`n7`n8`n"
    Exigir ($texto -match "Ana Maria teve m.dia 7`nSitua..o: A`nAprovado: true`n$") $texto
    Passou 'while e os cinco tipos na media'

    $exe = Compilar (Join-Path $PSScriptRoot 'integracao.macs') 'integracao.exe'
    $texto = Rodar $exe "A`ntrue`nCafe com leite`n7.5`n"
    Exigir ($texto -eq "55`nA`n15`n6`n5`n1`n14`ntrue`n") $texto
    Passou 'recursao, escopo, tipos, precedencia e for com trecho vazio'

    $invalidos = Join-Path $projeto 'exemplos\invalidos'
    foreach ($caso in @(@('simbolo.macs', 'l.xico'), @('pontuacao.macs', 'sint.tico'), @('tipo.macs', 'sem.ntico'))) {
        $r = Invocar $macsc @((Join-Path $invalidos $caso[0]), '--check')
        Exigir ($r.Codigo -eq 1 -and $r.Erro -match ('Erro ' + $caso[1] + ' na linha')) ($r.Saida + $r.Erro)
        ConferirMemoria $r 'compilador'
        Passou ('diagnostico de ' + $caso[0])
    }

    $casos = @(
        'print(desconhecida);',
        'var n: int; var n: int;',
        'func f(): int { if (true) { return 1; } }',
        'func f(n: int): int { return n; } print(f());',
        'if (1) {}',
        'for (var n: int = 0; n < 1; n = n + 1) {} print(n);',
        'return 1;',
        'print(9223372036854775808);'
    )
    $arquivo = Join-Path $pasta 'invalido.macs'
    foreach ($fonte in $casos) {
        [System.IO.File]::WriteAllText($arquivo, $fonte, $utf8)
        $r = Invocar $macsc @($arquivo, '--check')
        Exigir ($r.Codigo -eq 1 -and $r.Erro -match 'Erro sem.ntico na linha') ($fonte + $r.Erro)
        ConferirMemoria $r 'compilador'
    }
    Passou 'oito rejeicoes semanticas adicionais'
    Write-Host "$total grupos de testes aprovados."
} finally {
    # Remove somente a pasta criada por esta rodada, dentro de build.
    $caminho = [System.IO.Path]::GetFullPath($pasta)
    $raiz = [System.IO.Path]::GetFullPath($build) + '\'
    if ($caminho.StartsWith($raiz, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $caminho) -match '^testes-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $caminho -Recurse -Force
    }
}

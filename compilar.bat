@echo off
setlocal
where cl.exe >nul 2>nul
if errorlevel 1 (
    echo Abra o x86 Native Tools Command Prompt do Visual Studio.
    exit /b 1
)
if defined VSCMD_ARG_TGT_ARCH if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
    echo Use o prompt de ferramentas x86.
    exit /b 1
)

pushd "%~dp0"
if not exist build mkdir build
if errorlevel 1 exit /b 1
pushd build

set "MODO=/MT"
set "NOME=macsc.exe"
if /i "%~1"=="memoria" (
    set "MODO=/MTd /DMACS_MEMORIA"
    set "NOME=macsc-memoria.exe"
)

cl /nologo /TC /std:c11 /utf-8 /W4 /WX /Od /D_CRT_SECURE_NO_WARNINGS %MODO% ^
   ..\src\main.c ..\src\apoio.c ..\src\lexer.c ..\src\parser.c ..\src\semantico.c ^
   ..\src\gerador.c ..\src\arvore.c ..\src\windows.c /Fe:%NOME% ^
   /link /MACHINE:X86 /INCREMENTAL:NO
set "RESULTADO=%errorlevel%"
popd
popd
if not "%RESULTADO%"=="0" exit /b %RESULTADO%
echo Disponivel em build\%NOME%

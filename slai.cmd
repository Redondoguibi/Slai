@echo off
setlocal
if not exist "%~dp0bin\slai.exe" (
    echo Slai ainda nao foi montado. Execute: powershell -File "%~dp0scripts\bootstrap.ps1" 1>&2
    exit /b 1
)
"%~dp0bin\slai.exe" %*
exit /b %errorlevel%

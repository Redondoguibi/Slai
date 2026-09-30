@echo off
setlocal
set "PYTHONPATH=%~dp0;%PYTHONPATH%"
python -m slai %*
exit /b %errorlevel%

@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0platformio-root.ps1" %*
exit /b %ERRORLEVEL%

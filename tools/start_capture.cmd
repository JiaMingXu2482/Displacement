@echo off
setlocal
cd /d "%~dp0"
set "CAPTURE_PY=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if exist "%CAPTURE_PY%" (
  "%CAPTURE_PY%" "%~dp0capture_displacement.py" %*
) else (
  python "%~dp0capture_displacement.py" %*
)
pause

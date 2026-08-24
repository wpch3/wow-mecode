@echo off
setlocal
chcp 65001 >nul
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12或3.10。
  pause
  exit /b 2
)
"%PYTHON%" "%~dp0install_g17b0_source.py" --check "D:\TrinityCore"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (echo [PASS] G17-B0源码只读检查完成。) else (echo [FAIL] 不要Apply，请回传本窗口全文。)
pause
exit /b %RC%

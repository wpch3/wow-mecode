@echo off
setlocal
chcp 65001 >nul
set "ROOT=D:\TrinityCore"
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12：%PYTHON%
  pause
  exit /b 2
)
"%PYTHON%" "%~dp0install_f45.py" check "%ROOT%"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo F45 Check未通过，请把本窗口全部文字回传。
pause
exit /b %RC%

@echo off
setlocal
chcp 65001 >nul
set "ROOT=D:\TrinityCore"
set "WS=C:\Users\Administrator\Downloads\workspace"
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] Python 3.12/3.10 not found.
  pause
  exit /b 2
)
tasklist /FI "IMAGENAME eq worldserver.exe" 2>nul | find /I "worldserver.exe" >nul
if not errorlevel 1 (
  echo [STOP] worldserver is running. Stop it first.
  pause
  exit /b 3
)
echo === F45R1: silence the AoE loot announcement at the source ===
"%PYTHON%" "%~dp0tools\install_f45r1.py" apply "%ROOT%" "%WS%\uploads"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [F45R1] PATCH PASSED. Install G17-B3R11 next ^(it compiles worldserver^), or rebuild manually.
) else (
  echo [F45R1] PATCH FAILED. Paste the whole window back.
)
pause
exit /b %RC%

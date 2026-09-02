@echo off
setlocal
chcp 65001 >nul
set "ROOT=D:\TrinityCore"
set "WS=C:\Users\Administrator\Downloads\workspace"
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
echo === F45R1 Rollback (restore the backed-up CustomAoELoot.cpp) ===
"%PYTHON%" "%~dp0tools\install_f45r1.py" rollback "%ROOT%" "%WS%\uploads"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" ( echo [F45R1] ROLLBACK PASSED. Rebuild worldserver. ) else ( echo [F45R1] ROLLBACK FAILED. )
pause
exit /b %RC%

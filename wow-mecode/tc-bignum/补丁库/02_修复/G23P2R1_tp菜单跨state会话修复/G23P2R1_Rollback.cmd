@echo off
setlocal
chcp 65001 >nul
set "ROOT=D:\TC-Build\bin\RelWithDebInfo"
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12：%PYTHON%
  pause
  exit /b 2
)
tasklist /FI "IMAGENAME eq worldserver.exe" 2>nul | find /I "worldserver.exe" >nul
if not errorlevel 1 (
  echo [STOP] worldserver仍在运行，请先从控制台正常停服。
  pause
  exit /b 3
)
"%PYTHON%" "%~dp0install_g23p2r1.py" rollback "%ROOT%"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo G23-P2R1已回滚到P2版custom_teleport.lua；请正常启动worldserver。
) else (
  echo G23-P2R1回滚失败；请回传本窗口全文。
)
pause
exit /b %RC%

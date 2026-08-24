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
tasklist /FI "IMAGENAME eq worldserver.exe" 2>nul | find /I "worldserver.exe" >nul
if not errorlevel 1 (
  echo [STOP] worldserver仍在运行，请先从控制台正常停服。
  pause
  exit /b 3
)
"%PYTHON%" "%~dp0install_f45.py" apply "%ROOT%"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo F45 Apply已通过。下一步只需重新编译worldserver，不需要改conf、SQL或重跑CMake。
) else (
  echo F45 Apply失败，源码不会被部分覆盖；请把本窗口全部文字回传。
)
pause
exit /b %RC%

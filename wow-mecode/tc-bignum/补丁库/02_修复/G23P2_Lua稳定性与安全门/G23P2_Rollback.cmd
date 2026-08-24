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
"%PYTHON%" "%~dp0install_g23p2.py" rollback "%ROOT%"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo G23-P2脚本与配置已精确回滚。SQL数据为防止重复奖励而保留。
) else (
  echo G23-P2回滚失败；不要启动服务端，请把本窗口全部文字回传。
)
pause
exit /b %RC%

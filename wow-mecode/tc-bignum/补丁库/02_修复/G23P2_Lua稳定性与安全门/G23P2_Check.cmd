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
"%PYTHON%" "%~dp0install_g23p2.py" check "%ROOT%"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo G23-P2 Check完成。READY_TO_APPLY时先停服并导入sql目录中的原子奖励SQL。
) else (
  echo G23-P2 Check未通过，请把本窗口全部文字回传；不要强行覆盖。
)
pause
exit /b %RC%

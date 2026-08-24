@echo off
setlocal
chcp 65001 >nul
echo [NOTICE] 只有在G17B0 Check和数据库只读探针均获确认后才运行Apply。
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12或3.10。
  pause
  exit /b 2
)
"%PYTHON%" "%~dp0install_g17b0_source.py" --apply "D:\TrinityCore"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (echo [PASS] G17-B0源码Apply完成。) else (echo [FAIL] Apply已失败关闭；不要手工修改源码。)
pause
exit /b %RC%

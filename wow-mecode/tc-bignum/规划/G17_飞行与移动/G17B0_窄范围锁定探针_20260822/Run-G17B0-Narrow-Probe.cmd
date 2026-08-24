@echo off
setlocal
chcp 65001 >nul
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12：%PYTHON%
  pause
  exit /b 2
)
"%PYTHON%" "%~dp0probe_g17b0_lock.py"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [PASS] 只读探针完成。请到 C:\Users\Administrator\Downloads\workspace\uploads 取最新 G17B0_LOCK_RESULT_*.zip 并回传。
) else (
  echo [FAIL] 请回传本窗口全文；不要手工修改D:\TrinityCore。
)
pause
exit /b %RC%

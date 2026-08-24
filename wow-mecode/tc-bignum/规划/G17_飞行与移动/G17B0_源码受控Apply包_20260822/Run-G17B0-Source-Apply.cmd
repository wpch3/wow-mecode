@echo off
setlocal EnableExtensions
chcp 65001 >nul
set "PYTHONUTF8=1"
set "RESULT_DIR=C:\Users\Administrator\Downloads\workspace\uploads"
set "RESULT=%RESULT_DIR%\G17B0_SOURCE_APPLY_RESULT.txt"
if not exist "%RESULT_DIR%" mkdir "%RESULT_DIR%" >nul 2>&1
call :RUN > "%RESULT%" 2>&1
set "RC=%ERRORLEVEL%"
type "%RESULT%"
echo.
if "%RC%"=="0" (
  echo [PASS] G17-B0受控源码Apply完成；结果已保存：
  echo %RESULT%
) else (
  echo [FAIL] Apply已失败关闭；不要手工改源码、不要导入SQL、不要编译。
  echo 请上传：%RESULT%
)
pause
exit /b %RC%

:RUN
echo G17B0_SOURCE_APPLY_WRAPPER_BEGIN
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
if not exist "%PYTHON%" (
  echo [FAIL] 找不到Python 3.12或3.10。
  echo G17B0_SOURCE_APPLY_WRAPPER_PASS=False
  exit /b 2
)
echo PYTHON=%PYTHON%
"%PYTHON%" "%~dp0run_g17b0_source_apply.py" "D:\TrinityCore"
set "INNER_RC=%ERRORLEVEL%"
if not "%INNER_RC%"=="0" (
  echo G17B0_SOURCE_APPLY_WRAPPER_PASS=False
  exit /b %INNER_RC%
)
echo G17B0_SOURCE_APPLY_WRAPPER_PASS=True
echo G17B0_SOURCE_APPLY_WRAPPER_END
exit /b 0

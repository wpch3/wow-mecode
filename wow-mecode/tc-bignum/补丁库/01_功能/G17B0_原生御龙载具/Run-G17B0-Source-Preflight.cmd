@echo off
setlocal
chcp 65001 >nul
set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist "%PYTHON%" set "PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
if not exist "%PYTHON%" (
  echo [STOP] 找不到Python 3.12或3.10。
  pause
  exit /b 2
)
set "UPLOADS=C:\Users\Administrator\Downloads\workspace\uploads"
if not exist "%UPLOADS%" mkdir "%UPLOADS%"
set "OUT=%UPLOADS%\G17B0_SOURCE_PREFLIGHT_RESULT.txt"
>"%OUT%" echo G17B0_SOURCE_PREFLIGHT_BEGIN
>>"%OUT%" echo SOURCE_ROOT=D:\TrinityCore
>>"%OUT%" echo.
>>"%OUT%" echo ===== INSTALLER_SELF_TEST =====
"%PYTHON%" "%~dp0install_g17b0_source.py" --self-test >>"%OUT%" 2>&1
set "RC_SELF=%ERRORLEVEL%"
>>"%OUT%" echo G17B0_SELFTEST_RC=%RC_SELF%
if not "%RC_SELF%"=="0" goto finish
>>"%OUT%" echo.
>>"%OUT%" echo ===== WINDOWS_SOURCE_CHECK_READ_ONLY =====
"%PYTHON%" "%~dp0install_g17b0_source.py" --check "D:\TrinityCore" >>"%OUT%" 2>&1
set "RC_CHECK=%ERRORLEVEL%"
>>"%OUT%" echo G17B0_CHECK_RC=%RC_CHECK%
goto done
:finish
set "RC_CHECK=NOT_RUN"
>>"%OUT%" echo G17B0_CHECK_RC=NOT_RUN
:done
>>"%OUT%" echo G17B0_SOURCE_PREFLIGHT_END
set "RC=0"
if not "%RC_SELF%"=="0" set "RC=2"
if not "%RC_CHECK%"=="0" set "RC=2"
type "%OUT%"
echo.
echo [RESULT] %OUT%
if "%RC%"=="0" (echo [PASS] 源码预检完成；继续执行DBeaver只读SQL。) else (echo [FAIL] 不要Apply，上传本结果文件。)
pause
exit /b %RC%

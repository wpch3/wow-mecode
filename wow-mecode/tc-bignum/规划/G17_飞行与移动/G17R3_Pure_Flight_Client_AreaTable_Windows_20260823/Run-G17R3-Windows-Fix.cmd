@echo off
setlocal
set "UPLOADS=C:\Users\Administrator\Downloads\workspace\uploads"
set "MASTER=%UPLOADS%\G17R3_WINDOWS_FIX_RESULT.txt"
if not exist "%UPLOADS%" mkdir "%UPLOADS%"
>"%MASTER%" echo G17R3_WINDOWS_FIX_START
echo G17-R3 phase 1: server live-outdoor hardening and rebuild
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17R3-Server.ps1"
set "RC=%ERRORLEVEL%"
if exist "%UPLOADS%\G17R3_SERVER_WINDOWS_FIX_RESULT.txt" type "%UPLOADS%\G17R3_SERVER_WINDOWS_FIX_RESULT.txt" >>"%MASTER%"
if not "%RC%"=="0" goto fail
echo G17-R3 phase 2: owned patch slot AreaTable client upgrade
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Upgrade-G17R3-Client-MPQ.ps1"
set "RC=%ERRORLEVEL%"
if exist "%UPLOADS%\G17R3_CLIENT_MPQ_UPGRADE_RESULT.txt" type "%UPLOADS%\G17R3_CLIENT_MPQ_UPGRADE_RESULT.txt" >>"%MASTER%"
if not "%RC%"=="0" goto fail
>>"%MASTER%" echo G17R3_WINDOWS_FIX_RESULT=PASS
>>"%MASTER%" echo STOP_DO_NOT_START_WORLDSERVER_UNTIL_READING_RESULT
echo G17-R3 completed. Return G17R3_WINDOWS_FIX_RESULT.txt, then start both normally for spell 59961 test.
echo Result: %MASTER%
pause
exit /b 0
:fail
>>"%MASTER%" echo G17R3_WINDOWS_FIX_RESULT=FAIL
>>"%MASTER%" echo STOP_DO_NOT_START_WORLDSERVER
echo G17-R3 stopped safely. Return %MASTER% and do not retry older packages.
pause
exit /b %RC%

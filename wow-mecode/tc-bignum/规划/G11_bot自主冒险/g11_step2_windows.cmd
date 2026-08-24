@echo off
setlocal EnableExtensions

rem G11 Step 2 Windows helper.
rem Put this CMD file in the same folder as install_g11_step2.py.
rem Double-click defaults to a safe menu. CHECK never edits TrinityCore source.

set "SOURCE=D:\TrinityCore"
if not "%~1"=="" set "SOURCE=%~1"
set "INSTALLER=%~dp0install_g11_step2.py"
set "PYTHON="

if not exist "%INSTALLER%" (
    echo [ERROR] Missing installer next to this CMD file:
    echo         %INSTALLER%
    echo.
    echo Put these two files in the same folder:
    echo   install_g11_step2.py
    echo   g11_step2_windows.cmd
    echo.
    pause
    exit /b 2
)

if not exist "%SOURCE%" (
    echo [ERROR] TrinityCore source directory does not exist:
    echo         %SOURCE%
    echo.
    echo You may pass another source directory as the first argument.
    echo Example:
    echo   g11_step2_windows.cmd "D:\TrinityCore"
    echo.
    pause
    exit /b 3
)

call :FIND_PYTHON
if not defined PYTHON (
    echo [ERROR] No working Python executable was found.
    echo.
    echo The broken "py -3" launcher is not used by this helper.
    echo Repair or install 64-bit Python, then run this CMD again.
    echo.
    echo Checked common locations under:
    echo   %LOCALAPPDATA%\Programs\Python
    echo   %ProgramFiles%
    echo   PATH, excluding WindowsApps aliases
    echo.
    pause
    exit /b 4
)

:MENU
cls
echo ================================================================
echo G11 Step 2 - Windows Installation Helper
echo ================================================================
echo Source    : %SOURCE%
echo Installer : %INSTALLER%
echo Python    : %PYTHON%
echo.
echo [1] CHECK only - safe, does not edit source
echo [2] APPLY      - installs G11 into four source files
echo [3] ROLLBACK   - restores verified .g11_step2.bak files
echo [4] Show Python version
echo [5] Exit
echo.
set "CHOICE="
set /p "CHOICE=Choose 1-5: "

if "%CHOICE%"=="1" (
    set "MODE=--check"
    set "LOGNAME=g11_step2_check_result.txt"
    goto RUN_MODE
)
if "%CHOICE%"=="2" goto CONFIRM_APPLY
if "%CHOICE%"=="3" goto CONFIRM_ROLLBACK
if "%CHOICE%"=="4" goto SHOW_VERSION
if "%CHOICE%"=="5" exit /b 0

echo.
echo [ERROR] Invalid choice.
pause
goto MENU

:CONFIRM_APPLY
cls
echo APPLY modifies four files under:
echo   %SOURCE%
echo.
echo Run CHECK first. APPLY will still verify exact hashes and anchors.
echo Type APPLY exactly to continue, or press Enter to cancel.
set "CONFIRM="
set /p "CONFIRM=Confirmation: "
if /I not "%CONFIRM%"=="APPLY" goto MENU
set "MODE=--apply"
set "LOGNAME=g11_step2_apply_result.txt"
goto RUN_MODE

:CONFIRM_ROLLBACK
cls
echo ROLLBACK restores the four verified G11 backup files.
echo Type ROLLBACK exactly to continue, or press Enter to cancel.
set "CONFIRM="
set /p "CONFIRM=Confirmation: "
if /I not "%CONFIRM%"=="ROLLBACK" goto MENU
set "MODE=--rollback"
set "LOGNAME=g11_step2_rollback_result.txt"
goto RUN_MODE

:SHOW_VERSION
cls
echo Python executable:
echo   %PYTHON%
echo.
"%PYTHON%" --version
echo.
pause
goto MENU

:RUN_MODE
cls
echo [INFO] Running:
echo "%PYTHON%" "%INSTALLER%" "%SOURCE%" %MODE%
echo.
"%PYTHON%" "%INSTALLER%" "%SOURCE%" %MODE% > "%~dp0%LOGNAME%" 2>&1
set "RESULT=%ERRORLEVEL%"
type "%~dp0%LOGNAME%"
echo.
echo ================================================================
echo Exit code: %RESULT%
echo Full output saved to:
echo   %~dp0%LOGNAME%
echo ================================================================
echo.
if "%RESULT%"=="0" (
    echo [OK] Command completed successfully.
) else (
    echo [ERROR] Command failed. Send the complete result TXT file back.
    echo Do not bypass hash or anchor checks.
)
echo.
pause
goto MENU

:FIND_PYTHON
for %%V in (314 313 312 311 310 39 38) do call :TRY_PYTHON "%LOCALAPPDATA%\Programs\Python\Python%%V\python.exe"
if defined PYTHON exit /b 0
for %%V in (314 313 312 311 310 39 38) do call :TRY_PYTHON "%ProgramFiles%\Python%%V\python.exe"
if defined PYTHON exit /b 0
for /f "delims=" %%P in ('where python.exe 2^>nul') do call :TRY_PYTHON "%%P"
if defined PYTHON exit /b 0
for /f "delims=" %%P in ('where python3.exe 2^>nul') do call :TRY_PYTHON "%%P"
exit /b 0

:TRY_PYTHON
if defined PYTHON exit /b 0
if not exist "%~1" exit /b 0
echo(%~1| findstr /I /C:"\WindowsApps\" >nul
if not errorlevel 1 exit /b 0
"%~1" --version >nul 2>&1
if errorlevel 1 exit /b 0
set "PYTHON=%~1"
exit /b 0

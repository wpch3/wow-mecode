@echo off
chcp 936 >nul
setlocal enabledelayedexpansion

:: ===========================================================
::  难度档位切换工具
::  放在 D:\TC-Build\bin\RelWithDebInfo\ 下双击运行
:: ===========================================================

set "DIR=%~dp0worldserver.conf.d"

if not exist "%DIR%" (
    echo [错误] 找不到目录: %DIR%
    echo 请把 worldserver.conf.d 文件夹放到本 bat 同级目录
    pause & exit /b 1
)

echo.
echo ==========================================
echo    当前生效的档位
echo ==========================================
set FOUND=0
for %%t in (casual adventure epic hardcore legend) do (
    if exist "%DIR%\%%t.conf" (
        echo    ^>^> %%t
        set FOUND=1
    )
)
if !FOUND!==0 echo    (无 - 将使用 worldserver.conf 原始设置)

echo.
echo ==========================================
echo    选择要切换的档位
echo ==========================================
echo    1. casual     休闲 - 无脑爽（等同你原本设置）
echo    2. adventure  冒险 - 轻度保留
echo    3. epic       史诗 - 剧情推荐 [推荐]
echo    4. hardcore   硬核 - 接近原版
echo    5. legend     传奇 - 极限挑战
echo    0. 全部关闭（用 worldserver.conf 原始设置）
echo.
set /p CHOICE=请输入编号: 

if "%CHOICE%"=="1" set TIER=casual
if "%CHOICE%"=="2" set TIER=adventure
if "%CHOICE%"=="3" set TIER=epic
if "%CHOICE%"=="4" set TIER=hardcore
if "%CHOICE%"=="5" set TIER=legend
if "%CHOICE%"=="0" set TIER=NONE

if "%TIER%"=="" (
    echo [错误] 无效的选择
    pause & exit /b 1
)

:: 先把所有 .conf 改成 .conf.off
for %%t in (casual adventure epic hardcore legend) do (
    if exist "%DIR%\%%t.conf" ren "%DIR%\%%t.conf" "%%t.conf.off" 2>nul
)

if "%TIER%"=="NONE" (
    echo.
    echo [完成] 已关闭全部档位，将使用 worldserver.conf 原始设置
    echo        重启 worldserver 生效
    pause & exit /b 0
)

:: 启用选中的档位
if exist "%DIR%\%TIER%.conf.off" (
    ren "%DIR%\%TIER%.conf.off" "%TIER%.conf"
    echo.
    echo [完成] 已切换到: %TIER%
    echo        重启 worldserver 生效
    echo.
    echo        启动时留意这行日志确认加载成功:
    echo        Loaded additional config file ...%TIER%.conf
) else (
    echo [错误] 找不到 %DIR%\%TIER%.conf.off
)

echo.
pause

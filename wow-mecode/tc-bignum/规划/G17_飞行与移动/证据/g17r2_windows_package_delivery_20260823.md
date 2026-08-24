# G17-R2 Windows唯一包交付证据（2026-08-23）

## 唯一执行包

`tc-bignum/规划/G17_飞行与移动/G17R2_Pure_Flying_Server_Gate_Windows_20260823.zip`

```text
SIZE=75105
SHA256=e63724d3e3081103159c5e509aa5fed5aa58c7772295a8a4f7f559e5b86c003d
ZIP_FILES=8
ZIP_CRC_TEST=PASS
ZIP_MANIFEST_BAD=0
ZIP_UNLISTED=0
ZIP_STALE=0
EXTRACTED_SELFTEST=10/10_PASS
POWERSHELL_PARSER=7.6.5
POWERSHELL_PARSE_ERRORS=0
PS1_CRLF_ONLY=True
PS1_NON_ASCII_BYTES=0
```

## 包范围

- 只修改`src/server/game/Spells/SpellInfo.cpp`严格区域门；
- 携带G17-A原始前镜像和G17-R2完整后镜像；
- 不重复G17-R1源码安装；只读复验其SHA作为前置门；
- 不执行SQL；
- 不安装或修改客户端MPQ；
- Windows PowerShell 5.1安全native输出捕获；
- 优先直接调用Python312/Python310，不调用`py.exe`或WindowsApps别名；
- 需要fresh `SpellInfo*.obj`、更新后的EXE/PDB；首次应用需要EXE SHA变化；
- 执行完成不自动启动worldserver。

## 未冒充的状态

交付时状态为NOT_RUN；随后用户已确认：

```text
G17R2_WINDOWS_BUILD_RESULT=PASS_USER_CONFIRMED
G17R2_59961_WETLANDS_RUNTIME=FAIL_NOT_HERE
```

本轮没有附原始构建结果全文，也没有回报R2服务器marker；当前转R2A只读分流，禁止重复R2。

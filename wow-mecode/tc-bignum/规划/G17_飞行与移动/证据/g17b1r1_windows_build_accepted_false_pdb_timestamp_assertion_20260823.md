# G17-B1R1真实Windows构建接受：PDB前时间戳未快照导致最终假FAIL

日期：2026-08-23

原始报告：`G17B1R1_WINDOWS_BUILD_RESULT_20260823.txt`，3152字节，SHA-256=`aa5f752185762c8537c446749d33134a6971b45ad113c115a422fe11482f7bd7`。

## 已通过的真实门槛

- native selftest stdout/stderr均出现，exit=0；
- Python source apply exit=0；
- 源码从R1前像`10a7002...9b2f45`变为B1后像`2c7594d0...a68199`；
- MSBuild exit=0；
- 输出明确编译`cs_dragonriding.cpp`并链接`worldserver.exe`；
- fresh dragonriding OBJ=1，大小2197559；
- EXE SHA从`15005e8f...172a9`变为`dc85bd93...a2581`；
- 新EXE=36783616字节；
- BUILD_START=`2026-08-23T08:23:44.9793377Z`；EXE/PDB时间均为`2026-08-23T08:24:42Z`，严格晚于构建开始。

以上证明B1源码已进入真实新worldserver构建。乱码仅为MSBuild中文输出被错误代码页显示，不影响0退出或链接结果。

## 最终红字为什么是假阴性

B1R1构建前执行`$bp=Get-Item $Pdb`，但未在构建前读取并保存`$bp.LastWriteTimeUtc`。构建后判断才首次访问该属性，此时取得的是新PDB时间；它与构建后`$ap.LastWriteTimeUtc`相等，错误触发`exe/pdb timestamp did not advance`。

这不是PDB未更新，也不是C++编译失败。权威接受器逐项复验输出：`G17B1R1_WINDOWS_BUILD_ACCEPTANCE=PASS`。

## 状态与后续

- `G17B1_WINDOWS_BUILD=PASS_ACCEPTED_FALSE_FINAL_ASSERTION`
- 不回滚、不重编、不重跑B1R1安装器；
- 可以正常启动当前`D:\TC-Build\bin\RelWithDebInfo\worldserver.exe`进入B1 Runtime；
- B1 Runtime仍待地面坐骑、59961和室内解除三项。

规范脚本缺陷已在`补丁库/02_修复/G17B1R2_PDB前时间戳快照假失败修复/`修正为构建前值类型快照；本次用户无需执行该修复包。

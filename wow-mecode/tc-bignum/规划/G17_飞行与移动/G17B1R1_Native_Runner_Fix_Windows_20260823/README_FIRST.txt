G17-B1R1：修复Windows PowerShell原生参数卡死，并继续完成B1安装构建
日期：2026-08-23

先处理仍在卡住的旧窗口
----------------------
1. 在旧G17B1 CMD窗口按Ctrl+C；若无反应，直接关闭该窗口。
2. 不需要回滚源码：旧日志停在PYTHON路径之后、NATIVE_SELFTEST之前，且SOURCE_SHA256_BEFORE仍为R1前像10a7002...9b2f45。
3. 这证明旧包尚未进入Python source apply、MSBuild、SQL或客户端写入。
4. 旧`01_Install_Build_G17B1.cmd`永久禁止重跑。

根因
----
旧PowerShell函数把形参命名为`$Args`。PowerShell的`$args`是大小写不敏感的自动变量，导致`& cmd.exe @Args`可能没有收到`/d /c ...`参数；cmd.exe因此进入交互模式等待输入，看起来永久卡死。

B1R1修复
--------
- 参数重命名为`$NativeArgs`；
- 所有调用改为`-FilePath/-NativeArgs/-Prefix`显式命名绑定；
- 采用此前R2真实Windows已通过的native stderr安全捕获结构；
- Python继续只使用Python312/Python310直达路径，不使用py.exe；
- 接受当前源码是R1前像或B1后像，因此即使窗口关闭时机不同也会安全判定；
- 不运行SQL、不改客户端、不改R5。

安装
----
1. 确认旧窗口已经关闭，worldserver正常停止。
2. 解压本B1R1新包到C:\Users\Administrator\Downloads\workspace\uploads。
3. 运行：01_Install_Build_G17B1R1.cmd
4. 新结果必须出现以下连续进度，而不能再停在PYTHON：
   NATIVE_SELFTEST|G17B1R1_NATIVE_STDOUT
   NATIVE_SELFTEST|G17B1R1_NATIVE_STDERR
   NATIVE_SELFTEST_EXIT=0
   SOURCE_APPLY_EXIT=0
   MSBUILD_EXIT=0
5. 最终必须包含：G17B1R1_WINDOWS_BUILD_RESULT=PASS
6. 若FAIL，不启动worldserver，回传新结果文件。

结果文件
--------
C:\Users\Administrator\Downloads\workspace\uploads\G17B1R1_WINDOWS_BUILD_RESULT.txt

PASS后Runtime
-------------
正常启动worldserver：
- `.dragon status`应显示INACTIVE auto=on；
- 点击一个拥有的地面坐骑普通按钮，应保留外观、出现可控四技能Vehicle并可离地；
- 点击59961普通按钮，应保留红色始祖幼龙并自动接管；
- 进入真实室内应安全解除。

B1R1只修安装器卡死，B1游戏源码和验收范围不变。B1通过后继续B2五档动量与1200%安全极速。

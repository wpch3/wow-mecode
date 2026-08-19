# tools 目录

## check_dup.py  重复代码检测（F28）

```bash
python tools/check_dup.py D:/TrinityCore/src
```

检查补丁代码有没有被重复粘贴。**函数体内的语句重复编译器不报错**，
F27 的交易 bug 就是这么来的。退出码 1 = 有重复。

说明见 `补丁库/02_修复/F28_全面去重自检/工具使用说明.md`

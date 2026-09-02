# F45"自检字段"问题处理记录（2026-08-27）

## 用户报告
- F45 群体拾取的"自检字段"（聊天播报：发现%u 合法%u 完成尸体%u…保留[标志%u 权限%u 剥皮%u roll格%u 背包错误%u 上限%u]）太碍事，想回滚
- F45 回滚报错：4 条 package pre/post hash mismatch（CustomAoELoot.cpp/.h）＋ 一行中文被当命令执行

## 诊断（逐行核对 install_f45.py verify_package()）
1. **报错与源码无关**：verify_package() 校验的是**包自身**的 original/ 与 payload/ 镜像（在任何源码检查/写入之前）。用户机器上的 F45 解压文件夹被污染——包内镜像哈希（cpp fcc33c7e/fe872f19、h e1bfbeef/b6651b3e）与锁哈希（9a88/3492/4cb4/1142）全部不符＝新旧版本文件混杂（对旧解压目录重复解压所致，同 C3v2 教训）。D:\TrinityCore 源码未被触碰。
2. **中文命令报错**：同一污染的解压产物里 CMD 行被截断（"echo F45回滚未执"＋换行＋"行，请把本窗口全部文字回传。"）。仓库正式包的 CMD 完好。
3. **根本不需要回滚**：自检播报背后有现成配置开关——`sConfigMgr->GetBoolDefault("AoELoot.Announce", true)`（CustomAoELoot.cpp:344-346，original 与 payload 两版都有）。设 0 即静默。回滚反而会带回 F45 修掉的"随机漏尸＋组队金币语义"老 bug。

## 处理方案（已交用户）
1. worldserver.conf 添加一行：`AoELoot.Announce = 0` → 重启 worldserver（无需编译）
2. 删除机器上被污染的 F45 解压文件夹（勿重试回滚）
3. 与 G17-B3R10 一起生效：B3R10 装完重启即两者都到位

## 状态
- F45 保持已应用状态（拾取修复保留），仅播报静默
- F45 正式包（仓库 F45_delivery_20260822.zip）镜像校验完好，无缺陷

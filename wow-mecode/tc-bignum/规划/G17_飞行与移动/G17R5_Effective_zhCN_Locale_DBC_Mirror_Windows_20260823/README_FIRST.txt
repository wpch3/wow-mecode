G17-R5：把已经通过R4哈希验收的同一个MPQ原样镜像到有效zhCN locale封装槽
日期：2026-08-23

为什么不是继续加AreaTable Flag
---------------------------------
R3已经让湿地AreaTable行含0x400；R4又加入0x4000，但游戏内IsFlyableArea()仍为nil。
若客户端实际采用其中任一AreaTable，湿地父区和米奈希尔港/海湾/城堡相关子区均已含0x400，API不应继续为nil。
同时R1 Spell窄补丁在根Data自定义MPQ中也没有产生普通按钮效果。
两条证据共同指向：根Data的patch-Z.MPQ内容正确，但不是zhCN客户端最后实际采用的DBC来源。
R4真实报告还明确显示Data\zhCN\patch-zhCN-Z.MPQ是目录而非封装MPQ。

R5做什么
--------
1. 只读验证R4状态、根patch-Z.MPQ整体哈希、MPQ v2/4文件及内部Spell/Area哈希；
2. 拒绝覆盖任何已有Y槽；审计其它zhCN字母槽是否已有Spell/Area碰撞；
3. 把已验证的根R4 MPQ逐字节原样复制到空闲且优先级高的：
   D:\WOW\Data\zhCN\patch-zhCN-Y.MPQ
4. 再次解包核验内部Spell/Area；清除Cache；写入可精确回滚状态。

它不做什么
----------
- 不改服务端DBC、数据库或源码；
- 不改/删除根D:\WOW\Data\patch-Z.MPQ；
- 不改/删除目录型patch-zhCN-Z.MPQ；
- 不增加新Flag，不用无头骑士Spell伪装；
- 不覆盖任何未知文件。

安装
----
1. 确认R4已安装PASS，完全退出Wow.exe。
2. 解压本包到C:\Users\Administrator\Downloads\workspace\uploads。
3. 右键“以管理员身份运行”：01_Install_G17R5_Locale_Mirror.cmd
4. 结果必须含：G17R5_LOCALE_MIRROR_RESULT=PASS
5. 重启客户端并进入世界，不要只退回角色界面。

游戏内最小验收（先站在湿地旷野，离开建筑）
------------------------------------------
/run print("G17R5",GetZoneText(),GetSubZoneText(),IsIndoors(),IsFlyableArea())

期望末项为1。随后从坐骑页/法术书普通按钮召唤59961；必须能上马、起飞、水平移动、降落。
真实室内必须仍拒绝。

回传
----
请回传：
C:\Users\Administrator\Downloads\workspace\uploads\G17R5_LOCALE_MIRROR_RESULT.txt
以及上面宏的整行输出、普通按钮59961结果。

若API仍为nil，R5仍不会把安装PASS冒充Runtime PASS；此时根据该有效locale槽结果继续做可见canary，而不是盲加Flag。

回滚
----
运行02_Rollback_G17R5_Locale_Mirror.cmd。它只在当前Y槽哈希仍等于R5状态时移出该槽，并保留rescue副本；根R4 MPQ不动。

# G17-R5 真实Windows客户端与Runtime验收（2026-08-23）

## 安装原始结果

原件：`G17R5_LOCALE_MIRROR_RESULT_20260823.txt`

- `G17R5_LOCALE_MIRROR_RESULT=PASS`
- 根R4 MPQ：`D:\WOW\Data\patch-Z.MPQ`
- locale镜像：`D:\WOW\Data\zhCN\patch-zhCN-Y.MPQ`
- MPQ格式2、文件数4；内部Spell/Area哈希完全正确；其它locale DBC碰撞0；Cache已清除。
- 根R4 MPQ未改；服务端未改。

## 用户真实Runtime结论

用户原话摘要：

> 完全可以起飞和召唤上马，非常成功，进室内就会解除召唤，没有问题。

因此按最终行为门判定：

```text
G17R5_NORMAL_MOUNT_BUTTON_SUMMON=PASS
G17R5_MOUNT_AND_TAKEOFF=PASS
G17R5_HORIZONTAL_FLIGHT=PASS
G17R5_INDOOR_FORCED_DISMOUNT=PASS
G17R5_REAL_WINDOWS_RUNTIME=PASS
G17_PURE_FLYING_MOUNT_GATE=CLOSED_PASS
```

用户执行多参数`print`宏时聊天框只显示`G17R5`，没有显示后续参数；这不能作为API值记录，也不推翻已经通过的真实召唤、上马、起飞、移动和室内解除行为。后续若需3.3.5a API取证，应改用单字符串拼接，而不是多参数`print`。

## 根因闭环

R4根`Data` MPQ的内容正确但没有产生Runtime效果；同一归档逐字节放入有效封装的zhCN locale Y槽后，普通按钮和飞行行为立即通过。最终根因归类为：

```text
G17_PURE_FLIGHT_ROOT_CAUSE=EFFECTIVE_ZHCN_LOCALE_DBC_LOAD_SLOT_REQUIRED
```

R1–R5纯飞行线关闭，禁止重复。下一阶段进入G17-B1全坐骑自动接管与类型会话。

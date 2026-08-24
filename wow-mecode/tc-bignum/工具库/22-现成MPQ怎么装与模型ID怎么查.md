# 现成 MPQ 怎么装 + 模型 ID 到底去哪查

> 制定：2026-08-02
> 回答两个问题：
> 1. `.model` 非要这么麻烦吗？没有分享模型 id 的网站吗？
> 2. 已经打包好的 MPQ 还可以装进 MPQ 文件夹吗？

---

# 第一部分：模型 ID 去哪查

## 一、有网站，但网站的号不一定对得上你

| 来源 | 怎么用 | 对你有效吗 |
|---|---|---|
| wowhead 页面源码搜 `displayId` | 打开 npc 页面，右键查看源码，Ctrl+F `displayId` | 号是**暴雪原版**的 |
| dMorph 之类现成列表 | 直接抄 | 同上 |
| wow.tools | **已于 2025 年 5 月关站** | 用不了 |
| wago.tools | 在线 DB2 浏览器 | 主要是**正式服**数据，不是 3.3.5 |
| **你自己的 DBC** | 见下 | **100% 准确** |

### 为什么网站的号"不一定"对得上

关键在于**你打了 HD 补丁**。

补丁替换的是**模型文件本身**（`Creature\Wolf\Wolf.m2` 换成 WoD 高模），
**没有改 displayid 到模型的映射关系**。所以：

- **大部分情况下，网站的号是能用的**
- 但只要补丁作者动过 `CreatureDisplayInfo.dbc`（很多整合包会动），
  或者你的服务端 DBC 和客户端 DBC 不一致，号就会错

step31 v1 那次 32 个号全错，**根因不是补丁**，是我直接编的号。
但这件事暴露了一个真问题：**没有校验手段**。

---

## 二、真答案：权威表在你自己硬盘里

```
D:\TC-Build\bin\RelWithDebInfo\dbc\CreatureDisplayInfo.dbc
D:\TC-Build\bin\RelWithDebInfo\dbc\CreatureModelData.dbc
```

这两张表 JOIN 起来就是完整的模型 ID 总表：

```
displayid --CreatureDisplayInfo--> ModelID --CreatureModelData--> 模型文件路径
  26232                              2775              Creature\Nerubian\Nerubian.mdx
```

因为服务端和客户端读的是**同一套 DBC**，所以从这里读出来的号
在你的环境里一定成立。

### 已经给你做好两个工具

**A. 游戏内搜（step32 `.findmodel`）**

```
.fm wolf          搜所有狼模型，直接出 displayid
.fm sel           选中一个怪，读出它的模型号
.fm id 26232      反查这个号是什么
.fm npc 24191     看这个生物用什么模型
```

搜完直接 `.model id <号>`。**看到喜欢的怪，选中，读号，变身**，三步。

**B. 离线导出 Excel 表**

```bash
python 工具库/tools/dump_models.py "D:\TC-Build\bin\RelWithDebInfo\dbc"
```

生成 `模型总表.csv`，Excel 打开，几万行随便筛。

---

## 三、所以 `.model` 还麻烦吗

不麻烦了。原来的流程是：

```
想变成蜘蛛 -> 不知道号 -> 上网搜 -> 抄个号 -> 试 -> 错了 -> 再试
```

现在是：

```
.fm spider  ->  看到号  ->  .model id <号>
```

或者更快：

```
看到一只喜欢的怪 -> 选中 -> .fm sel -> .model id <号>
```

---

# 第二部分：现成的 MPQ 能直接装吗

## 一、能。而且这是最省事的情况

**直接把 `.MPQ` 文件丢进 `Data\` 文件夹就行。** 不用解压，不用重打包。

```
D:\WOW\Data\patch-4.MPQ      <- 丢这里
```

前提只有两条：

1. **文件名必须叫 `patch-<字符>.MPQ`**
2. **那个字符不能和已有的撞车**

---

## 二、命名规则

客户端只认这个格式：

```
patch.MPQ           <- 官方的
patch-2.MPQ         <- 官方的
patch-3.MPQ         <- 官方的
patch-4.MPQ ... patch-9.MPQ     <- 你可以用
patch-A.MPQ ... patch-Z.MPQ     <- 你可以用
```

**加载顺序**：数字先，字母后，字母表顺序。**后加载的覆盖先加载的**。

```
patch -> patch-2 -> patch-3 -> patch-4 ... patch-9 -> patch-A ... patch-Z
                                                                    ^
                                                              优先级最高
```

### 你现在的占用情况（来自之前的记录）

```
Data\zhCN\   A(HD人物+音乐)  D(登入界面)  F(开放DBC)  M(副本GPS)
Data\        patch-2(含官方3.3.0)  3(HD生物+NPC贴图)  4(HD武器装备)
             C(HD环境)  F(HD法术液体天空血液)  H(虎/迅猛龙还原,可选)
```

`Data\` 里**已占用**：2 3 4 C F H
`Data\` 里**空闲**：5 6 7 8 9 A B D E G I J K L M N O P Q R S T U V W X Y Z

所以下一个新补丁，**建议用 `patch-5.MPQ`**，或者想让它压过所有 HD 补丁
就用 `patch-Z.MPQ`。

---

## 三、【重要】locale 目录整体压过 Data 目录

这条规则很多人不知道，你之前也踩过：

```
Data\zhCN\patch-zhCN-A.MPQ      <- 优先级【高】
Data\patch-Z.MPQ                <- 优先级【低】，哪怕字母是 Z
```

**`Data\zhCN\` 里的任何一个补丁，都压过 `Data\` 里的所有补丁。**

所以：
- 想让新补丁**压过一切**，放 `Data\zhCN\patch-zhCN-Z.MPQ`
- 想让它**只压过 Data 里的**，放 `Data\patch-Z.MPQ`

---

## 四、拿到一个现成 MPQ，装之前先检查三件事

### 1. 它是不是真的 MPQ

用 MPQ Editor 打开试试。打不开就不是，或者是加密的。

### 2. 里面的目录层级对不对

打开后**第一层应该直接是** `Creature` / `Character` / `Interface` / `DBFilesClient` 这种。

```
正确：
  patch-X.MPQ
    Creature\
    Character\

错误（多套了一层）：
  patch-X.MPQ
    我的补丁v3\
      Creature\
```

**多套一层就完全不生效，而且不报错。** 这是最常见的坑。
如果层级错了，就得用 MPQ Editor 重新打包。

### 3. MPQ 版本号

3.3.5 客户端能读 **V1 和 V2** 格式。V3/V4 是 Cataclysm 之后的，读不了。

用 MPQ Editor 建新包时，「Game Compatibility」选 **World of Warcraft**
就是 V2，安全。

绝大多数给 3.3.5 做的补丁都是 V1/V2，一般不用担心。
如果一个 MPQ 死活不生效又检查不出问题，可以往这上面想。

---

## 五、【坑】MPQ 是整文件替换，不能合并

这条之前记过，再强调一次，因为它经常咬人：

**两个 MPQ 里有同名文件时，优先级高的那个【整个文件】赢，不会合并。**

具体后果：

- 你装了一套 HD 人物模型（`patch-A`），
  又装了一个只改某个 `.skin` 的小补丁（`patch-Z`），
  那个 `.skin` 会把 HD 模型那套的对应文件顶掉 -> **模型花掉或消失**

- `.m2` / `.skin` / `.blp` 是一套的，**必须同一个来源**。
  混着装就会出问题。

所以装现成 MPQ 时：**先想清楚它会覆盖掉谁**。

---

## 六、装完不生效怎么查

按顺序排查：

| 检查 | 怎么做 |
|---|---|
| 1. 文件名对不对 | 必须 `patch-X.MPQ`，X 是单个数字或字母 |
| 2. 放对目录没有 | `Data\` 或 `Data\zhCN\` |
| 3. 有没有被更高优先级的盖住 | `Data\zhCN\` 压过 `Data\`；字母大的压字母小的 |
| 4. 层级有没有多套一层 | MPQ Editor 打开看第一层 |
| 5. 客户端有没有完全退出重开 | MPQ 是启动时加载的 |
| 6. 是不是 V3/V4 格式 | MPQ Editor 看 |

诊断脚本：

```bash
python 工具库/tools/patch_scan.py "D:\WOW\Data"
```

---

## 七、槽位用完了怎么办 —— `patch-AA` / `patch-ZZ` 行不行

**不行。客户端只认【单个字符】。**

```
patch-4.MPQ      认
patch-Z.MPQ      认
patch-AA.MPQ     不认，静默忽略
patch-ZZ.MPQ     不认，静默忽略
patch-10.MPQ     不认，静默忽略
```

**注意"静默"两个字**：放进去不会报错，客户端照常启动，
就是那个补丁的内容完全不生效。所以这个坑很难自己发现。

### 你有多少槽位

每个目录 **36 个**（`0-9` 十个 + `A-Z` 二十六个），
而且 `Data\` 和 `Data\zhCN\` 是**两套独立的 36 个**。

用脚本扫一下就知道还剩几个：

```bash
python 工具库/tools/patch_slots.py "D:\WOW\Data"
```

按你目前的占用（`Data\` 用了 2 3 4 C F H，`zhCN\` 用了 A D F M）：

```
Data\      已用 6，空闲 30
Data\zhCN\ 已用 4，空闲 32
合计还有 62 个
```

**62 个空位，离用完还远得很。** 这个问题现在不用担心。

### 万一真用完了，三条出路

按推荐顺序：

**1. 合并（最优）**

一个 MPQ 内部能装的文件数量**没有实际上限**。
槽位不够，就把几个补丁合成一个：

用 MPQ Editor 把 A 补丁和 B 补丁的内容都拖进同一个 `patch-5.MPQ`。

> 前提：两个补丁**没有同名文件冲突**。有冲突就得先决定谁赢
> —— 这跟第五节说的"整文件替换"是同一个道理。

**2. 挪到 locale 目录**

`Data\zhCN\` 是另外独立的 36 个槽位。
`Data\` 满了就往那边放，改名成 `patch-zhCN-X.MPQ` 即可。

代价：locale 目录整体压过 `Data\`，优先级会变高，
可能盖住你本来想让它生效的东西。挪之前想清楚。

**3. 文件夹形式**

你的 exe 打过补丁，**支持把文件夹当 MPQ 用**（之前确认过）。
文件夹和 MPQ 文件在优先级上是平等的，都按名字排。

这条路不占 MPQ 槽位，但只有打过补丁的 exe 才认。

---

## 八、一句话总结

**现成的 MPQ：改个不撞车的【单字符】名字，丢进 `Data\`，重启客户端。完事。**

比自己打包省事得多 —— 自己打包才要操心目录层级，
现成的包作者已经弄好了。

---

## 附：相关文档

- `01-MPQ编辑器.md` —— MPQ Editor 用法
- `15-把压缩包做成MPQ补丁.md` —— 拿到的是 zip/rar 时怎么自己打包
- `18-补丁不生效诊断与exe换不了.md` —— 优先级速查表
- `21-HD模型整个消失的原因与解法.md` —— 混装导致模型花掉
- `patches/step32_模型搜索/安装说明.md` —— `.findmodel` 指令

## 附：本文用到的脚本

```bash
# 扫描补丁槽位占用，看还剩几个位置
python 工具库/tools/patch_slots.py "D:\WOW\Data"

# 导出模型总表（Excel 可开）
python 工具库/tools/dump_models.py "D:\TC-Build\bin\RelWithDebInfo\dbc"

# 补丁内容体检（贴图编码/DBC列数）
python 工具库/tools/patch_scan.py "补丁目录"
```

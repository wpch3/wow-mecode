# 模型补丁安装 / BfA 模型能不能用

> 制定日期：2026-07-31
> 场景：下到一个"争霸艾泽拉斯全种族模型"补丁，里面有 `.blp` + `.m2` + `.skin`，
> 不知道能不能在 3.3.5a 用，也不知道怎么装。

---

## 一、先跑体检，别猜

**M2 文件头里明明白白写着版本号，读一下就知道。**

```bash
python 工具库/tools/patch_scan.py "D:/下载/你的补丁解压目录"
```

只用 Python 标准库，不装任何东西。**只读，不改文件。**

它直接读二进制文件头，输出四段：文件清点 / **M2 版本判定** / BLP 编码检查 / 结论。

---

## 二、判定依据（wowdev.wiki 权威版本表）

### 第一层：魔数 —— 生死线

| 魔数 | 含义 | 3.3.5 能用吗 |
|---|---|---|
| **MD20** | 传统格式 | **看版本号（见下）** |
| **MD21** | Legion+ **分块格式** | **完全不认**，崩溃或不显示 |

wowdev.wiki 原文：

> From Legion and up, the file might be **chunked** instead.
> If this is the case, the magic will be anything but 'MD20'
> and the m2 data will be in the 'MD21' chunk.

**BfA (8.0) 的原始模型就是 MD21。** 这是硬性区别，不是"凑合能用"的问题。

### 第二层：版本号

| 版本 | 资料片 | 3.3.5 |
|---|---|---|
| 256-257 | Classic | 低版本，一般能读 |
| 260-263 | TBC | 能读 |
| **264** | **WotLK 3.3.5** | **原生，直接用** |
| 265-271 | Cataclysm | 不行 |
| 272 | MoP / WoD | 不行 |
| 273-274 | Legion / BfA / SL | 不行 |

**你要的数字就是 264。**

### 第三层：分块类型（决定转换难度）

如果是 MD21，脚本会列出内部分块。这两个尤其关键：

| 分块 | 从哪个版本开始 | 影响 |
|---|---|---|
| **TXID** | **BfA 8.0.1.26629** | 贴图引用从"路径字符串"换成**纯数字 FileDataID**，转换时要逐个还原成路径 |
| **SKID** | Legion 7.3 | 骨骼放在外部 `.skel` 文件，**多数转换器不支持** |

Adspartan 的 Legion→WotLK Multi Converter 说明里明写：

> **M2 using .skel are not supported.**

**所以：补丁里如果有 SKID 块，基本就是死路**，除非作者已经处理过。

---

## 三、三种结果，三条路

### 结果 A：全是 MD20 v264 —— 直接能用

```
>> 【可以直接用】
   全部 N 个模型都是 MD20 v264 = 原生 WotLK 3.3.5 格式。
```

**恭喜，作者已经移植好了。** 你下的是成品，不是原料。跳到第四节看怎么装。

### 结果 B：有 MD21 —— 不能直接用

```
>> 【不能直接用】
   补丁里有 N 个 Legion+ 分块格式(MD21)模型。
```

说明这是**从正式服直接扒的原始文件**。要用必须先转换，见第五节。

**但先想清楚**：全种族人物模型的转换工作量极大，而社区**已经有做好的成品**
（见第六节）。自己转不划算。

### 结果 C：MD20 但版本不是 264 —— 差一步

已经转过一轮但没到位，再跑一次转换器降到 264。

---

## 四、怎么装（结果 A 的情况）

### 4.1 先确认目录结构

补丁解压后，内部路径**必须和客户端 MPQ 里一致**。人物模型的标准路径长这样：

```
Character\
  Human\
    Female\
      HumanFemale.m2
      HumanFemale00.skin
      HumanFemaleSkin00_00.blp
      ...
    Male\
  NightElf\
  Orc\
  ...
```

**如果解压出来第一层就是 `Character` 文件夹，那就对了。**

如果第一层是作者的说明文件夹（比如 `全种族模型v3/Character/...`），
**打包时要从 `Character` 那一层开始**，别把外层文件夹也打进去。

### 4.2 查一个空字母

```bash
bash 工具库/tools/client_check.sh "/d/你的魔兽目录"
```

看第 4 段的字母占用图，记下推荐的空位。假设是 `patch-Y.MPQ`。

> **重要**：你的客户端已经有一堆补丁，很可能**其中一个就是人物模型包**。
> 如果新旧两个模型包同时存在，会打架。装之前先确认现有补丁里有没有
> `Character\` 开头的内容 —— 用 MPQ Editor 打开看一眼。

### 4.3 打包成 MPQ

用 MPQ Editor（批次 2）：

```
1. MPQs -> New MPQ
2. 命名 patch-Y.MPQ，一路 Next 到底
3. 把补丁的 Character 文件夹整个拖进去
4. 如果补丁带 DBC，再建 DBFilesClient 文件夹，把 .dbc 放进去
5. 保存
```

**或者更省事的办法**：如果你的 exe 打过文件夹补丁
（`client_check.sh` 会告诉你），直接在 `Data\` 下建个**文件夹**叫
`patch-Y.MPQ`，把内容丢进去，不用打包。

### 4.4 关键一步：删 Cache

```
删掉客户端根目录的 Cache 文件夹
```

**不删的话改动大概率不生效**，这是最常见的"我装了怎么没变化"的原因。

### 4.5 验收

进游戏看人物。**建议先看角色选择界面**，那里模型加载最快。

---

## 五、如果补丁没带 DBC（很可能出问题）

体检脚本会提示：

```
[注意] 补丁【没带 DBC】。人物模型替换通常需要改
       CharSections.dbc / ChrRaces.dbc
```

**为什么需要改 DBC**：高版本人物贴图在 3.3.5 里走 `_HD.blp` 这套命名。
ownedcore 上做 MoP 人物移植的人原话：

> You need to go into **charSections.dbc** and put `_HD.blp` for all the missing textures.
> 比如 `Character\Human\Female\HumanFemaleFaceLower03_05.blp`
> 改成 `..._HD.blp`

> 还要改 **chrraces.dbc**，看第 25、26 列。
> 兽人原本 25 是 1、26 是 1，改成 25=2、26=0

**这就是为什么批次 3（WDBX Editor）对你从"可选"变成"必需"。**

流程：
1. 从客户端 MPQ 里提出 `CharSections.dbc` 和 `ChrRaces.dbc`
2. WDBX Editor 打开
3. 按上面的规则改
4. 存回去，放进你的 `patch-Y.MPQ` 的 `DBFilesClient\` 目录

> **但注意**：好的移植包**通常会自带改好的 DBC**。
> 如果作者没带，多半说明这个包不是给 3.3.5 做的。

---

## 六、结果 B 的两条路（有 MD21 时）

### 路线一（推荐）：换一个已移植的成品包

社区已经有人把 WoD/Legion 人物模型完整移植到 3.3.5，
**模型 + 贴图 + DBC 全套配套**，解压丢 Data 就能用。

| 包 | 说明 |
|---|---|
| **Leeviathan 版** | 人物 + NPC，社区评价最好，含血精灵 |
| **Finsternis 版** | 人物 + 生物 + 坐骑，覆盖面广 |
| Warmane 官方 WoD 包 | 自带在 Warmane 客户端里，**但公认较差** |

Warmane 论坛上有人对比后原话：

> This model patch is way much better than Warmane's one CHARACTERS wise.
> It also includes Blood Elves, which is absolutely incredible.

**注意冲突**：这几个包不能混装。装 Leeviathan 的要先删 Warmane 的
`patch-w.mpq` 和 `Data\enUS\patch-enUS-w.mpq`。

**已知问题**（论坛实录，装之前知道比装完困惑好）：
- Reload UI 有几率不加载 MPQ，出现白色无贴图物体，重登即可
- 少数 NPC（燃烧军团兽人、食人魔）贴图可能缺失
- 达拉然可能报错 → 用 **4GB Patch**（`ntcore.com/files/4gb_patch.zip`）

### 路线二：自己转（工作量大，不推荐用于全种族人物）

需要的工具链：

| 工具 | 用途 | 地址 |
|---|---|---|
| **CASCExplorer** | 从正式服提取原始文件 | github.com/WoW-Tools/CASCExplorer/releases |
| **Legion→WotLK Multi Converter** | 主力转换器 | model-changing.net/files/file/62 |
| **010 Editor** | 跑转换脚本（Inico 的批处理需要）| sweetscape.com（付费）|
| MPQ Editor | 打包 | zezula.net |

**Multi Converter 用法**：打开 → 把文件/文件夹拖进去 → 点 Fix。
`.skin` 在同目录会一起转。已转过的不会重复转。

**已知限制**（作者原话）：
- **M2 using .skel are not supported** ← 就是有 SKID 块的那些
- 前向飞行动画部分模型有问题
- 粒子动画未完成

**为什么不推荐**：
1. 人物模型骨骼复杂，转换后动画容易出错
2. BfA 有 TXID 块，贴图引用是数字 ID，要逐个还原成路径
3. 全种族 = 十几个种族 × 男女 = 二十多套，每套都要验
4. **社区已有成品**，重复劳动

---

## 七、你的情况怎么判断

你说补丁里有 `.blp` + `.m2` + `.skin`，这是个**好信号**：

| 信号 | 含义 |
|---|---|
| 有 `.m2` | 不是纯贴图包，UV 错位风险排除 |
| 有 `.skin` | 网格文件配套齐全 |
| **有 `.skin` 这件事本身** | Legion+ 原始文件的 skin 是**通过 SFID 块引用 FileDataID** 的，<br>能以独立 `.skin` 文件形式存在，**说明很可能已经转过** |

**但"争霸艾泽拉斯模型"这个说法指的可能是"模型来源"而不是"文件格式"。**
移植包也会说自己是"BfA 模型"，因为素材确实来自 BfA。

**所以必须跑一下脚本才知道。** 一秒钟的事。

---

## 八、装之前的三个提醒

### 1. 你的客户端已有补丁，先查冲突

整合包作者很可能已经装了人物模型包。两个包同时存在会打架。
用 MPQ Editor 挨个打开现有 `patch-*.MPQ`，看有没有 `Character\` 开头的内容。

### 2. 备份

装之前把 `Data` 目录的文件列表记一下（`client_check.sh` 的输出就够）。
出问题时把你加的那个字母的包**改名加个下划线**就能禁用，不用删。

社区惯用做法：`patch-Y.MPQ` → `patch-Y.MPQ_`

### 3. 全种族包体积大，先试一个种族

如果补丁支持，先只放一个种族的文件夹（比如 `Character\Human\`）测试。
成功了再放全部。省得出问题时不知道是哪个种族的锅。

---

## 九、决策流程图

```
                    跑 patch_scan.py
                          |
              +-----------+-----------+
              |           |           |
          全MD20 v264   有MD21    MD20但非264
              |           |           |
        [直接能用]        |      [再转一次]
              |           |
              |     有 SKID 块?
              |        /      \
              |      是        否
              |       |         |
              |  [基本没戏]  [可以转，但麻烦]
              |       |         |
              |       +----+----+
              |            |
              |     强烈建议改用
              |     Leeviathan / Finsternis 成品包
              |
        打包 patch-Y.MPQ
              |
        补丁带 DBC 吗?
           /      \
         带        没带
          |         |
          |    要自己改 CharSections.dbc
          |    + ChrRaces.dbc（批次3 WDBX）
          |         |
          +----+----+
               |
          删 Cache 文件夹
               |
          进游戏验收
               |
        人物没变化? -> 90% 是没删 Cache
                    -> 或者字母优先级不够
                    -> 或者缺 DBC
```

---

## 十、常见故障对照

| 现象 | 原因 |
|---|---|
| **进游戏毫无变化** | 没删 Cache（最常见）|
| 变化了但只有部分种族 | 缺 DBC，或那几个种族文件没打进去 |
| **人物变成白模/无贴图** | BLP 路径不对，或 Reload UI 导致 MPQ 没加载（重登）|
| 人物贴图错位（脸贴胳膊） | 贴图和模型不配套 —— 典型的"只搬贴图不搬模型" |
| 直接崩溃 | MD21 格式，3.3.5 读不了 |
| 达拉然报错 | 内存不足，用 4GB Patch |
| 装完游戏起不来 | 字母冲突，覆盖了官方文件 → 改名加下划线禁用 |

---

## 十一、和批次的关系（更新）

原路线图里批次 2、3 是"可缓"，**你这个需求下变成必需**：

| 批次 | 原定位 | 现在 |
|---|---|---|
| 0 · idTip + 体检 | 立刻装 | 不变 |
| 1 · WoWDatabaseEditor | 立刻装 | 不变 |
| **2 · MPQ Editor** | 可缓 | **必需**（打包 + 查冲突）|
| **3 · WDBX Editor** | 可缓 | **补丁不带 DBC 时必需** |
| 4 · exe 补丁 | 可选 | 仍可选（文件夹补丁能省事）|
| 5 · 素材工具 | 按需 | **只有走"自己转"才需要** |

**如果 `patch_scan.py` 说"可以直接用"且补丁带 DBC，
那你只需要批次 2，连批次 3 都能省。**

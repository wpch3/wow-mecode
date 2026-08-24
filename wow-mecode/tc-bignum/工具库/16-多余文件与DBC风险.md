# 补丁里有多余种族会出错吗 / DBC 才是真风险

> 制定：2026-07-31
> 起因：用户问"如果 patch 里有还未出现的种族，是否会直接跳过不会出错？"

---

## 一、直接回答：不会出错，会被完全忽略

**WoW 客户端是"按需索取"的，不是"扫描加载"的。**

工作方式：

```
客户端要显示一个人类女性
   -> 查 DBC，得知模型路径 Character\Human\Female\HumanFemale.m2
   -> 按这个路径去 MPQ 里【找】
   -> 找到就用，找不到就报错
```

**它从来不会去遍历 MPQ 里有什么。** 所以：

| 情况 | 结果 |
|---|---|
| MPQ 里有 `Character\Vulpera\...`（3.3.5 没这个种族）| **完全忽略**，没有任何影响 |
| MPQ 里有 100 个用不到的模型 | 忽略，只占硬盘空间 |
| MPQ 里有 `Character\Human\...` | 正常覆盖生效 |

ownedcore 上 stoneharry（版主，做客户端精简的）原话印证了反向的情况：

> The DBC's just map the files to records of data that can be used by the maps and client.
> **If you delete the files from the MPQ that the DBC specifies, then I think it will still
> work in most situations unless that piece of data is requested to be used.**

意思是：**没被请求到的资源，存在与否都无所谓。** 反过来也成立 —— 多出来的资源不会被碰。

### 唯一的代价

**硬盘空间和打包时间。** 一个全种族包可能 1-2 GB，其中一半你用不到。

如果想瘦身，可以只挑你要的种族目录打包。但**不挑也不会出错**。

---

## 二、【但是】真正的风险在别处 —— DBC

**你问错了地方。多余的模型无害，但同一个包里的 DBC 可能让客户端直接起不来。**

### 为什么 DBC 危险

DBC 是**定长结构的二进制表**，客户端启动时会校验列数。列数对不上直接拒绝运行：

```
Error #121 (0x85100079) Version Mismatch
DBFilesClient\ChrRaces.dbc has wrong number of columns (found 68, expected 69)
```

这是 ownedcore 上的真实报错记录。**注意：只差 1 列就报错。**

### 为什么补丁里的 DBC 可能不对

如果补丁作者是**从高版本客户端提取的 DBC**，列数完全不一样：

| DBC | 3.3.5 列数 | 高版本 |
|---|---|---|
| `ChrRaces.dbc` | **69** | BfA 是 90+ |
| `CharSections.dbc` | **10** | 各版本不同 |

**模型文件可以是移植好的 v264，但 DBC 忘了转 —— 这种组合很常见。**

而且症状很吓人：**客户端直接起不来**，比"模型不显示"严重得多。

---

## 三、脚本已经会查这个了

`patch_scan.py` 更新了，现在会逐个核对 DBC 列数：

```bash
python 工具库/tools/patch_scan.py "D:/下载/你的补丁"
```

### 输出示例

**情况好**：
```
  [重要] 补丁自带 DBC。DBC 列数错了客户端会【拒绝启动】，逐个查：

    [OK] CharSections.dbc             10 列 / 800 条  符合 3.3.5
    [OK] ChrRaces.dbc                 69 列 / 20 条  符合 3.3.5

  >> 【可以直接用】
     2 个 DBC 列数也都对，一起放进 DBFilesClient\ 目录。
```

**情况坏**：
```
    [!!] ChrRaces.dbc                 96 列 / 30 条  <<< 3.3.5 要求 69 列

    [严重] 1 个 DBC 列数不符！
           客户端会报 Error #121 Version Mismatch 并【无法启动】

  >> 【模型能用，但 DBC 有问题 -- 不要整包直接装】
     做法：打包时【只放模型和贴图，不要放这些 DBC】
```

### 内置对照表

脚本知道这些表在 3.3.5 的正确列数：

```
chrraces.dbc            69
charsections.dbc        10
creaturedisplayinfo.dbc 16
creaturemodeldata.dbc   30
item.dbc                 8
itemdisplayinfo.dbc     25
chrclasses.dbc          60
charstartoutfit.dbc     77
charhairgeosets.dbc      9
```

不在表里的会标 `[??] 无对照数据`，需要你自己判断。

---

## 四、如果 DBC 列数不对怎么办

**不要放弃整个补丁 —— 模型是好的，只是 DBC 不能用。**

### 做法

```
1. 打包时【只放 Character 文件夹】，不放那些 DBC
2. 先这样装上试试
```

### 两种结果

| 结果 | 说明 |
|---|---|
| **模型正常显示** | 完事了，根本不需要那些 DBC |
| 模型没变化 | 才需要改 DBC，见下 |

### 真需要改 DBC 时

**用你自己客户端的原版 DBC 改，不要用补丁带的。**

```
1. MPQ Editor 从客户端提出原版 CharSections.dbc
2. WDBX Editor 打开（选 WotLK 3.3.5）
3. 按 13 篇第五节的规则改（贴图路径加 _HD 后缀等）
4. 存回去，放进你的 patch-Y.MPQ 的 DBFilesClient\ 目录
```

这样列数一定是对的，因为基底就是你客户端的。

---

## 五、装之前的最后检查清单

```
[ ] findroot.sh 找到正确的打包根目录
[ ] m2ver.sh / patch_scan.py 确认模型是 MD20 v264
[ ] patch_scan.py 确认 DBC 列数没问题（或决定不放 DBC）
[ ] MPQ Editor 查过现有补丁有没有 Character\ 冲突
[ ] 打包后重新打开自检，第一层是 Character
[ ] 复制到 Data\
[ ] 删掉 Cache\ 文件夹
```

**七项，五分钟。** 少做一项可能排查两小时。

---

## 六、风险等级排序（供参考）

| 问题 | 症状 | 严重度 |
|---|---|---|
| **DBC 列数不符** | **客户端起不来** | **最高** |
| M2 是 MD21 格式 | 崩溃或不显示 | 高 |
| 目录层级错 | 装了没变化 | 中（难排查）|
| 没删 Cache | 装了没变化 | 中（好解决）|
| 字母被占 | 被别的补丁覆盖 | 中 |
| **多余种族文件** | **无** | **无** |

**你担心的那项排最后，实际上没有风险。**

---

## 七、一句话

> **多余的模型客户端根本不看，放心。**
>
> **但 DBC 会被逐字节校验，列数错一个客户端就起不来。**
>
> 脚本现在两个都会查。

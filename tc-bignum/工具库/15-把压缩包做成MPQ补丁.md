# 把压缩包做成 MPQ 补丁 —— 逐步图解

> 制定：2026-07-31
> 场景：下到一个模型补丁，**是普通压缩包（zip/rar/7z），不是现成的 .MPQ**，
> 里面一堆 `.m2` / `.skin` / `.blp`，要自己打包成客户端认的 `patch-X.MPQ`。

---

## 零、先明确一件事：这很正常

**大部分模型作者发的就是裸文件压缩包**，不是打好的 MPQ。原因是：

- 用户的 patch 字母占用情况各不相同，作者没法替你决定叫 `patch-4` 还是 `patch-Y`
- 裸文件方便你挑着用（比如只要人类不要兽人）

**所以"不是 patch"不是问题，打包是标准流程，五分钟的事。**

---

## 一、【最关键】先搞清目录层级

**这是整个流程唯一真正会出错的地方。** 层级错一层，游戏完全不认，
而且不报错——你只会看到"装了没变化"，然后排查半天。

### 客户端认的路径长什么样

WoW 读文件用的是**MPQ 内部路径**，人物模型的标准路径是：

```
Character\Human\Female\HumanFemale.m2
Character\Human\Female\HumanFemale00.skin
Character\Human\Female\HumanFemaleSkin00_00.blp
```

**所以你的 MPQ 打开后，第一层必须直接是 `Character`。**

### 解压后可能遇到的三种情况

#### 情况 A：解压出来第一层就是 Character —— 最理想

```
D:\下载\模型包\
    Character\
        Human\
        NightElf\
        ...
```

**打包时选 `D:\下载\模型包\` 这一层。**（选 `Character` 的父目录，不是 Character 本身）

#### 情况 B：外面套了一层作者的文件夹 —— 最常见

```
D:\下载\BfA全种族模型v3\
    BfA全种族模型v3\          <- 解压软件自动加的，或作者自己套的
        Character\
        说明.txt
```

**打包时要选到 `D:\下载\BfA全种族模型v3\BfA全种族模型v3\` 这一层**，
也就是**能直接看到 `Character` 文件夹的那一层**。

> **判断口诀**：你选的那个目录，用资源管理器打开，
> **必须一眼就看到 `Character` 文件夹**。看不到就是选错了。

#### 情况 C：作者按种族分了包

```
D:\下载\模型包\
    人类\
        Character\Human\...
    暗夜精灵\
        Character\NightElf\...
```

这种要**先手动合并**：新建一个空文件夹，把各个包里的 `Character`
**合并**到一起（Windows 会问"是否合并文件夹"，选是）。

---

### 不想自己判断？跑脚本

```bash
bash 工具库/tools/findroot.sh "/d/下载/你解压出来的目录"
```

它会扫描出"能直接看到 `Character` 的那一层"，
并且**直接给你 Windows 格式的路径**，复制粘贴到 MPQ Editor 就行。

三种情况它都能认：
- 第一层就是 Character -> 直接给路径
- 外面套了几层 -> 穿透进去找到正确的那层
- **压缩包里其实是现成的 .MPQ** -> 告诉你不用打包，改名直接用

---

## 二、打包（两种方法，推荐第二种）

### 方法一：先建空包再拖（灵活，适合挑文件）

```
1. MPQ Editor -> MPQs -> New MPQ
2. 输入名字: patch-Y            <- 用 client_check.sh 给你的字母
3. Next
4. 选 "Create an empty MPQ archive"
5. Next -> Next -> Finish
6. 把 Character 文件夹整个拖进右边窗口
7. 有 DBC 的话，右键包名 -> New Folder -> 命名 DBFilesClient
   -> 选中它 -> 点上方绿色加号 -> 添加 .dbc 文件
```

### 方法二：直接从目录构建（推荐，不会拖错）

```
1. MPQ Editor -> MPQs -> New MPQ
2. 输入名字: patch-Y
3. Next
4. 勾选 "Build the MPQ archive from a file or directory"
5. 点 "..." 选择目录 —— 【选那个能看到 Character 的目录】
6. Game Compatibility 点 Change -> 选 World of Warcraft
7. Next -> Next -> Finish
```

**方法二的好处**：它会自动按你选的目录结构建立 MPQ 内部路径，
不会出现"拖进去之后层级不对"的问题。

> **注意 Game Compatibility 这项**。默认可能是 Warcraft III。
> 选错了文件数上限和格式版本会不对，3.3.5 可能读不出来。
> **一定点 Change 改成 World of Warcraft。**

---

## 三、打完包立刻自检（30 秒，别跳）

**关掉再重新用 MPQ Editor 打开你刚做的 `patch-Y.MPQ`。**

看到的应该是：

```
patch-Y.MPQ
  |- Character
  |    |- Human
  |    |    |- Female
  |    |    |    |- HumanFemale.m2
  ...
  |- DBFilesClient        <- 如果有 DBC
```

### 对照表

| 你看到的 | 判断 |
|---|---|
| 第一层是 `Character` | **正确** |
| 第一层是 `模型包v3`，点进去才是 Character | **错了**，多套一层，重做 |
| 第一层直接是 `Human` / `NightElf` | **错了**，少了一层，重做 |
| 一堆 `unknown\xxx` | 没配 listfile，不影响使用，但建议配上 |

**这 30 秒能省你两小时。**

---

## 四、放进客户端 + 删缓存

```
1. 把 patch-Y.MPQ 复制到  <客户端>\Data\
2. 【必做】删掉 <客户端>\Cache\ 整个文件夹
3. 启动游戏
```

**第 2 步不做，90% 概率没变化。**

---

## 五、更省事的路子：文件夹补丁

如果 `client_check.sh` 第 3 段报告**"发现文件夹型补丁"**，
说明你的 exe 支持把文件夹当 MPQ 用。那就**完全不用打包**：

```
在 <客户端>\Data\ 下新建【文件夹】，命名 patch-Y.MPQ
把 Character 文件夹直接放进去
删 Cache，进游戏
```

结构：
```
Data\
  patch-Y.MPQ\          <- 这是个文件夹，不是文件
    Character\
      Human\
      ...
```

**好处**：以后改任何一个贴图，存盘就生效，**不用重新打包**。
调试模型时这是质变。

> 不确定支不支持？用 `11` 篇的"三分钟活体测试"测一下，
> 或者直接试——不支持的话就是没效果，不会有任何损坏。

---

## 六、常见故障

| 现象 | 原因 | 处理 |
|---|---|---|
| **装了毫无变化** | 没删 Cache | 删 `Cache\` 文件夹 |
| 装了毫无变化 | **目录层级错了** | 用第三节自检 |
| 装了毫无变化 | 字母优先级不够 | 换更靠后的字母 |
| 装了毫无变化 | 被别的补丁覆盖 | 查冲突（`14` 篇第 5 步）|
| 游戏起不来 | 字母冲突/覆盖了官方文件 | 改名加下划线禁用 |
| MPQ Editor 建包报错 | Game Compatibility 选错 | 改成 World of Warcraft |
| 包里全是 unknown | 没配 listfile | 配上，或忽略（不影响游戏）|

**万能回退**：
```
patch-Y.MPQ  ->  patch-Y.MPQ_
```
加下划线立刻禁用，客户端恢复原样。不用删文件。

---

## 七、完整流程速查

```
解压补丁
   |
   v
找到"能直接看到 Character 的那一层"      <- 最关键
   |
   v
跑 m2ver.sh 确认版本是 264
   |
   v
MPQ Editor -> New MPQ -> patch-Y
   |
   +-- 勾 "Build from directory"
   +-- 选上面找到的那层目录
   +-- Game Compatibility 改成 World of Warcraft
   |
   v
Finish
   |
   v
重新打开包自检：第一层是不是 Character?    <- 30秒，别跳
   |
   v
复制到 Data\
   |
   v
删 Cache\ 文件夹                          <- 必做
   |
   v
进游戏，先看角色选择界面
```

---

## 八、关于 DBC 的额外提醒

如果补丁里带了 `.dbc` 文件，它们**不能和 Character 放一起**，
必须单独放在 `DBFilesClient\` 目录：

```
patch-Y.MPQ
  |- Character\...
  |- DBFilesClient\
       |- CharSections.dbc
       |- ChrRaces.dbc
```

**用方法二从目录构建时**，要在打包前就把目录结构摆好：

```
你选的目录\
    Character\
    DBFilesClient\
        CharSections.dbc
```

这样构建出来的 MPQ 结构就是对的。

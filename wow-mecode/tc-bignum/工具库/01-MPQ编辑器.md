# Ladik's MPQ Editor —— 数据包解包/打包

> 作者：Ladislav Zezula（StormLib 的作者）
> 官方下载：http://www.zezula.net/en/mpq/download.html
> 镜像：https://www.wowmodding.net/files/file/149-ladiks-mpq-editor/
> **版本：4.0.0.937**（2026-07-31 实查，官方最新。旧文档写的 3.6.0.844 已过时）
>
> **官方有中文版**：http://www.zezula.net/download/mpqeditor_cn.zip
> 英文版：http://www.zezula.net/download/mpqeditor_en.zip
> **listfile 官方包**：http://www.zezula.net/download/listfiles.zip （9.83 MB）
>
> 32+64 位在同一个包里。Windows 7 以上可用，也能在 Wine 9.0+ 上跑。

---

## 一、为什么它是最重要的工具

**所有客户端魔改最终都要落到 MPQ 里。** 模型、贴图、DBC、音效、
不管你用什么工具做出来，最后都得打包成 `patch-4.MPQ` 放进 `Data` 文件夹。

Ladik 是 **StormLib** 的作者 —— 那是所有 MPQ 工具（包括 WMV、wow.export）
底层用的库。用他的编辑器最不容易出兼容问题。

支持的游戏：暗黑 1/2/3、War2/War3、星际 1/2、**WoW 从 Vanilla 到 MoP**。

---

## 二、安装与首次配置

1. 从 zezula.net 下载对应位数的版本（32/64 位都有）
2. 解压即用，绿色软件

### 【必做】配置 Work 目录和 ListFile

很多人第一次用报错就是漏了这步。

1. Tools → Options
2. 设置 **Work directory**（解出来的文件放哪）
3. 设置 **ListFile directory**

**ListFile 是什么**：MPQ 内部只存文件名的哈希，不存明文路径。
没有 listfile，你打开 MPQ 只能看到一堆 `unknown\xxxxx`。
listfile 是一份"哈希 → 真实路径"的对照表。

编辑器自带常用 listfile，但**建议另外下官方的完整包**：

```
http://www.zezula.net/download/listfiles.zip   (9.83 MB)
```

官方说明原话：
> Listfiles for MPQ archives. They are **necessary** for opening older
> MPQ archives using MPQ viewers and editors.

**这个包是解别人整合包 `unknown\xxx` 的关键**，
详见 `11-整合客户端体检与加密应对.md` 第五节。

---

## 三、3.3.5 客户端的 MPQ 结构

```
World of Warcraft/
├── Data/
│   ├── common.MPQ          基础数据（Vanilla）
│   ├── common-2.MPQ        基础数据补充
│   ├── expansion.MPQ       TBC 数据
│   ├── lichking.MPQ        WotLK 数据
│   ├── patch.MPQ           3.1.0 补丁
│   ├── patch-2.MPQ         3.2.0 补丁
│   ├── patch-3.MPQ         3.3.0 补丁
│   └── enUS/               本地化目录（或 zhCN 等）
│       ├── locale-enUS.MPQ 本地化基础
│       ├── base-enUS.MPQ
│       ├── expansion-locale-enUS.MPQ
│       ├── lichking-locale-enUS.MPQ
│       ├── patch-enUS.MPQ
│       ├── patch-enUS-2.MPQ
│       └── patch-enUS-3.MPQ
```

### 加载优先级（后面的覆盖前面的）

```
Data\<locale>\  整体优先于  Data\

Data 内：  common → common-2 → expansion → lichking
           → patch → patch-2 → patch-3
           → patch-4 ... patch-9 → patch-A ... patch-Z   ← 自定义放这
```

**所以自定义补丁命名 `patch-4.MPQ` 起步**，能覆盖所有官方文件。

**命名规则（严格）**：
- `patch-` 开头
- 后接单个字符：`4`-`9` 或 `A`-`Z`
- `.MPQ` **必须大写**

---

## 四、基本操作

### 解包：提取文件

1. MPQs → Open MPQ，选要打开的（比如 `lichking.MPQ`）
2. 左侧目录树浏览
3. 右键要的文件 → **Extract**
4. 文件会出现在你配的 Work directory

**技巧**：可以同时打开多个 MPQ，编辑器会自动按优先级合并显示，
让你看到"游戏实际会用哪个版本的文件"。

### 打包：创建自定义补丁

1. MPQs → **New MPQ**
2. 命名 `patch-4.MPQ`
3. 一路 Next → Done
4. 在新建的 MPQ 里右键 → **New Folder**，建目录
5. 把改好的文件**拖进对应目录**

**目录结构必须和游戏原始路径一致**，例如：

```
patch-4.MPQ/
├── DBFilesClient/              ← DBC 文件放这
│   ├── ItemDisplayInfo.dbc
│   └── CreatureDisplayInfo.dbc
├── Creature/
│   └── murloc2/
│       ├── murloc2.m2
│       └── murloc2.blp
├── Item/
│   └── ObjectComponents/
│       └── Weapon/
│           └── sword_2h_xxx.m2
└── Interface/
    └── Icons/
        └── inv_sword_2h_xxx.blp
```

> **注意大小写**。虽然 Windows 不区分，但保持和原路径一致最保险。

6. 关闭 MPQ，把文件放进 `World of Warcraft\Data\`
7. **删客户端 Cache 文件夹**
8. 重进游戏

---

## 五、【关键】客户端要能加载自定义 MPQ

原版 3.3.5 exe **不一定加载**你的 `patch-4.MPQ`。
需要**打过补丁的客户端 exe**，详见 `00-工具总览与前置条件.md`。

打过补丁的 exe 还有个大好处：
**可以把 `patch-4.MPQ` 做成普通文件夹**，改文件不用重新打包。

```
Data/
└── patch-4.MPQ/          ← 这是个文件夹，不是文件！
    └── DBFilesClient/
        └── ItemDisplayInfo.dbc
```

调试阶段强烈建议用文件夹模式，改一个文件存盘就能测，效率天差地别。

---

## 六、实用技巧

### 直接改 MPQ 内的文件（不推荐但快）

可以直接把新文件拖进**官方 MPQ**（如 `lichking.MPQ`）覆盖同名文件。

**不推荐**，因为：
- 破坏原始文件，出错难恢复
- 官方补丁会覆盖回去
- 分享给别人时要传整个 MPQ

**除非**你只是想快速试一下效果。试完记得改回来（先备份）。

### 找某个文件在哪个 MPQ

同时打开所有 MPQ，用编辑器的搜索功能。它会显示文件来自哪个归档。

### 常见错误

| 错误 | 原因 |
|---|---|
| 打开 MPQ 显示一堆 unknown | 缺 listfile |
| 无法打开 MPQ | WoW 正在运行，先关游戏 |
| 改了游戏里没变化 | ①没删 Cache ②exe 没打补丁 ③补丁命名不对 ④路径不对 |
| 提示只读 | MPQ 属性是只读，或文件被占用 |

---

## 七、替代工具

| 工具 | 说明 |
|---|---|
| **MyWarcraftStudio** | 老牌工具，能直接预览模型/贴图，但停更很久 |
| **CASCExplorer** | 只支持新版 CASC 格式，3.3.5 用不上 |

**建议就用 Ladik's**，最稳定，兼容性最好。

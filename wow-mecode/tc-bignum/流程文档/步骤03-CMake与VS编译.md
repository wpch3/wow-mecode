# 第 2 步：CMake 重新生成 + VS 2022 编译

**你的环境**：源码 `D:\TrinityCore`（分支 `bignum-mod`）、build `D:\TC-Build`、VS 2022、CMake GUI。
**预计耗时**：CMake 2 分钟 + 编译 20~60 分钟（看 CPU）。

---

## ⚠️ 最重要的一件事：这次**必须**重跑 CMake

平时只改了 .cpp 内容，可以直接在 VS 里编译，不用碰 CMake。
**但你这次情况特殊，必须重跑 CMake GUI。**

### 为什么

我查了你这套源码的构建脚本，发现两个关键点：

**① 源文件是用 GLOB 自动扫描的**（`cmake/macros/AutoCollect.cmake:26`）：
```cmake
file(GLOB COLLECTED_SOURCES ...)
```
GLOB 的意思是"编译时扫描目录下所有文件"。这个列表**在 CMake 生成时固定下来**，
之后 VS 不会重新扫描。

**② 你的 Eluna 子模块之前是空的**

还记得第 1 步开始时吗：
```
-4608e3f094110089488ee8ddbd87b17de3cc449b src/server/game/LuaEngine
↑ 减号 = 目录是空的
```

也就是说，**你上次生成 `D:\TC-Build` 时，LuaEngine 目录里一个文件都没有**。
GLOB 扫描到的是空目录 → 生成的 VS 工程里**根本没有 Eluna 的任何源文件**。

现在子模块下载好了（`LuaEngine.cpp` 等几百个文件），但 VS 工程还是旧的，
**不重跑 CMake 的话，Eluna 压根不会被编译进去**，你的 uint64 修复也就白改了。

> 顺带说明：`src/server/game/CMakeLists.txt:128` 有 `add_subdirectory(LuaEngine)`，
> 这句在目录为空时会直接失败或跳过。所以重跑 CMake 是必须的。

---

## 2-1. 重跑 CMake GUI

### ① 打开 CMake GUI

确认上方两个路径（应该是你上次填的，一般会自动记住）：

```
Where is the source code:      D:/TrinityCore
Where to build the binaries:   D:/TC-Build
```

> ⚠️ 注意 CMake GUI 里用的是**正斜杠 `/`**，不是反斜杠。

### ② 先点 `Configure`

会弹窗问生成器，选：

```
Generator:  Visual Studio 17 2022
Platform:   x64                    ← 必须是 x64，不能是 Win32
```

然后点 Finish，等它跑完（1~2 分钟）。

> 如果没弹窗直接开始跑，说明它复用了上次的配置，也正常。

### ③ 检查关键输出

Configure 完成后，在下方日志里找这几行：

**必须看到 Eluna 相关：**
```
Lua version: lua52
```
这行来自 `dep/lualib/CMakeLists.txt:3`，看到它说明 Eluna 的 Lua 库被正确纳入了。

**中间的红色高亮不一定是错误** —— CMake GUI 会把新增的变量标红，正常现象。
只要最下面显示 `Configuring done` 就是成功。

### ④ 检查选项（在中间的变量列表里）

确认这几个（可以在上方搜索框输入关键字过滤）：

| 变量 | 应为 | 说明 |
|---|---|---|
| `ELUNA` | ✅ 勾选 | Eluna 引擎，默认就是开的 |
| `SERVERS` | ✅ 勾选 | 编译 worldserver/authserver |
| `SCRIPTS` | `static` | 默认值 |
| `TOOLS` | ⬜ **建议取消勾选** | 地图提取工具，你已经有提取好的地图了，不编能省 5~10 分钟 |
| `USE_COREPCH` | ✅ 勾选 | 预编译头，能大幅加快编译 |
| `USE_SCRIPTPCH` | ✅ 勾选 | 同上 |
| `LUA_VERSION` | `lua52` | 默认值，别动 |

> `TOOLS` 取消勾选是可选优化。如果你不确定，保持原样也行。

### ⑤ 点 `Generate`

看到 `Generating done` 就成功了。

---

## 2-2. 用 VS 2022 编译

### ① 打开解决方案

```
D:\TC-Build\TrinityCore.sln
```

双击打开（首次加载可能要 1~2 分钟）。

### ② 选择编译配置

VS 顶部工具栏的两个下拉框，设为：

```
RelWithDebInfo    ▼        x64    ▼
```

> **为什么用 `RelWithDebInfo` 而不是 `Release`**：
> 它是"优化 + 保留调试符号"，性能和 Release 几乎一样，但崩溃时能看到
> 出错的函数名和行号。开服排查问题会方便非常多。
>
> **千万别选 `Debug`** —— 那个版本运行起来会慢 5~10 倍。

### ③ 开始编译

菜单栏：**生成(B) → 生成解决方案(B)**，或按 `Ctrl + Shift + B`。

然后等。20~60 分钟，取决于你的 CPU 核心数。

> 💡 加速技巧：**工具 → 选项 → 项目和解决方案 → 生成并运行**，
> 把"最大并行项目生成数"设为你的 CPU 核心数。

### ④ 编译成功的标志

底部"输出"窗口最后一行类似：

```
========== 生成: 成功 24 个，失败 0 个，最新 0 个，跳过 0 个 ==========
```

**关键是"失败 0 个"。** 警告（warning）有几百条都正常，不用管。

---

## 2-3. 验证编译产物

编译完成后，新的 exe 在：

```
D:\TC-Build\bin\RelWithDebInfo\
```

检查这几个文件的**修改时间是不是刚才**：

- `worldserver.exe`
- `authserver.exe`

在 Git Bash 里快速验证：

```bash
ls -lh /d/TC-Build/bin/RelWithDebInfo/*.exe
```

**还要确认 Eluna 真的编进去了**：

```bash
ls /d/TC-Build/bin/RelWithDebInfo/lua_scripts/
```

应该能看到 `extensions` 文件夹（这是 `game/CMakeLists.txt:111` 的
POST_BUILD 步骤自动复制的）。**如果这个文件夹不存在，说明 Eluna 没编译进去**，
回到 2-1 重跑 CMake。

---

## 2-4. ⚠️ 先别急着替换服务端

**这一步先停下，不要用新 exe 覆盖你正在运行的服务端。**

因为数据库还没改（第 3 步才做）。现在的状态是：
- ✅ 新 exe：能读大数值
- ⬜ 数据库：还是 smallint 小字段

这个组合**可以正常运行**（新 exe 读旧数据库完全没问题），
但要等第 3 步执行完 SQL，大数值才真正可用。

**建议**：先把新 exe 复制到一个临时目录测试，别动线上服务端。

---

## ✅ 完成检查清单

- [ ] CMake `Configure` 成功，看到 `Lua version: lua52`
- [ ] CMake `Generate` 成功
- [ ] VS 配置为 `RelWithDebInfo` + `x64`
- [ ] 编译结果"失败 0 个"
- [ ] `D:\TC-Build\bin\RelWithDebInfo\worldserver.exe` 时间是刚才
- [ ] `bin\RelWithDebInfo\lua_scripts\extensions\` 文件夹存在

---

## 🔧 常见报错处理

**`error C2065: 未声明的标识符`（在 LuaEngine 相关文件里）**
→ 子模块文件不全。执行 `git submodule update --init --recursive` 后重跑 CMake。

**`fatal error C1083: 无法打开包括文件: "lua.h"`**
→ CMake 没正确配置 Eluna。检查 `ELUNA` 选项是否勾选，重跑 Configure + Generate。

**`LNK1104: 无法打开文件 "worldserver.exe"`**
→ 服务端正在运行，占用了文件。先关掉 worldserver.exe 再编译。

**`error MSB8020: 无法找到 v143 生成工具`**
→ VS 2022 的 C++ 组件没装全。打开 Visual Studio Installer →
   修改 → 勾选"使用 C++ 的桌面开发"。

**编译到一半卡住不动**
→ 内存不够（TrinityCore 编译很吃内存）。把并行数调低到 2~4 再试。

---

## 🔙 回滚

编译产物有问题不影响源码，直接重编即可。
如果想彻底重来：

```
删除 D:\TC-Build 整个目录 → 重新跑 CMake GUI → 重新编译
```

（会慢一些，但能解决绝大部分诡异的编译问题）

---

## 完成后告诉我

回复「第 2 步完成」+ VS 最后那行「生成: 成功 X 个，失败 0 个」。

如果有编译错误，**把第一条 error 的完整内容发我**（不是最后一条，
第一条才是根因，后面的往往是连锁反应）。

下一步（第 3 步）就是执行 SQL 扩列，那一步做完大数值就真正生效了。

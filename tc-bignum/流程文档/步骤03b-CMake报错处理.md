# CMake 报错处理：生成器冲突 + lua52 失败

**结论：你猜对了，是 `D:\TC-Build` 没清理。但先别急着删——错误信息暴露了一个更重要的问题。**

---

## 一、⚠️ 关键发现：你上次编译用的不是 VS 2022

看这行报错：

```
Does not match the generator used previously: Visual Studio 18 2026
                                              ^^^^^^^^^^^^^^^^^^^^^
```

`D:\TC-Build` 里缓存的生成器是 **Visual Studio 18 2026**，不是 VS 2022。

这说明**你现在能正常跑的那个服务端，是用 VS 2026 编译出来的**，不是你以为的 VS 2022。

### 这是什么东西

- **Visual Studio 18 2026** = VS 2026，目前还是 **Insiders 预览版**（未正式发布）
- CMake 从 4.2 才加入这个生成器，且官方标注为 **experimental（实验性）**
- 你装的 CMake 是 4.4（从报错路径 `cmake-4.4` 可以看出），所以支持它

### 为什么会这样

CMake GUI 首次配置时如果不手动选，会**自动挑一个它找到的最新 VS**。
你机器上大概率同时装了 VS 2026 Insiders 和 VS 2022，CMake 自动选了更新的那个。

---

## 二、先确认你到底装了哪些 VS

**在 Git Bash 里执行**（vswhere 是 VS 官方自带的检测工具）：

```bash
"/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" -all -property displayName
```

或者用 PowerShell：

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -all -property displayName
```

会列出你装的所有 VS，比如：

```
Visual Studio Community 2022
Visual Studio Community 2026 Insiders
```

**把这个输出发我**，或者自己对照下面选一条路走。

---

## 三、两条路，选一条

### 🅰️ 路线 A：继续用 VS 2026（推荐，如果你之前一直用它）

**优点**：和你现有的工作环境一致，风险最低。
**做法**：CMake GUI 里生成器选 **`Visual Studio 18 2026`**，而不是 2022。

这样连 build 目录都**不用删**——生成器匹配上了就不会报第一个错。

> 但第二个 lua52 错误可能还在，见第四节。

---

### 🅱️ 路线 B：改用 VS 2022（更稳，但要全部重编）

**优点**：VS 2022 是正式版，TrinityCore 官方支持，不会踩预览版的坑。
**缺点**：必须删掉整个 build 目录，全量重编（多花 20~40 分钟）。

**做法**：

```bash
# 1. 彻底删除旧 build（Git Bash）
rm -rf /d/TC-Build

# 2. 重建空目录
mkdir -p /d/TC-Build
```

或者在文件资源管理器里直接删 `D:\TC-Build` 文件夹再新建。

然后 CMake GUI：
- Where is the source code: `D:/TrinityCore`
- Where to build the binaries: `D:/TC-Build`
- 点 Configure → 选 **`Visual Studio 17 2022`** + **`x64`**

> ⚠️ 只删 `CMakeCache.txt` 和 `CMakeFiles` 文件夹**不够**，
> 因为 `_deps/lua52-subbuild/` 里还有一份独立的缓存（就是第二个报错的来源）。
> **整个目录删掉最干净。**

---

## 四、第二个报错：lua52 为什么失败

```
CMake step for lua52 failed: 1
dep/lualib/lua/CMakeLists.txt:55 (FetchContent_MakeAvailable)
```

### 根因（大概率）

它和第一个错误是**同一个原因**。我查了源码：

```cmake
# dep/lualib/lua/CMakeLists.txt:41-44
FetchContent_Declare(
  lua52
  URL      https://www.lua.org/ftp/lua-5.2.4.tar.gz
  URL_HASH SHA256=b9e2e4aad6789b3b63a056d442f7b39f0ecfca3ae0f1fc0ae4e9614401b69f4b
)
```

FetchContent 会在 `D:\TC-Build\_deps\lua52-subbuild\` 建一个**独立的子工程**，
并用**和主工程相同的生成器**去配置它。主工程生成器冲突 → 子工程也跟着炸。

**所以只要解决了生成器问题（删目录或选对生成器），这个错大概率自动消失。**

### 但还有一个隐患：这个库要联网下载

注意那个 URL —— **lua 源码是编译时从 lua.org 下载的**，不在你的仓库里。

国内访问 `lua.org` 可能很慢或超时。如果修好生成器后仍然报 lua52 失败，
先看看是不是卡在下载。**解决办法**：

**方案 1：手动下载后指定路径**

1. 用浏览器/迅雷下载：`https://www.lua.org/ftp/lua-5.2.4.tar.gz`
2. 解压到比如 `D:\lua-5.2.4`（解压后里面应有 `src` 文件夹）
3. CMake GUI 里点 `Add Entry`，添加：
   - Name: `FETCHCONTENT_SOURCE_DIR_LUA52`
   - Type: `PATH`
   - Value: `D:/lua-5.2.4`
4. 重新 Configure

这样 CMake 就直接用本地目录，不再联网。

**方案 2：挂代理后重试**（如果你有）

---

## 五、我的建议

**如果你之前编译一直没问题** → 走 **路线 A**（继续用 VS 2026），改动最小。

**如果你想要更稳定的长期环境** → 走 **路线 B**（换 VS 2022 + 删目录）。
TrinityCore 官方文档推荐 VS 2019/2022，用预览版将来可能遇到奇怪的编译错误。

我个人倾向 **路线 B**，理由：
- VS 2026 还是 Insiders 预览版，CMake 对它的支持也标注为 experimental
- 你后面还要加 6 个新指令文件、改 Player.cpp，编译次数会很多
- 现在花 30 分钟换到稳定环境，比后面反复踩坑划算

但**如果你 VS 2022 没装全 C++ 组件**，那就先走路线 A，别折腾。

---

## 六、无论走哪条路，都要注意

删除 `D:\TC-Build` **不会影响**：
- ✅ 你的源码 `D:\TrinityCore`（第 1 步的改动都在 git 里，安全）
- ✅ 你正在运行的服务端（那是另一个目录的 exe）
- ✅ 你的数据库

只是要重新编译一次而已。

---

## 完成后告诉我

请回复：

1. **vswhere 的输出**（你装了哪些 VS）
2. **你选择路线 A 还是 B**
3. Configure 是否成功（看到 `Configuring done`）

如果还有新报错，把**完整的红字**发我。

# 删除 build 后：用 VS 2022 重建

**你已经删掉 `D:\TC-Build`，这没问题。** 依赖路径 CMake 会自动重新探测，
只有 Boost 大概率需要你手动指定一次。下面按顺序做。

---

## ⚠️ 先做一件事：确认 VS 2022 装了 C++ 组件

这是路线 B 唯一的前置风险。**先确认，别等编译到一半才发现。**

Git Bash 里执行：

```bash
"/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" -version "[17.0,18.0)" -property installationPath
"/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName
```

**期望**：
- 第一条输出 VS 2022 的安装路径，如 `C:\Program Files\Microsoft Visual Studio\2022\Community`
- 第二条输出 `Visual Studio Community 2022`

**如果第二条是空的** → C++ 工作负载没装。打开
**Visual Studio Installer → VS 2022 → 修改 → 勾选「使用 C++ 的桌面开发」→ 修改**。
装完再继续。

---

## 1. 重建 build 目录

```bash
mkdir -p /d/TC-Build
```

确认是空的：

```bash
ls -A /d/TC-Build
```
应该没有任何输出。

---

## 2. CMake GUI 配置

打开 CMake GUI：

| 字段 | 填写 |
|---|---|
| Where is the source code | `D:/TrinityCore` |
| Where to build the binaries | `D:/TC-Build` |

> 注意用**正斜杠** `/`。

点 **Configure**，弹窗里选：

```
Generator:                        Visual Studio 17 2022
Optional platform for generator:  x64          ← 必须选，默认可能是 Win32
Use default native compilers      ← 保持这个选项
```

点 **Finish**。

---

## 3. 大概率会遇到的 Boost 报错

我查了 `dep/boost/CMakeLists.txt`，Windows 下要求 **Boost ≥ 1.78**，
且它是这样找 Boost 的：

```cmake
if(DEFINED ENV{BOOST_ROOT})
  set(BOOST_ROOT $ENV{BOOST_ROOT})
endif()
...
message(FATAL_ERROR "No BOOST_ROOT environment variable could be found!
                     Please make sure it is set and the points to your Boost installation.")
```

**也就是说 Boost 靠环境变量 `BOOST_ROOT` 定位。**

### 情况 A：Configure 顺利通过

说明你系统里 `BOOST_ROOT` 环境变量还在（之前装 Boost 时设的），
**跳过这一节，直接看第 4 步。**

### 情况 B：报 `No BOOST_ROOT environment variable could be found`

先找到你的 Boost 装在哪：

```bash
ls -d /c/local/boost* 2>/dev/null
ls -d /d/boost* 2>/dev/null
ls -d /c/boost* 2>/dev/null
echo "现有环境变量: $BOOST_ROOT"
```

找到后（假设是 `C:\local\boost_1_85_0`），**在 CMake GUI 里加一条**：

1. 点 **Add Entry**
2. 填写：
   - Name: `BOOST_ROOT`
   - Type: `PATH`
   - Value: `C:/local/boost_1_85_0`   ← 正斜杠
3. 点 OK，再点 **Configure**

> 也可以设成系统环境变量（永久生效，推荐）：
> Win+R → `sysdm.cpl` → 高级 → 环境变量 → 新建系统变量
> `BOOST_ROOT` = `C:\local\boost_1_85_0`
> **设完要重启 CMake GUI 才能读到。**

---

## 4. 其他依赖：应该会自动找到

| 依赖 | 如何定位 | 要手动填吗 |
|---|---|---|
| **MySQL** | 读注册表 `MySQL Server *` / `MariaDB *`，自动 | 基本不用 |
| **OpenSSL** | `find_package(OpenSSL 1.1 REQUIRED)`，标准路径 | 基本不用 |
| **Boost** | 靠 `BOOST_ROOT` 环境变量 | **大概率要** |

如果 MySQL 或 OpenSSL 报错找不到，告诉我报错原文，我给对应的变量名。

---

## 5. Configure 成功的标志

日志最后出现：

```
Configuring done
```

并且中途能看到这几行关键信息：

```
Lua version: lua52              ← Eluna 的 Lua 库被纳入了
Found MySQL library: ...
Found OpenSSL library: ...
* Build with Eluna LuaEngine   : Yes      ← 确认 Eluna 开启
```

> 中间变量列表变红是正常的（表示新增变量），不是错误。

---

## 6. 检查选项后 Generate

在变量列表里确认（可用上方搜索框过滤）：

| 变量 | 应为 |
|---|---|
| `ELUNA` | ✅ 勾选 |
| `SERVERS` | ✅ 勾选 |
| `SCRIPTS` | `static` |
| `TOOLS` | ⬜ 建议取消（省 5~10 分钟，你已有地图文件） |
| `USE_COREPCH` | ✅ 勾选 |
| `USE_SCRIPTPCH` | ✅ 勾选 |
| `CMAKE_BUILD_TYPE` | 留空即可（VS 是多配置生成器，在 VS 里选） |

改动后再点一次 **Configure**，然后点 **Generate**。

看到 `Generating done` 即成功。

---

## 7. 关于 lua52 下载

删掉 build 后，`_deps` 缓存也没了，**CMake 会重新下载 lua-5.2.4.tar.gz**。

- 顺利的话几秒钟就好
- 如果卡住/超时（国内访问 lua.org 可能慢），按下面处理

**手动下载方案**：

1. 下载 `https://www.lua.org/ftp/lua-5.2.4.tar.gz`
2. 解压到 `D:\lua-5.2.4`（里面应有 `src` 文件夹）
3. CMake GUI → Add Entry：
   - Name: `FETCHCONTENT_SOURCE_DIR_LUA52`
   - Type: `PATH`
   - Value: `D:/lua-5.2.4`
4. 重新 Configure

---

## 8. 编译

打开 `D:\TC-Build\TrinityCore.sln`

顶部配置选：**`RelWithDebInfo`** + **`x64`**

生成 → 生成解决方案（`Ctrl+Shift+B`）

> 这次是全量编译，比平时慢，20~60 分钟正常。

---

## 9. 编译后验证

```bash
ls -lh /d/TC-Build/bin/RelWithDebInfo/*.exe
ls /d/TC-Build/bin/RelWithDebInfo/lua_scripts/
```

- `worldserver.exe`、`authserver.exe` 时间是刚才
- `lua_scripts/` 下有 `extensions` 文件夹 ← **Eluna 编进去的证明**

---

## ✅ 完成清单

- [ ] VS 2022 确认装了 C++ 组件
- [ ] `D:\TC-Build` 已重建为空目录
- [ ] Generator = `Visual Studio 17 2022` + `x64`
- [ ] Configure 通过，看到 `Lua version: lua52`
- [ ] Generate 通过
- [ ] VS 用 `RelWithDebInfo` + `x64` 编译
- [ ] 「失败 0 个」
- [ ] `lua_scripts/extensions` 存在

---

## 遇到报错怎么发我

**把第一条 error 的完整内容发我**，包括它上面 3~5 行的上下文。
第一条才是根因，后面的都是连锁反应。

CMake 的报错可以直接在 GUI 下方日志框里选中复制。

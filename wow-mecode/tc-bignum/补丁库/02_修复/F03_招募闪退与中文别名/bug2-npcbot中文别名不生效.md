# Bug 2：`.npcbot 信息` 不能用，但 `.pbot 过来` 可以

> 用户反馈：「执行了sql更改了botcommands.cpp，还有重新拷贝了playerbot.cpp
> 都没有中文别名，还是只能用 npc info 不能用 npc 信息等，
> 但playerbot没有问题可以用中文别名」
> 2026-08-04

---

## 一、这条线索本身就是答案

**`.pbot` 中文能用 + `.npcbot` 中文不能用** —— 两个文件差在哪？

```
cs_playerbot.cpp    我给你的【新文件】      -> 你直接保存，是 UTF-8
botcommands.cpp     上游【原有文件】        -> 你在 VS2022 里打开后加中文
```

我实查了上游文件的编码：

```
botcommands.cpp: C++ source, ASCII text
非ASCII字节数: 0
无BOM
```

**原文件是纯 ASCII、没有 BOM。**

VS2022 打开一个纯 ASCII 文件、你加了中文再保存 ->
**默认存成 GBK（简体中文系统的 ANSI 代码页）**，不是 UTF-8。

---

## 二、为什么 GBK 会导致命令匹配失败

同样是「信息」两个字：

```
客户端聊天框发来的（UTF-8）: E4 BF A1 E6 81 AF
GBK 编码源文件里的         : D0 C5 CF A2
```

**字节序列完全不同，永远匹配不上。**

我写了对照实验验证（`compile_test/enctest.cpp`）：

```
A. 源码UTF-8 + 客户端UTF-8 : 匹配成功
B. 源码GBK   + 客户端UTF-8 : 匹配失败   <- 你的情况
C1. .pbot 源码GBK  比较: 不等
C2. .pbot 源码UTF8 比较: 相等           <- .pbot 能用，说明它是UTF-8
```

**C2 这一行反过来证明了 `cs_playerbot.cpp` 是 UTF-8**，
所以它的中文别名正常工作。

---

## 三、为什么执行 SQL 没用

你提到「执行了 sql」。那个 SQL（step41 的汉化）改的是
`world.npc_text` / `npc_text_locale` —— **那是对话框里的文字**，
和**命令名**是两回事。

命令名注册在代码里（`botcommands.cpp` 的 `npcbotCommandTable`），
数据库的 `command` 表只是**给命令加帮助文本**，不能新增命令：

```cpp
// ChatCommand.cpp:93-105  读 command 表时
auto it = map->find(key);
if (it != map->end()) { ... }
else
{
    TC_LOG_ERROR("sql.sql", "Table `command` contains data for
                  non-existant command '{}'. Skipped.", name);
    // ^^^ 命令表里没有的名字，直接跳过
}
```

**所以汉化 SQL 和命令别名互不相干，两件事都要做。**

---

## 四、修复：把 botcommands.cpp 存成【带 BOM 的 UTF-8】

### 4.1 为什么必须带 BOM

项目没有开 `/utf-8` 编译选项（我查过 CMakeLists.txt 和 cmake/ 目录，没有）。

没有 BOM 时，MSVC 会**按系统代码页**（简中系统 = GBK）解析源文件。
即使你手动存成 UTF-8，MSVC 也会当成 GBK 读 -> 中文字面量还是错的。

**带 BOM 的 UTF-8 是 MSVC 唯一能可靠识别的方式。**

GCC/Clang 也接受 BOM（我实测过），所以不影响跨平台。

### 4.2 VS2022 操作步骤

1. 打开 `botcommands.cpp`
2. 菜单 **文件** -> **另存为**
3. 点保存按钮**右边的小箭头** -> **编码保存**
4. 弹窗问是否覆盖 -> **是**
5. 编码列表里选：

```
Unicode (UTF-8 带签名) - 代码页 65001
                ^^^^^^ 必须是"带签名"，那个"签名"就是 BOM
```

6. 保存

### 4.3 确认改对了

保存后，VS2022 右下角状态栏应该显示 `UTF-8 with BOM`（或"UTF-8 带签名"）。

也可以用 Git Bash 验证：

```bash
head -c 3 src/server/game/AI/NpcBots/botcommands.cpp | od -An -tx1
```

输出 `ef bb bf` 就对了。

### 4.4 重新编译

**只改了文件编码，内容没变，不用重跑 CMake**，直接重新生成 `game` 项目即可。

---

## 五、同样的坑会影响哪些文件

只要是**你在 VS2022 里往上游原有文件加中文**，都会中招。

已知需要检查的：

```
[ ] botcommands.cpp          <- 本次的中文别名（必须改）
[ ] bot_ai.cpp               <- 如果你在里面加过中文（step37/38/39 我给的是纯代码，
                                 但注释里有中文，注释错了不影响功能）
[ ] Unit.cpp                 <- step39 的一行修复，注释含中文
```

**判断标准**：
- 中文只在**注释**里 -> 编码错了也不影响功能（顶多注释显示乱码）
- 中文在**字符串字面量**里（会被玩家看到 / 参与比较）-> **必须 UTF-8 BOM**

我给的**新文件**（`cs_playerbot.cpp` / `pbot_autoaccept.*` / `cs_botrename.cpp`）
都是 UTF-8，只要你没在 VS 里"另存为 ANSI"就没问题。

---

## 六、一个更稳妥的长期方案（可选）

给项目加上 `/utf-8` 编译选项，之后所有文件都按 UTF-8 解析，
不再依赖 BOM。

**改 `CMakeLists.txt`**，在合适位置加：

```cmake
if(MSVC)
    add_compile_options(/utf-8)
endif()
```

**好处**：以后加中文再也不用管 BOM
**代价**：要重跑 CMake + 全量重编译（时间较长）

**我的建议**：这次先用 BOM 解决（5 分钟），
等你哪天有空全量重编译时再加 `/utf-8`。

---

## 七、验证清单

```
[ ] botcommands.cpp 另存为 UTF-8 带签名
[ ] head -c 3 确认输出 ef bb bf
[ ] 重新编译 game 项目（不用重跑CMake）
[ ] .npcbot info          英文仍然能用（没被破坏）
[ ] .npcbot 信息          【关键】现在应该能用了
[ ] .npcbot 招募          选中一个bot能招募
[ ] .npcbot 列表 spawned  子菜单中文入口也能用
```

**如果改完还是不行**，可能是别名段没插对位置，
把 `botcommands.cpp` 第 653 行附近截图发我。

---

## 八、核实记录

```
【命令查找链路】
ChatCommand.cpp:33    ChatSubCommandMap = std::map<string_view, Node, StringCompareLessI_T>
ChatCommand.cpp:207   FilteredCommandListIterator
ChatCommand.cpp:211   _it{ map.lower_bound(token) }        <- 前缀匹配起点
ChatCommand.cpp:229   StringStartsWithI(_it->first, _token) <- 字节级比较
Util.cpp:706          StringEqualI   -> std::tolower(char)
Util.cpp:717          StringCompareLessI -> std::tolower(char)
Util.h:364            StringStartsWithI

【两种命令表的区别】
botcommands.cpp:487   ChatCommandTable GetCommands()        <- 新式，走上面的map
cs_playerbot.cpp:124  std::vector<ChatCommand> GetCommands() <- 旧式
cs_playerbot.cpp:326  tok[0] == "上线"                       <- 中文是【函数内比较】
                                                              不经过命令表
                      ^^^ 这就是 .pbot 中文能用的原因：
                          它的中文根本不是命令名，是参数

【command 表的作用】
ChatCommand.cpp:93-105  只能给【已存在的】命令加帮助文本，
                        不能新增命令。找不到就 TC_LOG_ERROR 跳过。

【编码】
botcommands.cpp  原文件: ASCII text, 无BOM, 非ASCII字节数=0
CMakeLists.txt   没有 /utf-8 选项 -> MSVC 按系统代码页解析
实测: GCC 接受 UTF-8 BOM 并正确输出中文
```

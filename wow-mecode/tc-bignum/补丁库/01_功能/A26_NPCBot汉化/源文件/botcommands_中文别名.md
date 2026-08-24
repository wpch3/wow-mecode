# NPCBot 指令中文别名 —— 改动清单

> 用户决定：「指令就不用中文吧，听你的，但是可以加中文别名」
> 改上游：`botcommands.cpp` 一处

---

## 一、原理

TrinityCore 的指令表允许**同一个处理函数注册多个名字**。
上游自己就在这么用：

```cpp
// botcommands.cpp:672-673   unhide 和 show 指向同一个函数
{ "unhide",  HandleNpcBotUnhideCommand, ... },
{ "show",    HandleNpcBotUnhideCommand, ... },

// botcommands.cpp:675-676   kill 和 suicide 也是
{ "kill",    HandleNpcBotKillCommand, ... },
{ "suicide", HandleNpcBotKillCommand, ... },
```

**所以加中文别名 = 复制一行、换个名字。零风险。**

---

## 二、改法

**Ctrl+F 搜**（`botcommands.cpp:653`）：

```cpp
        static ChatCommandTable npcbotCommandTable =
        {
```

**在这个 `{` 下面，插入下面整段**（放在 `//{ "debug",` 那两行注释之前或之后都行）：

```cpp
            // ================= 中文别名（step41）=================
            // 原理同上游自己的 unhide/show、kill/suicide 双注册。
            // 英文指令【全部保留】，中文只是多一个入口。
            { "招募",       HandleNpcBotAddCommand,                 rbac::RBAC_PERM_COMMAND_NPCBOT_ADD,                Console::No  },
            { "解雇",       HandleNpcBotRemoveCommand,              rbac::RBAC_PERM_COMMAND_NPCBOT_REMOVE,             Console::No  },
            { "释放",       HandleNpcBotFreeCommand,                rbac::RBAC_PERM_COMMAND_NPCBOT_REMOVE,             Console::No  },
            { "生成",       HandleNpcBotSpawnCommand,               rbac::RBAC_PERM_COMMAND_NPCBOT_SPAWN,              Console::No  },
            { "移动",       HandleNpcBotMoveCommand,                rbac::RBAC_PERM_COMMAND_NPCBOT_MOVE,               Console::No  },
            { "查找",       HandleNpcBotLookupCommand,              rbac::RBAC_PERM_COMMAND_NPCBOT_LOOKUP,             Console::Yes },
            { "复活",       HandleNpcBotReviveCommand,              rbac::RBAC_PERM_COMMAND_NPCBOT_REVIVE,             Console::No  },
            { "信息",       HandleNpcBotInfoCommand,                rbac::RBAC_PERM_COMMAND_NPCBOT_INFO,               Console::Yes },
            { "隐藏",       HandleNpcBotHideCommand,                rbac::RBAC_PERM_COMMAND_NPCBOT_HIDE,               Console::No  },
            { "显示",       HandleNpcBotUnhideCommand,              rbac::RBAC_PERM_COMMAND_NPCBOT_UNHIDE,             Console::No  },
            { "杀死",       HandleNpcBotKillCommand,                rbac::RBAC_PERM_COMMAND_NPCBOT_KILL,               Console::No  },
            { "修复",       HandleNpcBotFixCommand,                 rbac::RBAC_PERM_COMMAND_NPCBOT_REVIVE,             Console::No  },
            { "传送",       HandleNpcBotGoCommand,                  rbac::RBAC_PERM_COMMAND_NPCBOT_MOVE,               Console::No  },
            { "装等",       HandleNpcBotGearScoreCommand,           rbac::RBAC_PERM_COMMAND_NPCBOT_COMMAND_MISC,       Console::No  },
            { "召回",       npcbotRecallCommandTable                                                                                },
            { "列表",       npcbotListCommandTable                                                                                  },
            { "删除",       npcbotDeleteCommandTable                                                                                },
            { "设置",       npcbotSetCommandTable                                                                                   },
            { "命令",       npcbotCommandCommandTable                                                                               },
            { "距离",       npcbotDistanceCommandTable                                                                              },
            { "指令",       npcbotOrderCommandTable                                                                                 },
            // ================= 中文别名结束 =================
```

---

## 三、用法对照表

| 中文 | 等同于 | 作用 |
|---|---|---|
| `.npcbot 招募` | `.npcbot add` | 招募选中的bot |
| `.npcbot 解雇` | `.npcbot remove` | 解雇 |
| `.npcbot 释放` | `.npcbot free` | 释放为无主 |
| `.npcbot 生成` | `.npcbot spawn` | 生成bot |
| `.npcbot 移动` | `.npcbot move` | 移动 |
| `.npcbot 查找 <名>` | `.npcbot lookup` | 查找bot |
| `.npcbot 复活` | `.npcbot revive` | 复活 |
| `.npcbot 信息` | `.npcbot info` | 查看信息 |
| `.npcbot 隐藏` / `显示` | `hide` / `unhide` | 隐藏显示 |
| `.npcbot 杀死` | `.npcbot kill` | 杀死 |
| `.npcbot 修复` | `.npcbot fix` | 修复异常状态 |
| `.npcbot 传送` | `.npcbot go` | 传送 |
| `.npcbot 装等` | `.npcbot gs` | 查看装备等级 |
| `.npcbot 召回` | `.npcbot recall` | 召回子菜单 |
| `.npcbot 列表 ...` | `.npcbot list ...` | 列表子菜单 |
| `.npcbot 设置 ...` | `.npcbot set ...` | 设置子菜单 |

**英文指令全部保留，两种都能用。**

---

## 四、注意事项

### 中文指令要切输入法

这也是我建议**指令名保持英文、只加别名**的原因。
真正高频的操作（比如 `.npcbot add`）你打英文更快。

**中文别名的价值在于：忘了英文怎么拼的时候有个退路。**

### 编译

只改 `botcommands.cpp` 内容，**不用重跑 CMake**。

---

## 五、验证

```
[ ] .npcbot 信息        应该和 .npcbot info 一样
[ ] .npcbot 招募        选中一个bot后能招募
[ ] .npcbot 列表 spawned   子菜单也能用中文入口
[ ] .npcbot info        英文的仍然能用（没被破坏）
```

---

## 六、如果想加更多别名

照上面的格式复制一行，改名字即可。
**注意**：
- 名字不能和已有的重复
- 子菜单（`npcbotXXXCommandTable`）那种没有 rbac 和 Console 参数
- 改完重新编译 `game` 项目

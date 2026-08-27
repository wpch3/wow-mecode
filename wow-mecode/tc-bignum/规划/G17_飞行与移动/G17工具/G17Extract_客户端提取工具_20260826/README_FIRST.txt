G17Extract 客户端提取工具
==========================

从 WoW 3.3.5a 客户端 MPQ 里提取 Interface/FrameXML 源码（.lua + .xml 文件），
用于研究载具动作条、玩家动作条和 UI 切换机制。

用途：
  1. 提取 VehicleMenuBar.lua/xml —— 理解载具动作条的工作原理
  2. 提取 ActionBar 相关文件 —— 理解为什么上坐骑后玩家动作条被隐藏
  3. 为"用角色UI做御龙术技能"提供源码依据

使用方法：
  python g17_extract.py --client-root "D:\WOW" --output "D:\G17_extracted"

  --list-only  只列出文件不提取
  --filter Interface  过滤条件（默认提取 Interface 目录下的文件）

输出：
  D:\G17_extracted\Interface\FrameXML\ 目录下的 .lua 和 .xml 文件

重点研究的文件：
  VehicleMenuBar.lua   载具动作条逻辑（按钮数、切换、冷却）
  VehicleMenuBar.xml   载具动作条布局
  ActionBarController.lua  动作条控制器（何时显示/隐藏哪个条）
  MainMenuBar.lua      玩家主动作条
  SecureTemplates.lua  安全按钮模板（施法按钮的底层机制）
  UIParent.lua         UI 父框架（全局显示/隐藏逻辑）

提取后可以用文本编辑器打开这些文件搜索：
  - "VehicleMenuBar" 了解载具条怎么工作
  - "HasVehicleUI" 了解为什么玩家条被隐藏
  - "MainMenuBar" 了解怎么让玩家条保持显示

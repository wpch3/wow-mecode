G17ChatLog 聊天记录+导出工具
============================

解决两个痛点：
  1. 3.3.5 聊天框不能复制文字 → 导出窗口 Ctrl+A 全选 Ctrl+C 复制
  2. 聊天记录被顶掉 → 上限提升到 5000 行 + 独立无限缓冲区(10000条)

安装：
  1. 把 G17ChatLog 文件夹复制到 D:\WOW\Interface\AddOns\G17ChatLog\
     （确保里面有 G17ChatLog.toc 和 G17ChatLog.lua 两个文件）
  2. 重启 WoW 客户端，登录界面勾选 G17 ChatLog

使用：
  /g17log save       打开导出窗口（全部记录）
  /g17log last 100   只看最近100行
  /g17log clear      清空记录
  /g17log help       帮助

导出窗口操作：
  1. 窗口里有一个大文本框，显示所有记录
  2. 点击文本框获得焦点
  3. Ctrl+A 全选
  4. Ctrl+C 复制
  5. 粘贴到任何地方（论坛/聊天/文档）

配合 G17Diag 使用：
  1. 骑上坐骑
  2. /g17diag
  3. /g17log save
  4. Ctrl+A → Ctrl+C → 粘贴回传

也可以直接截图导出窗口（包含完整记录）。

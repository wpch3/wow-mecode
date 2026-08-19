-- ============================================================
--  step21 世界指令 RBAC 绑定到 3 级 GM 组
--  192 = 3级GM组
--
--  执行完必须【重启 worldserver】
--  ChatCommand.cpp:477 的 IsInvokerVisible 只在启动时读一次
-- ============================================================
REPLACE INTO auth.rbac_linked_permissions (id, linkedId) VALUES (192, 71012)

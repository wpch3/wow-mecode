/* 幻化系统 RBAC 2/2 -- 挂到 3 级 GM 组(192) */
/* 想让普通玩家也能用，把 192 换成 195(玩家组) 再执行一次 */
REPLACE INTO auth.rbac_linked_permissions (id, linkedId) VALUES (192, 71009);

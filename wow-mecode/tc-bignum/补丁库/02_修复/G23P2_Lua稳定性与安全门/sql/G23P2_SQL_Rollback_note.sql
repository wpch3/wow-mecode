-- G23-P2 SQL回滚说明
-- 本批次仅新增向后兼容的custom_daily_reward_claim表，并保留原custom_daily_reward表结构。
-- 脚本回滚后旧版custom_daily_reward.lua仍可继续使用原汇总表。
-- 为防止丢失领取审计和误发奖励，本文件故意不DROP、不DELETE任何数据。
SELECT 'G23P2_SQL_ROLLBACK_DATA_PRESERVED' AS result;

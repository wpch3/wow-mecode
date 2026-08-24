-- G23-P2 只读：查看超过10分钟仍pending的每日奖励claim。
-- 不自动删除：pending可能代表奖励已发但最终确认中断，自动删除会造成重复发放。
SELECT guid, claim_date, token, status, streak, total_days, created_at
FROM characters.custom_daily_reward_claim
WHERE status='pending'
  AND created_at < NOW() - INTERVAL 10 MINUTE
ORDER BY created_at;

-- ############################################################################
--  【！！本文件已废弃，请改用 批量生成bot模板_v2无存储过程.sql ！！】
--
--  原因：本文件用了 存储过程 + DELIMITER，
--        但 DBeaver 默认【不识别 DELIMITER】，会把过程体在第一个分号处切断，
--        报错：SQL 错误 [1064] ... near '' at line 11
--        （第11行正是 DECLARE i INT DEFAULT 0;）
--
--  v2 改用【数字辅助表 + 一次性 INSERT SELECT】，
--  完全不用循环、不用存储过程，任何客户端都能跑。
-- ############################################################################

-- ============================================================================
--  step46  批量生成 NPCBot 模板 —— 突破"服务器 bot 总数"上限
-- ============================================================================
--
--  【为什么需要这个】
--
--  botdatamgr.cpp:1878
--      uint32 maxbots = sBotGen->GetSpareBotsCount();
--      if (maxbots < wandering_bots_desired)
--      {
--          BOT_LOG_FATAL("... Desired amount of wandering bots ({}) cannot be created. Aborting!");
--          ASSERT(false);        <- 【直接崩服】
--      }
--
--  配置里 NpcBot.WanderingBots.Continents.Count = 10000，
--  但库里只有 500 个 bot 模板 -> 服务端启动就崩。
--
--  【模板池怎么算】botdatamgr.cpp:316-327
--      遍历 creature_template_npcbot_extras 里的每个 entry，
--      只有【没有人拥有过】(!_botsData.contains(id)) 的才算"备用"。
--      _botsData 来自 characters_npcbot 表（botdatamgr.cpp:1023）。
--
--  所以：可用模板数 = extras表行数 - characters_npcbot表行数
--
--  【entry 上限已经放开】botcommon.h:39-41
--      BOT_ENTRY_BEGIN        = 70001
--      //BOT_ENTRY_END        = 71000     <- 上游【自己注释掉了】
--      BOT_ENTRY_CREATE_BEGIN = 70800     // 70800+ 留给自动创建
--
--  next_bot_id 是 uint32，理论到 42 亿。
--
--  【本脚本做什么】
--      克隆一个【已存在的】bot 模板 N 份，生成新 entry。
--      克隆比手写字段安全得多 —— 不会漏字段、不会填错 displayid。
--      （我以前吃过"硬编码32个displayid全是编的"的亏，不再手写）
--
--  执行：DBeaver 里 Alt+X 执行全部。全部用完全限定名，不用选库。
-- ============================================================================


-- ############################################################################
--  第 0 步：先看看现在有多少
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)      AS '模板总数',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)               AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)               AS '可用备用数';

--  【重要】"可用备用数" 必须 >= 你配置的
--  NpcBot.WanderingBots.Continents.Count，否则启动崩服。


-- 看看现有模板的 entry 范围和职业分布
SELECT
    `class`                AS '职业ID',
    COUNT(*)               AS '数量',
    MIN(`entry`)           AS '最小entry',
    MAX(`entry`)           AS '最大entry'
FROM `world`.`creature_template_npcbot_extras`
GROUP BY `class`
ORDER BY `class`;

--  职业ID对照（botcommon.h:217-238）：
--    1=战士 2=圣骑士 3=猎人 4=潜行者 5=牧师 6=死亡骑士
--    7=萨满 8=法师 9=术士 11=德鲁伊
--    12+ 是扩展职业（BM/大法师/死亡骑士王/地穴领主/黑暗游侠/
--        恐惧魔王/亡灵法师/海巫/斩杀者/斯芬克斯）


-- ############################################################################
--  第 1 步：设定参数
-- ############################################################################
--
--  改这三个值就行：
--    @src_entry  = 拿哪个现有 bot 当模板（从上面查询里挑一个）
--    @start_id   = 新 entry 从哪开始（建议 71000+ 避开上游保留区）
--    @count      = 生成多少个
-- ############################################################################

SET @src_entry := 70001;      -- 源模板（改成你库里真实存在的）
SET @start_id  := 71000;      -- 新 entry 起点
SET @count     := 500;        -- 生成数量


-- 安全检查 1：源模板必须存在
SELECT
    CASE WHEN EXISTS(SELECT 1 FROM `world`.`creature_template` WHERE `entry` = @src_entry)
         THEN CONCAT('[OK] 源模板 ', @src_entry, ' 存在')
         ELSE CONCAT('[停止] 源模板 ', @src_entry, ' 不存在，换一个再执行')
    END AS '检查1_源模板';

-- 安全检查 2：目标区间不能和现有 entry 冲突
SELECT
    CASE WHEN EXISTS(
            SELECT 1 FROM `world`.`creature_template`
            WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
         THEN CONCAT('[停止] ', @start_id, '-', @start_id + @count - 1, ' 区间已有数据，换个起点')
         ELSE CONCAT('[OK] ', @start_id, '-', @start_id + @count - 1, ' 区间空闲')
    END AS '检查2_区间冲突';

--  【上面两个检查都要是 [OK] 才继续往下执行】


-- ############################################################################
--  第 2 步：生成（用存储过程做循环）
-- ############################################################################

DROP PROCEDURE IF EXISTS `world`.`sp_gen_npcbots`;

DELIMITER $$

CREATE PROCEDURE `world`.`sp_gen_npcbots`(
    IN p_src   INT,
    IN p_start INT,
    IN p_count INT
)
BEGIN
    DECLARE i INT DEFAULT 0;
    DECLARE new_entry INT;

    WHILE i < p_count DO
        SET new_entry = p_start + i;

        -- 1) 克隆 creature_template
        --
        --  【关键：不写列名】
        --  我第一版手写了全部列名，核对真实建表语句后发现是错的 ——
        --  写了 trainer_type / spell1-8 / resistance1-6 / InhabitType
        --  这些【本版本根本不存在】的列，还漏了 StringId。
        --  （这正是坑表里"列名照C++结构体写"那条，又差点犯）
        --
        --  正确做法：先整行复制，再单独改 entry。
        --  这样不管表有多少列、将来加没加列，都不会错。
        CREATE TEMPORARY TABLE IF NOT EXISTS `tmp_ct_clone`
            SELECT * FROM `world`.`creature_template` WHERE `entry` = p_src;

        UPDATE `tmp_ct_clone` SET `entry` = new_entry;

        INSERT INTO `world`.`creature_template`
            SELECT * FROM `tmp_ct_clone`;

        DROP TEMPORARY TABLE `tmp_ct_clone`;

        -- 2) 克隆 creature_template_npcbot_extras（职业和种族）
        INSERT INTO `world`.`creature_template_npcbot_extras` (`entry`, `class`, `race`)
        SELECT new_entry, e.`class`, e.`race`
        FROM `world`.`creature_template_npcbot_extras` e
        WHERE e.`entry` = p_src;

        -- 3) 克隆外观（有就克隆，没有跳过）
        INSERT INTO `world`.`creature_template_npcbot_appearance`
            (`entry`, `gender`, `skin`, `face`, `hair`, `haircolor`, `features`)
        SELECT new_entry, a.`gender`, a.`skin`, a.`face`, a.`hair`, a.`haircolor`, a.`features`
        FROM `world`.`creature_template_npcbot_appearance` a
        WHERE a.`entry` = p_src;

        -- 4) 克隆装备（有就克隆）
        INSERT INTO `world`.`creature_equip_template`
            (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
        SELECT new_entry, q.`ID`, q.`ItemID1`, q.`ItemID2`, q.`ItemID3`, q.`VerifiedBuild`
        FROM `world`.`creature_equip_template` q
        WHERE q.`CreatureID` = p_src;

        SET i = i + 1;
    END WHILE;
END$$

DELIMITER ;


-- ############################################################################
--  第 3 步：执行生成
-- ############################################################################

CALL `world`.`sp_gen_npcbots`(@src_entry, @start_id, @count);


-- ############################################################################
--  第 4 步：验证
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)      AS '模板总数_新',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)               AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)               AS '可用备用数_新';

-- 抽查几个新生成的
SELECT ct.`entry`, ct.`name`, e.`class`, e.`race`
FROM `world`.`creature_template` ct
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` BETWEEN @start_id AND @start_id + 4;


-- ############################################################################
--  第 5 步：配置 & 重启
-- ############################################################################
--
--  worldserver.conf.d\unlimited.conf
--
--    # 这个值【必须小于等于】上面查出来的"可用备用数_新"
--    NpcBot.WanderingBots.Continents.Count = 500
--
--  然后重启 worldserver。
--
--  启动日志里应该看到：
--    >> Set up spawning of 500 wandering bots in XXX ms
--
--  如果看到这个就是模板不够：
--    Only N out of M bots of enabled classes aren't spawned.
--    Desired amount of wandering bots (X) cannot be created. Aborting!
--  -> 回到第0步查"可用备用数"，把 Count 调小或者多生成一些模板。


-- ############################################################################
--  想生成不同职业的怎么办
-- ############################################################################
--
--  改 @src_entry 换一个不同职业的源模板，再跑一次，起点错开即可：
--
--    SET @src_entry := 70020;    -- 换个法师模板
--    SET @start_id  := 71500;    -- 起点错开，别和上一批重叠
--    SET @count     := 500;
--    CALL `world`.`sp_gen_npcbots`(@src_entry, @start_id, @count);
--
--  【建议】各职业均衡生成，不然游荡bot全是战士很怪。


-- ############################################################################
--  怎么撤销
-- ############################################################################
--
--  DELETE FROM `world`.`creature_template`                WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template_npcbot_extras`  WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template_npcbot_appearance` WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_equip_template`          WHERE `CreatureID` BETWEEN 71000 AND 71499;
--
--  【删之前先把 NpcBot.WanderingBots.Continents.Count 调小】，
--  否则重启时模板不够会 ASSERT 崩服。


-- ############################################################################
--  性能提醒（这个必须说）
-- ############################################################################
--
--  TrinityCore 是【单线程主循环】(World::Update)，每个 bot 每 tick 都跑 AI。
--
--    1,000 个    约 0.5-1 GB 内存，可接受
--   10,000 个    约 5-10 GB，单核会吃紧
--  100,000 个    单进程扛不住 —— 不是内存问题，是主循环会卡死
--
--  【建议】先 500 -> 1000 -> 3000 逐档验证，
--  每档观察 worldserver 的 CPU 占用和 "Update time diff" 日志。
--
--  要真正上万，需要先做【bot休眠/分片机制】
--  （远离玩家的bot降低AI频率），那是第3阶段的事。

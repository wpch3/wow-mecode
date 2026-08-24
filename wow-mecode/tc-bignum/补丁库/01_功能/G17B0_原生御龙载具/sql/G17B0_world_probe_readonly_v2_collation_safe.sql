-- G17-B0 world库只读资源探针 v2：跨collation安全、单语句、单结果表
-- 修复v1在information_schema与world表字符集/排序规则不同环境中出现：
-- ERROR 1271 (HY000): Illegal mix of collations for operation 'UNION'
--
-- 安全边界：只读；不写库、不建临时表、不SET会话变量、不使用密码。
-- 使用方法：在DBeaver选中真实world库后，完整执行本文件唯一SELECT，
-- 将结果表导出为UTF-8 TSV/TXT：G17B0_DB_PROBE_RESULT.txt。
--
-- 兼容策略：UNION ALL的三个文本输出列在每个分支都显式转换为同一个
-- utf8mb4_unicode_ci，避免information_schema、world表和连接默认collation互相冲突。

SELECT 10 AS seq,
       CONVERT('META' USING utf8mb4) COLLATE utf8mb4_unicode_ci AS section_name,
       CONVERT('marker' USING utf8mb4) COLLATE utf8mb4_unicode_ci AS key_name,
       CONVERT('G17B0_DB_PROBE_START' USING utf8mb4) COLLATE utf8mb4_unicode_ci AS value_text
UNION ALL
SELECT 11,
       CONVERT('META' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('database' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(COALESCE(DATABASE(),'<NULL>') USING utf8mb4) COLLATE utf8mb4_unicode_ci
UNION ALL
SELECT 12,
       CONVERT('META' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('database_version' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(VERSION() USING utf8mb4) COLLATE utf8mb4_unicode_ci
UNION ALL
SELECT 13,
       CONVERT('META' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('probe_time' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CAST(CURRENT_TIMESTAMP AS CHAR) USING utf8mb4) COLLATE utf8mb4_unicode_ci

UNION ALL
SELECT 20,
       CONVERT('TABLE' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(TABLE_NAME USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('engine=',COALESCE(ENGINE,'NULL'),';collation=',COALESCE(TABLE_COLLATION,'NULL')) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM information_schema.TABLES
WHERE TABLE_SCHEMA=DATABASE()
  AND TABLE_NAME IN
  ('creature_template','creature_template_spell','creature_template_movement',
   'spell_script_names','spell_dbc','npc_spellclick_spells','vehicle_template_accessory')

UNION ALL
SELECT 30,
       CONVERT('COLUMN' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT(TABLE_NAME,'.',COLUMN_NAME) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('pos=',ORDINAL_POSITION,';type=',COLUMN_TYPE,
              ';nullable=',IS_NULLABLE,';default=',COALESCE(CAST(COLUMN_DEFAULT AS CHAR),'NULL')) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA=DATABASE()
  AND TABLE_NAME IN
  ('creature_template','creature_template_spell','creature_template_movement','spell_script_names')
  AND COLUMN_NAME IN
  ('entry','difficulty_entry_1','difficulty_entry_2','difficulty_entry_3',
   'name','subname','IconName','modelid1','modelid2','modelid3','modelid4',
   'VehicleId','ScriptName','AIName','MovementType','VerifiedBuild',
   'CreatureID','CreatureId','Index','Spell','Ground','Swim','Flight','Rooted',
   'Chase','Random','InteractionPauseTimer','spell_id')

UNION ALL
SELECT 40,
       CONVERT('PREIMAGE' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('state' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CASE
         WHEN (SELECT COUNT(*) FROM creature_template WHERE entry=27756 AND VehicleId<>0)<>1
           THEN 'BLOCKED_SOURCE_27756_MISSING_OR_NOT_VEHICLE'
         WHEN (SELECT COUNT(*) FROM creature_template WHERE entry=1000171)=0
           THEN 'G17B0_DB_PREIMAGE_READY'
         WHEN (SELECT COUNT(*) FROM creature_template
               WHERE entry=1000171 AND ScriptName='npc_g17_dragonriding_vehicle')=1
           THEN 'G17B0_DB_ALREADY_OWNED'
         ELSE 'BLOCKED_TARGET_1000171_COLLISION'
       END USING utf8mb4) COLLATE utf8mb4_unicode_ci

UNION ALL
SELECT 50,
       CONVERT('CREATURE' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CAST(entry AS CHAR) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('name=',name,';subname=',COALESCE(subname,'NULL'),
              ';icon=',COALESCE(IconName,'NULL'),
              ';models=',modelid1,',',modelid2,',',modelid3,',',modelid4,
              ';level=',minlevel,'-',maxlevel,';faction=',faction,
              ';npcflag=',npcflag,';unit_flags=',unit_flags,';unit_flags2=',unit_flags2,
              ';dynamicflags=',dynamicflags,';VehicleId=',VehicleId,
              ';AIName=',AIName,';MovementType=',MovementType,
              ';HoverHeight=',HoverHeight,';flags_extra=',flags_extra,
              ';ScriptName=',ScriptName,';VerifiedBuild=',COALESCE(CAST(VerifiedBuild AS CHAR),'NULL')) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM creature_template
WHERE entry IN (27756,1000171)

UNION ALL
SELECT 60,
       CONVERT('ACTION_BAR' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT(CreatureID,':',`Index`) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('Spell=',Spell,';VerifiedBuild=',COALESCE(CAST(VerifiedBuild AS CHAR),'NULL')) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM creature_template_spell
WHERE CreatureID IN (27756,1000171)

UNION ALL
SELECT 70,
       CONVERT('MOVEMENT' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CAST(CreatureId AS CHAR) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('Ground=',COALESCE(CAST(Ground AS CHAR),'NULL'),
              ';Swim=',COALESCE(CAST(Swim AS CHAR),'NULL'),
              ';Flight=',COALESCE(CAST(Flight AS CHAR),'NULL'),
              ';Rooted=',COALESCE(CAST(Rooted AS CHAR),'NULL'),
              ';Chase=',COALESCE(CAST(Chase AS CHAR),'NULL'),
              ';Random=',COALESCE(CAST(Random AS CHAR),'NULL'),
              ';Pause=',COALESCE(CAST(InteractionPauseTimer AS CHAR),'NULL')) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM creature_template_movement
WHERE CreatureId IN (27756,1000171)

UNION ALL
SELECT 80,
       CONVERT('SPELLCLICK' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT(npc_entry,':',spell_id) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('cast_flags=',cast_flags,';user_type=',user_type) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM npc_spellclick_spells
WHERE npc_entry IN (27756,1000171)

UNION ALL
SELECT 90,
       CONVERT('VEHICLE_ACCESSORY' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT(entry,':',seat_id) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('accessory_entry=',accessory_entry,';minion=',minion,
              ';summontype=',summontype,';summontimer=',summontimer,
              ';description=',description) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM vehicle_template_accessory
WHERE entry IN (27756,1000171)

UNION ALL
SELECT 100,
       CONVERT('SPELL_SCRIPT' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT(spell_id,':',ScriptName) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('existing_binding' USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM spell_script_names
WHERE spell_id IN (9573,55215,52197,53208)
   OR ScriptName IN
   ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
    'spell_g17_dragon_climb','spell_g17_dragon_safe_landing')

UNION ALL
SELECT 110,
       CONVERT('CUSTOM_SPELL_DBC' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CAST(Id AS CHAR) USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('name=',COALESCE(SpellName,'NULL'),
              ';Attributes=',Attributes,';Ex=',AttributesEx,';Ex2=',AttributesEx2,';Ex3=',AttributesEx3,
              ';Targets=',Targets,';RangeIndex=',RangeIndex,';DurationIndex=',DurationIndex,
              ';Effects=',Effect1,',',Effect2,',',Effect3,
              ';TargetA=',EffectImplicitTargetA1,',',EffectImplicitTargetA2,',',EffectImplicitTargetA3,
              ';TargetB=',EffectImplicitTargetB1,',',EffectImplicitTargetB2,',',EffectImplicitTargetB3,
              ';Aura=',EffectApplyAuraName1,',',EffectApplyAuraName2,',',EffectApplyAuraName3,
              ';Misc=',EffectMiscValue1,',',EffectMiscValue2,',',EffectMiscValue3) USING utf8mb4) COLLATE utf8mb4_unicode_ci
FROM spell_dbc
WHERE Id IN (9573,55215,52197,53208)

UNION ALL
SELECT 120,
       CONVERT('SUMMARY' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('counts' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT(CONCAT('source_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=27756),
              ';source_vehicle_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=27756 AND VehicleId<>0),
              ';target_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=1000171),
              ';source_actions=',(SELECT COUNT(*) FROM creature_template_spell WHERE CreatureID=27756),
              ';target_actions=',(SELECT COUNT(*) FROM creature_template_spell WHERE CreatureID=1000171),
              ';candidate_script_bindings=',(SELECT COUNT(*) FROM spell_script_names WHERE spell_id IN (9573,55215,52197,53208))) USING utf8mb4) COLLATE utf8mb4_unicode_ci
UNION ALL
SELECT 130,
       CONVERT('SUMMARY' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('marker' USING utf8mb4) COLLATE utf8mb4_unicode_ci,
       CONVERT('G17B0_DB_PROBE_COMPLETE' USING utf8mb4) COLLATE utf8mb4_unicode_ci
ORDER BY seq,key_name;

-- G17-B0 world库只读资源探针：单语句、单结果表
-- 不写库、不建临时表、不使用密码。DBeaver选中真实world库后执行本语句，
-- 将结果表导出为UTF-8 TSV/TXT：G17B0_DB_PROBE_RESULT.txt。

SELECT 10 AS seq, 'META' AS section_name, 'marker' AS key_name,
       'G17B0_DB_PROBE_START' AS value_text
UNION ALL SELECT 11,'META','database',COALESCE(DATABASE(),'<NULL>')
UNION ALL SELECT 12,'META','database_version',VERSION()
UNION ALL SELECT 13,'META','probe_time',CAST(CURRENT_TIMESTAMP AS CHAR)

UNION ALL
SELECT 20,'TABLE',TABLE_NAME,
       CONCAT('engine=',COALESCE(ENGINE,'NULL'),';collation=',COALESCE(TABLE_COLLATION,'NULL'))
FROM information_schema.TABLES
WHERE TABLE_SCHEMA=DATABASE()
  AND TABLE_NAME IN
  ('creature_template','creature_template_spell','creature_template_movement',
   'spell_script_names','spell_dbc','npc_spellclick_spells','vehicle_template_accessory')

UNION ALL
SELECT 30,'COLUMN',CONCAT(TABLE_NAME,'.',COLUMN_NAME),
       CONCAT('pos=',ORDINAL_POSITION,';type=',COLUMN_TYPE,
              ';nullable=',IS_NULLABLE,';default=',COALESCE(CAST(COLUMN_DEFAULT AS CHAR),'NULL'))
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
SELECT 40,'PREIMAGE','state',CASE
  WHEN (SELECT COUNT(*) FROM creature_template WHERE entry=27756 AND VehicleId<>0)<>1
    THEN 'BLOCKED_SOURCE_27756_MISSING_OR_NOT_VEHICLE'
  WHEN (SELECT COUNT(*) FROM creature_template WHERE entry=1000171)=0
    THEN 'G17B0_DB_PREIMAGE_READY'
  WHEN (SELECT COUNT(*) FROM creature_template
        WHERE entry=1000171 AND ScriptName='npc_g17_dragonriding_vehicle')=1
    THEN 'G17B0_DB_ALREADY_OWNED'
  ELSE 'BLOCKED_TARGET_1000171_COLLISION'
END

UNION ALL
SELECT 50,'CREATURE',CAST(entry AS CHAR),
       CONCAT('name=',name,';subname=',COALESCE(subname,'NULL'),
              ';icon=',COALESCE(IconName,'NULL'),
              ';models=',modelid1,',',modelid2,',',modelid3,',',modelid4,
              ';level=',minlevel,'-',maxlevel,';faction=',faction,
              ';npcflag=',npcflag,';unit_flags=',unit_flags,';unit_flags2=',unit_flags2,
              ';dynamicflags=',dynamicflags,';VehicleId=',VehicleId,
              ';AIName=',AIName,';MovementType=',MovementType,
              ';HoverHeight=',HoverHeight,';flags_extra=',flags_extra,
              ';ScriptName=',ScriptName,';VerifiedBuild=',COALESCE(CAST(VerifiedBuild AS CHAR),'NULL'))
FROM creature_template
WHERE entry IN (27756,1000171)

UNION ALL
SELECT 60,'ACTION_BAR',CONCAT(CreatureID,':',`Index`),
       CONCAT('Spell=',Spell,';VerifiedBuild=',COALESCE(CAST(VerifiedBuild AS CHAR),'NULL'))
FROM creature_template_spell
WHERE CreatureID IN (27756,1000171)

UNION ALL
SELECT 70,'MOVEMENT',CAST(CreatureId AS CHAR),
       CONCAT('Ground=',COALESCE(CAST(Ground AS CHAR),'NULL'),
              ';Swim=',COALESCE(CAST(Swim AS CHAR),'NULL'),
              ';Flight=',COALESCE(CAST(Flight AS CHAR),'NULL'),
              ';Rooted=',COALESCE(CAST(Rooted AS CHAR),'NULL'),
              ';Chase=',COALESCE(CAST(Chase AS CHAR),'NULL'),
              ';Random=',COALESCE(CAST(Random AS CHAR),'NULL'),
              ';Pause=',COALESCE(CAST(InteractionPauseTimer AS CHAR),'NULL'))
FROM creature_template_movement
WHERE CreatureId IN (27756,1000171)

UNION ALL
SELECT 80,'SPELLCLICK',CONCAT(npc_entry,':',spell_id),
       CONCAT('cast_flags=',cast_flags,';user_type=',user_type)
FROM npc_spellclick_spells
WHERE npc_entry IN (27756,1000171)

UNION ALL
SELECT 90,'VEHICLE_ACCESSORY',CONCAT(entry,':',seat_id),
       CONCAT('accessory_entry=',accessory_entry,';minion=',minion,
              ';summontype=',summontype,';summontimer=',summontimer,
              ';description=',description)
FROM vehicle_template_accessory
WHERE entry IN (27756,1000171)

UNION ALL
SELECT 100,'SPELL_SCRIPT',CONCAT(spell_id,':',ScriptName),'existing_binding'
FROM spell_script_names
WHERE spell_id IN (9573,55215,52197,53208)
   OR ScriptName IN
   ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
    'spell_g17_dragon_climb','spell_g17_dragon_safe_landing')

UNION ALL
SELECT 110,'CUSTOM_SPELL_DBC',CAST(Id AS CHAR),
       CONCAT('name=',COALESCE(SpellName,'NULL'),
              ';Attributes=',Attributes,';Ex=',AttributesEx,';Ex2=',AttributesEx2,';Ex3=',AttributesEx3,
              ';Targets=',Targets,';RangeIndex=',RangeIndex,';DurationIndex=',DurationIndex,
              ';Effects=',Effect1,',',Effect2,',',Effect3,
              ';TargetA=',EffectImplicitTargetA1,',',EffectImplicitTargetA2,',',EffectImplicitTargetA3,
              ';TargetB=',EffectImplicitTargetB1,',',EffectImplicitTargetB2,',',EffectImplicitTargetB3,
              ';Aura=',EffectApplyAuraName1,',',EffectApplyAuraName2,',',EffectApplyAuraName3,
              ';Misc=',EffectMiscValue1,',',EffectMiscValue2,',',EffectMiscValue3)
FROM spell_dbc
WHERE Id IN (9573,55215,52197,53208)

UNION ALL
SELECT 120,'SUMMARY','counts',
       CONCAT('source_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=27756),
              ';source_vehicle_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=27756 AND VehicleId<>0),
              ';target_rows=',(SELECT COUNT(*) FROM creature_template WHERE entry=1000171),
              ';source_actions=',(SELECT COUNT(*) FROM creature_template_spell WHERE CreatureID=27756),
              ';target_actions=',(SELECT COUNT(*) FROM creature_template_spell WHERE CreatureID=1000171),
              ';candidate_script_bindings=',(SELECT COUNT(*) FROM spell_script_names WHERE spell_id IN (9573,55215,52197,53208)))
UNION ALL SELECT 130,'SUMMARY','marker','G17B0_DB_PROBE_COMPLETE'
ORDER BY seq,key_name;

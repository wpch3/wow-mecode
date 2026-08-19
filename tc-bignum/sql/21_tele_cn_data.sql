-- ============================================================
--  传送点中文名 · 导入数据 (v2 精确匹配版)
--  【本文件只有一条语句，把光标放在语句里执行】
--
--  v1 的 bug：用 LIKE '%%stormwind%%' 模糊匹配，
--             把 StormwindJail(暴风城监狱) 也标成了"暴风城"，
--             点"暴风城"会传到监狱副本出口，卡在栅栏门里。
--
--  v2 改为【精确匹配】：LOWER(REPLACE(name,' ','')) = 'stormwind'
--                      监狱单独一条 'stormwindjail' -> '暴风城监狱'
--
--  执行前先跑 22_tele_cn_clear.sql 清空旧数据
--  执行后核对：SELECT COUNT(*) FROM world.custom_tele_cn
-- ============================================================
INSERT IGNORE INTO world.custom_tele_cn (tele_id, name_cn, category, sort)
  SELECT id, '暴风城', '主城', 10 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stormwind'
  UNION ALL
  SELECT id, '铁炉堡', '主城', 11 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ironforge'
  UNION ALL
  SELECT id, '达纳苏斯', '主城', 12 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'darnassus'
  UNION ALL
  SELECT id, '埃索达', '主城', 13 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'exodar'
  UNION ALL
  SELECT id, '奥格瑞玛', '主城', 20 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'orgrimmar'
  UNION ALL
  SELECT id, '雷霆崖', '主城', 21 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'thunderbluff'
  UNION ALL
  SELECT id, '幽暗城', '主城', 22 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'undercity'
  UNION ALL
  SELECT id, '银月城', '主城', 23 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'silvermoon'
  UNION ALL
  SELECT id, '沙塔斯城', '主城', 30 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'shattrath'
  UNION ALL
  SELECT id, '达拉然', '主城', 31 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'dalaran'
  UNION ALL
  SELECT id, '藏宝海湾', '主城', 32 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'bootybay'
  UNION ALL
  SELECT id, '加基森', '主城', 33 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'gadgetzan'
  UNION ALL
  SELECT id, '永望镇', '主城', 34 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'everlook'
  UNION ALL
  SELECT id, '棘齿城', '主城', 35 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ratchet'
  UNION ALL
  SELECT id, '艾尔文森林', '东部王国', 100 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'elwynn'
  UNION ALL
  SELECT id, '西部荒野', '东部王国', 101 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'westfall'
  UNION ALL
  SELECT id, '赤脊山', '东部王国', 102 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'redridge'
  UNION ALL
  SELECT id, '暮色森林', '东部王国', 103 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'duskwood'
  UNION ALL
  SELECT id, '荆棘谷', '东部王国', 104 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stranglethorn'
  UNION ALL
  SELECT id, '悲伤沼泽', '东部王国', 105 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'swampofsorrows'
  UNION ALL
  SELECT id, '诅咒之地', '东部王国', 106 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'blastedlands'
  UNION ALL
  SELECT id, '燃烧平原', '东部王国', 107 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'burningsteppes'
  UNION ALL
  SELECT id, '灼热峡谷', '东部王国', 108 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'searinggorge'
  UNION ALL
  SELECT id, '荒芜之地', '东部王国', 109 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'badlands'
  UNION ALL
  SELECT id, '湿地', '东部王国', 110 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'wetlands'
  UNION ALL
  SELECT id, '丹莫罗', '东部王国', 111 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'dunmorogh'
  UNION ALL
  SELECT id, '阿拉希高地', '东部王国', 112 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'arathi'
  UNION ALL
  SELECT id, '希尔斯布莱德丘陵', '东部王国', 113 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'hillsbrad'
  UNION ALL
  SELECT id, '奥特兰克山脉', '东部王国', 114 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'alterac'
  UNION ALL
  SELECT id, '辛特兰', '东部王国', 115 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'hinterlands'
  UNION ALL
  SELECT id, '西瘟疫之地', '东部王国', 116 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'westernplaguelands'
  UNION ALL
  SELECT id, '东瘟疫之地', '东部王国', 117 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'easternplaguelands'
  UNION ALL
  SELECT id, '提瑞斯法林地', '东部王国', 118 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'tirisfal'
  UNION ALL
  SELECT id, '银松森林', '东部王国', 119 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'silverpine'
  UNION ALL
  SELECT id, '幽魂之地', '东部王国', 120 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ghostlands'
  UNION ALL
  SELECT id, '永歌森林', '东部王国', 121 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'eversong'
  UNION ALL
  SELECT id, '杜隆塔尔', '卡利姆多', 200 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'durotar'
  UNION ALL
  SELECT id, '莫高雷', '卡利姆多', 201 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'mulgore'
  UNION ALL
  SELECT id, '贫瘠之地', '卡利姆多', 202 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'barrens'
  UNION ALL
  SELECT id, '灰谷', '卡利姆多', 203 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ashenvale'
  UNION ALL
  SELECT id, '石爪山脉', '卡利姆多', 204 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stonetalon'
  UNION ALL
  SELECT id, '凄凉之地', '卡利姆多', 205 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'desolace'
  UNION ALL
  SELECT id, '菲拉斯', '卡利姆多', 206 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'feralas'
  UNION ALL
  SELECT id, '尘泥沼泽', '卡利姆多', 207 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'dustwallow'
  UNION ALL
  SELECT id, '塔纳利斯', '卡利姆多', 208 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'tanaris'
  UNION ALL
  SELECT id, '安戈洛环形山', '卡利姆多', 209 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ungoro'
  UNION ALL
  SELECT id, '希利苏斯', '卡利姆多', 210 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'silithus'
  UNION ALL
  SELECT id, '费伍德森林', '卡利姆多', 211 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'felwood'
  UNION ALL
  SELECT id, '冬泉谷', '卡利姆多', 212 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'winterspring'
  UNION ALL
  SELECT id, '月光林地', '卡利姆多', 213 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'moonglade'
  UNION ALL
  SELECT id, '艾萨拉', '卡利姆多', 214 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'azshara'
  UNION ALL
  SELECT id, '千针石林', '卡利姆多', 215 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'thousandneedles'
  UNION ALL
  SELECT id, '泰达希尔', '卡利姆多', 216 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'teldrassil'
  UNION ALL
  SELECT id, '黑海岸', '卡利姆多', 217 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'darkshore'
  UNION ALL
  SELECT id, '血谜岛', '卡利姆多', 218 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'bloodmyst'
  UNION ALL
  SELECT id, '蔚蓝湾', '卡利姆多', 219 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'azuremyst'
  UNION ALL
  SELECT id, '地狱火半岛', '外域', 300 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'hellfire'
  UNION ALL
  SELECT id, '赞加沼泽', '外域', 301 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'zangarmarsh'
  UNION ALL
  SELECT id, '泰罗卡森林', '外域', 302 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'terokkar'
  UNION ALL
  SELECT id, '纳格兰', '外域', 303 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'nagrand'
  UNION ALL
  SELECT id, '刀锋山', '外域', 304 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'bladesedge'
  UNION ALL
  SELECT id, '虚空风暴', '外域', 305 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'netherstorm'
  UNION ALL
  SELECT id, '影月谷', '外域', 306 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'shadowmoon'
  UNION ALL
  SELECT id, '北风苔原', '诺森德', 400 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'boreantundra'
  UNION ALL
  SELECT id, '嚎风峡湾', '诺森德', 401 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'howlingfjord'
  UNION ALL
  SELECT id, '龙骨荒野', '诺森德', 402 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'dragonblight'
  UNION ALL
  SELECT id, '灰熊丘陵', '诺森德', 403 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'grizzlyhills'
  UNION ALL
  SELECT id, '祖达克', '诺森德', 404 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'zuldrak'
  UNION ALL
  SELECT id, '索拉查盆地', '诺森德', 405 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'sholazar'
  UNION ALL
  SELECT id, '风暴峭壁', '诺森德', 406 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stormpeaks'
  UNION ALL
  SELECT id, '冰冠冰川', '诺森德', 407 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'icecrown'
  UNION ALL
  SELECT id, '冬拥湖', '诺森德', 408 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'wintergrasp'
  UNION ALL
  SELECT id, '水晶歌森林', '诺森德', 409 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'crystalsong'
  UNION ALL
  SELECT id, '死亡矿井', '副本', 500 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'deadmines'
  UNION ALL
  SELECT id, '哀嚎洞穴', '副本', 501 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'wailingcaverns'
  UNION ALL
  SELECT id, '影牙城堡', '副本', 502 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'shadowfang'
  UNION ALL
  SELECT id, '暴风城监狱', '副本', 503 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stormwindjail'
  UNION ALL
  SELECT id, '诺莫瑞根', '副本', 504 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'gnomeregan'
  UNION ALL
  SELECT id, '剃刀高地', '副本', 505 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'razorfendowns'
  UNION ALL
  SELECT id, '剃刀沼泽', '副本', 506 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'razorfenkraul'
  UNION ALL
  SELECT id, '血色修道院', '副本', 507 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'scarletmonastery'
  UNION ALL
  SELECT id, '乌达曼', '副本', 508 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'uldaman'
  UNION ALL
  SELECT id, '祖尔法拉克', '副本', 509 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'zulfarrak'
  UNION ALL
  SELECT id, '玛拉顿', '副本', 510 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'maraudon'
  UNION ALL
  SELECT id, '沉没的神庙', '副本', 511 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'sunkentemple'
  UNION ALL
  SELECT id, '黑石深渊', '副本', 512 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'blackrockdepths'
  UNION ALL
  SELECT id, '黑石塔', '副本', 513 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'blackrockspire'
  UNION ALL
  SELECT id, '厄运之槌', '副本', 514 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'diremaul'
  UNION ALL
  SELECT id, '斯坦索姆', '副本', 515 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'stratholme'
  UNION ALL
  SELECT id, '通灵学院', '副本', 516 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'scholomance'
  UNION ALL
  SELECT id, '熔火之心', '团本', 600 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'moltencore'
  UNION ALL
  SELECT id, '奥妮克希亚的巢穴', '团本', 601 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'onyxia'
  UNION ALL
  SELECT id, '黑翼之巢', '团本', 602 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'blackwinglair'
  UNION ALL
  SELECT id, '安其拉', '团本', 603 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ahnqiraj'
  UNION ALL
  SELECT id, '纳克萨玛斯', '团本', 604 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'naxxramas'
  UNION ALL
  SELECT id, '卡拉赞', '团本', 610 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'karazhan'
  UNION ALL
  SELECT id, '格鲁尔的巢穴', '团本', 611 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'gruulslair'
  UNION ALL
  SELECT id, '玛瑟里顿的巢穴', '团本', 612 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'magtheridon'
  UNION ALL
  SELECT id, '毒蛇神殿', '团本', 613 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'serpentshrine'
  UNION ALL
  SELECT id, '风暴要塞', '团本', 614 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'tempestkeep'
  UNION ALL
  SELECT id, '海加尔山', '团本', 615 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'hyjal'
  UNION ALL
  SELECT id, '黑暗神殿', '团本', 616 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'blacktemple'
  UNION ALL
  SELECT id, '太阳井高地', '团本', 617 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'sunwell'
  UNION ALL
  SELECT id, '祖阿曼', '团本', 618 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'zulaman'
  UNION ALL
  SELECT id, '黑曜石圣殿', '团本', 620 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'obsidiansanctum'
  UNION ALL
  SELECT id, '永恒之眼', '团本', 621 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'eyeofeternity'
  UNION ALL
  SELECT id, '奥杜尔', '团本', 622 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'ulduar'
  UNION ALL
  SELECT id, '十字军的试炼', '团本', 623 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'trialofthecrusader'
  UNION ALL
  SELECT id, '冰冠堡垒', '团本', 624 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'icecrowncitadel'
  UNION ALL
  SELECT id, '红玉圣殿', '团本', 625 FROM world.game_tele WHERE LOWER(REPLACE(name,' ','')) = 'rubysanctum';

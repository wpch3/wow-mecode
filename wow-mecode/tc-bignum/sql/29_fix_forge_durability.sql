/* 修复自造装备的耐久溢出 */
/* 原因 Item.cpp:365 存耐久用 setUInt16 上限65535 超了回绕成0 武器被判定已损坏 */
UPDATE world.item_template SET MaxDurability = 60000 WHERE entry BETWEEN 800000 AND 899999 AND MaxDurability > 60000;

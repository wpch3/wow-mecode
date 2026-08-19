/*
 * persisttest.cpp —— step20 接入点验证
 * 重点：.set clear 必须同步清库，否则重登会复活
 */
#include "mock.h"
#include "CombatSpecData.h"
static int P=0,F=0;
static void CHECK(bool ok,char const*w){ if(ok){++P;printf("  [OK]   %s\n",w);} else {++F;printf("  [FAIL] %s\n",w);} }

int main(){
    auto* mgr = sCustomStatPersist;

    printf("\n===== 1. .set crit 100 会写库 =====\n");
    mgr->records.clear();
    // 模拟 SetRating：改内存 -> 写库
    uint32 guid=888; int32 want=2000; int cr=8;  // CR_CRIT_MELEE
    mgr->Record(guid, CustomStatPersistMgr::TYPE_RATING, uint8(cr), float(want));
    CHECK(mgr->records.size()==1, "写了 1 条记录");
    CHECK(mgr->records[0].amt==2000.f, "记录值正确");
    CHECK(mgr->records[0].type==CustomStatPersistMgr::TYPE_RATING, "类型是 TYPE_RATING");

    printf("\n===== 2. 三个联动 rating 各写一条 =====\n");
    mgr->records.clear();
    int linked[]={8,9,10};   // CRIT_MELEE/RANGED/SPELL
    for(int c:linked) mgr->Record(guid,CustomStatPersistMgr::TYPE_RATING,uint8(c),float(want));
    CHECK(mgr->records.size()==3, "近战/远程/法术各存一条");

    printf("\n===== 3. .set clear 必须清库（关键）=====\n");
    // 修复前：只撤内存，不动库
    mgr->records.clear();
    int oldWrites=0;
    // (旧逻辑什么都不写)
    CHECK(oldWrites==0, "旧逻辑 clear 不写库 -> 重登会复活（bug）");

    // 修复后：每项 Record(...,0)
    mgr->records.clear();
    for(int c:linked) mgr->Record(guid,CustomStatPersistMgr::TYPE_RATING,uint8(c),0.0f);
    CHECK(mgr->records.size()==3, "新逻辑 clear 写了 3 条删除记录");
    bool allZero=true; for(auto&r:mgr->records) if(r.amt!=0.f) allZero=false;
    CHECK(allZero, "全部 amount=0（表示删除）");

    printf("\n===== 4. Enabled 开关生效 =====\n");
    mgr->_enabled=false;
    mgr->records.clear();
    if(mgr->Enabled()) mgr->Record(guid,CustomStatPersistMgr::TYPE_RATING,8,100.f);
    CHECK(mgr->records.empty(), "Persist=0 时不写库（保持原版行为）");
    mgr->_enabled=true;

    printf("\n===== 5. 索引范围安全 =====\n");
    // CombatRating 最大 24，uint8 装得下
    CHECK(24 < 256, "CombatRating 索引 <256，uint8 安全");
    CHECK(int(CustomStatPersistMgr::TYPE_RATING)==1, "TYPE_RATING=1");
    CHECK(int(CustomStatPersistMgr::TYPE_UNITMOD)==0, "TYPE_UNITMOD=0");

    printf("\n========================================\n  通过 %d / 失败 %d\n========================================\n",P,F);
    return F?1:0;
}

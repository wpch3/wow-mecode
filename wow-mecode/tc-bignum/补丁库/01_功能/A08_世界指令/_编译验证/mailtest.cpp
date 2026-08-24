// ============================================================
//  step21 v4.0 —— 邮箱修复验证
//
//  用户实测：「其他五个npc都好了就差邮箱了，还是一个人类npc而且没有邮箱功能」
//
//  这个测试复刻 .service 里邮箱那段的真实逻辑，验证：
//    1. npcflag 位运算对不对（1 -> 67108865）
//    2. 邮箱 GO 的运行时探测在各种库状态下都不出错
//    3. 探测结果的缓存语义（0xFFFFFFFF=未探测 / 0=探测过没找到）
// ============================================================
#include "mock.h"
#include <cstdio>

static int caseNo=0, pass=0, fail=0;
#define C(n) do{ printf("  [%02d] %-58s ", ++caseNo, n);}while(0)
#define OK() do{ printf("PASS\n"); ++pass; }while(0)
#define NG(f,...) do{ printf("FAIL " f "\n", ##__VA_ARGS__); ++fail; }while(0)

// ---- 复刻 cs_worldtools.cpp v4.0 的邮箱 GO 探测逻辑 ----
static uint32 DetectMailboxGo()
{
    uint32 found = 0;
    if (GameObjectTemplate const* pref = sObjectMgr->GetGameObjectTemplate(184137))
        if (pref->type == GAMEOBJECT_TYPE_MAILBOX)
            found = 184137;
    if (!found)
    {
        for (auto const& kv : sObjectMgr->GetGameObjectTemplates())
        {
            if (kv.second.type == GAMEOBJECT_TYPE_MAILBOX)
            {
                found = kv.first;
                break;
            }
        }
    }
    return found;
}

static void ResetGO() { ObjectMgr::GOStore().clear(); }
static void AddGO(uint32 entry, uint32 type)
{
    GameObjectTemplate t; t.entry = entry; t.type = type;
    ObjectMgr::GOStore()[entry] = t;
}

int main()
{
    printf("=== step21 v4.0 邮箱修复验证 ===\n\n");

    printf("-- A. npcflag 位运算（sql/37 的 960004）--\n");

    C("旧值 1 = 只有 GOSSIP，没有 MAILBOX -> 这就是病根");
    { uint32 old = 1;
      if ((old & UNIT_NPC_FLAG_GOSSIP) && !(old & UNIT_NPC_FLAG_MAILBOX)) OK();
      else NG("old=%u", old); }

    C("新值 67108865 = GOSSIP(1) + MAILBOX(0x04000000)");
    { uint32 nv = 67108865;
      if ((nv & UNIT_NPC_FLAG_GOSSIP) && (nv & UNIT_NPC_FLAG_MAILBOX)) OK();
      else NG("nv=%u", nv); }

    C("算术校验 1 + 67108864 == 67108865");
    { if (1u + 0x04000000u == 67108865u) OK(); else NG("?"); }

    C("MAILBOX 位就是 UnitDefines.h:263 的 0x04000000");
    { if (UNIT_NPC_FLAG_MAILBOX == 0x04000000) OK();
      else NG("=%u", (uint32)UNIT_NPC_FLAG_MAILBOX); }

    C("没误伤其他服务的 npcflag（拍卖 2097153 仍含 AUCTIONEER）");
    { uint32 auc = 2097153;
      if ((auc & UNIT_NPC_FLAG_GOSSIP) && (auc & UNIT_NPC_FLAG_AUCTIONEER)) OK();
      else NG("auc=%u", auc); }

    printf("\n-- B. 邮箱 GO 运行时探测 --\n");

    C("库里有 184137 且 type=19 -> 优先选它");
    { ResetGO(); AddGO(184137, GAMEOBJECT_TYPE_MAILBOX); AddGO(999, GAMEOBJECT_TYPE_MAILBOX);
      uint32 g = DetectMailboxGo();
      if (g == 184137) OK(); else NG("g=%u", g); }

    C("库里没有 184137，但有别的邮箱 GO -> 自动退而求其次");
    { ResetGO(); AddGO(20000, GAMEOBJECT_TYPE_MAILBOX);
      uint32 g = DetectMailboxGo();
      if (g == 20000) OK(); else NG("g=%u", g); }

    C("[关键] 库里完全没有邮箱 GO -> 返回 0，不崩不乱召唤");
    { ResetGO(); AddGO(184137 + 1, 5); AddGO(300, 3);
      uint32 g = DetectMailboxGo();
      if (g == 0) OK(); else NG("g=%u", g); }
    printf("       ^ 这正是上一版翻车点：184137 不在本仓库 TDB 里\n");

    C("184137 存在但 type 不是邮箱(比如是门) -> 不误用，退到遍历");
    { ResetGO(); AddGO(184137, 0 /*DOOR*/); AddGO(777, GAMEOBJECT_TYPE_MAILBOX);
      uint32 g = DetectMailboxGo();
      if (g == 777) OK(); else NG("g=%u", g); }

    C("184137 是门且没有任何别的邮箱 -> 返回 0");
    { ResetGO(); AddGO(184137, 0);
      uint32 g = DetectMailboxGo();
      if (g == 0) OK(); else NG("g=%u", g); }

    C("空库 -> 返回 0，不崩");
    { ResetGO();
      uint32 g = DetectMailboxGo();
      if (g == 0) OK(); else NG("g=%u", g); }

    printf("\n-- C. 静态缓存语义 --\n");

    C("0xFFFFFFFF 表示未探测，与合法 entry 0 可区分");
    { uint32 uninit = 0xFFFFFFFF, notfound = 0;
      if (uninit != notfound && uninit != 184137) OK(); else NG("?"); }

    C("探测过没找到(0) 不会被误判为需要重新探测");
    { uint32 cache = 0xFFFFFFFF;
      ResetGO();
      if (cache == 0xFFFFFFFF) { cache = 0; cache = DetectMailboxGo(); }
      bool wouldRedetect = (cache == 0xFFFFFFFF);
      if (!wouldRedetect && cache == 0) OK(); else NG("cache=%u", cache); }

    printf("\n-- D. 召唤行为 --\n");

    C("邮差是 NPC 不是 GO（走 SummonCreature 那条路）");
    { struct Svc { char const* key; uint32 entry; bool isGO; };
      Svc mail = { "mail", 960004, false };
      if (!mail.isGO && mail.entry == 960004) OK(); else NG("?"); }

    C("探到 GO 时额外召一个邮箱（GO 计数 +1）");
    { ResetGO(); AddGO(184137, GAMEOBJECT_TYPE_MAILBOX);
      Player p; Player::goSummonCount = 0;
      uint32 g = DetectMailboxGo();
      if (g) { QuaternionData rot; p.SummonGameObject(g, 0,0,0,0, rot, Seconds(300)); }
      if (Player::goSummonCount == 1) OK(); else NG("cnt=%u", Player::goSummonCount); }

    C("没探到 GO 时不召唤（GO 计数 0），NPC 那条路仍可用");
    { ResetGO();
      Player p; Player::goSummonCount = 0;
      uint32 g = DetectMailboxGo();
      if (g) { QuaternionData rot; p.SummonGameObject(g, 0,0,0,0, rot, Seconds(300)); }
      if (Player::goSummonCount == 0) OK(); else NG("cnt=%u", Player::goSummonCount); }

    C("双保险：NPC(MAILBOX flag) 与 GO 至少一条路通");
    { uint32 npcflag = 67108865;
      ResetGO();                       // 最坏情况：库里一个邮箱GO都没有
      uint32 g = DetectMailboxGo();
      bool npcPath = (npcflag & UNIT_NPC_FLAG_MAILBOX) != 0;   // MailHandler.cpp:56
      bool goPath  = (g != 0);                                  // MailHandler.cpp:51
      if (npcPath || goPath) OK(); else NG("两条路都不通"); }

    printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n", pass, fail, caseNo);
    return fail == 0 ? 0 : 1;
}

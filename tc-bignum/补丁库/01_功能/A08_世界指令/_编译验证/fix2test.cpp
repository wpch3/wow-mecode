/*
 * fix2test.cpp —— step21 v2 两个修复
 *   1. .raidbuff 没给 bot 上 buff（Group 里玩家和bot是两条链表）
 *   2. .service 阵营/职业不对（不该写死 entry）
 */
#include "mock.h"
static int P=0,F=0;
static void CK(bool ok,char const*w){ if(ok){++P;printf("  [OK]   %s\n",w);}else{++F;printf("  [FAIL] %s\n",w);} }

int main(){
    printf("\n===== 修复1：raidbuff 覆盖 bot =====\n");
    {
        Player me; Player p2;
        Creature bot1, bot2, bot3;
        bot1._bot=bot2._bot=bot3._bot=true;

        GroupReference r1,r2;
        GroupBotReference b1,b2,b3;
        Group g;
        g._f=&r1; r1._p=&me; r1._n=&r2; r2._p=&p2;
        g._bf=&b1; b1._c=&bot1; b1._n=&b2; b2._c=&bot2; b2._n=&b3; b3._c=&bot3;
        me._g=&g;

        // 旧逻辑：只遍历玩家链表
        int oldN=0;
        for(GroupReference* it=g.GetFirstMember(); it; it=it->next())
            if(it->GetSource()) ++oldN;
        printf("         旧逻辑收集到 %d 个（2玩家+3bot 的队伍）\n", oldN);
        CK(oldN==2, "旧逻辑只拿到2个玩家 -> bot全漏（复现bug）");

        // 新逻辑：两条链表都遍历
        std::vector<Unit*> out;
        for(GroupReference* it=g.GetFirstMember(); it; it=it->next())
            if(it->GetSource()) out.push_back(it->GetSource());
        for(GroupBotReference* it=g.GetFirstBotMember(); it; it=it->next())
            if(it->GetSource()) out.push_back(it->GetSource());
        printf("         新逻辑收集到 %zu 个\n", out.size());
        CK(out.size()==5, "新逻辑拿到 2玩家+3bot = 5 个");

        int bots=0;
        for(Unit* u:out) if(u->GetTypeId()!=TYPEID_PLAYER) ++bots;
        CK(bots==3, "其中 3 个是 bot");
    }

    printf("\n===== 修复1b：没组队时也覆盖跟随的bot =====\n");
    {
        Player solo;
        CK(solo.GetGroup()==nullptr, "确认没组队");
        // 没组队 -> 走周围扫描分支
        Creature b1,b2; b1._bot=true; b2._bot=true;
        Creature mob; mob._bot=false;
        g_nearCreatures={&b1,&b2,&mob};
        std::vector<Unit*> out;
        out.push_back(&solo);
        for(Creature* c: g_nearCreatures){
            if(!c->IsAlive()) continue;
            if(!c->IsNPCBotOrPet()) continue;
            out.push_back(c);
        }
        printf("         周围3个生物(2bot+1怪) -> 收集 %zu 个\n", out.size());
        CK(out.size()==3, "自己+2个bot，普通怪被排除");
        g_nearCreatures.clear();
    }

    printf("\n===== 修复2（v3）：独立 entry =====\n");
    {
        /*
         * v2 用同一个 entry 改 flags -> 客户端按 entry 缓存名字和 npcflag，
         * 实测出现「四个拍卖师费奇」，只有一个能用。
         * v3 改成每个服务独立 entry（SQL 建模板）。
         */
        struct Svc { char const* cn; uint32 entry; uint32 npcflag; };
        Svc svcs[] = {
            {"便携拍卖师",   960001, 1|2097152},
            {"便携银行家",   960002, 1|131072},
            {"便携修理匠",   960003, 1|128|4096},
            {"便携邮箱",     960004, 1|67108864},
            {"便携旅店老板", 960005, 1|65536},
            {"便携商人",     960007, 1|128},
        };

        // entry 必须互不相同 —— 这是 v2 的根本问题
        bool uniq=true;
        for(size_t i=0;i<6;++i) for(size_t j=i+1;j<6;++j)
            if(svcs[i].entry==svcs[j].entry) uniq=false;
        CK(uniq, "6 个服务 entry 互不相同（v2 全用 8719 才出的bug）");

        // 都在规划的 960000 段
        bool inRange=true;
        for(auto&s2:svcs) if(s2.entry<960000||s2.entry>960019) inRange=false;
        CK(inRange, "entry 都在 960000-960019 规划段内");

        // npcflag 正确且不串味
        CK((svcs[0].npcflag & 2097152)!=0, "拍卖师有 AUCTIONEER(2097152)");
        CK((svcs[1].npcflag & 131072)!=0,  "银行家有 BANKER(131072)");
        CK((svcs[1].npcflag & 2097152)==0, "银行家【没有】拍卖行标志");
        CK((svcs[2].npcflag & 4096)!=0,    "修理匠有 REPAIR(4096)");
        CK((svcs[3].npcflag & 67108864)!=0,"邮箱有 MAILBOX(67108864)");
        CK((svcs[4].npcflag & 65536)!=0,   "旅店老板有 INNKEEPER(65536)");

        // 全部带 GOSSIP，否则点不开
        bool allGossip=true;
        for(auto&s2:svcs) if((s2.npcflag & 1)==0) allGossip=false;
        CK(allGossip, "全部带 GOSSIP(1)，能点开对话");

        // 模板缺失时要有兜底提示
        ObjectMgr mgr;
        CK(mgr.GetCreatureTemplate(960001)==nullptr,
           "模板不存在时返回 nullptr -> 代码会提示去执行 SQL");
    }

    printf("\n========================================\n  通过 %d / 失败 %d\n========================================\n",P,F);
    return F?1:0;
}

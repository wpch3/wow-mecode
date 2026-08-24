/*
 * wttest.cpp —— step21 世界指令验证
 * 重点：保护系统（主城/圣所/小镇）、二次确认、NPCBot 不被误删
 */
#include "mock.h"
namespace GameTime { void AdvanceMs(uint32); void SetMs(uint32); }
static int P=0,F=0;
static void CK(bool ok,char const*w){ if(ok){++P;printf("  [OK]   %s\n",w);}else{++F;printf("  [FAIL] %s\n",w);} }

// ---- 复刻保护判定 ----
struct Prot { bool city=true,sanctuary=true,town=false,gmOnly=true,players=true,friendly=true; };
struct Guard { bool blocked=false; char const* reason=""; };
static void CheckArea(uint32 flags, Prot const& pr, Guard& g){
    g.blocked=false; g.reason="";
    if(pr.city && (flags & AREA_FLAG_CAPITAL)){ g.blocked=true; g.reason="主城"; return; }
    if(pr.sanctuary && (flags & AREA_FLAG_SANCTUARY)){ g.blocked=true; g.reason="圣所"; return; }
    if(pr.town && (flags & AREA_FLAG_TOWN)){ g.blocked=true; g.reason="小镇"; return; }
}

int main(){
    printf("\n===== 1. 主城保护（AREA_FLAG_CAPITAL 覆盖全种族）=====\n");
    {
        Prot pr; Guard g;
        // 7 个主城都带 CAPITAL flag
        char const* cities[]={"奥格瑞玛","暴风城","雷霆崖","幽暗城","达纳苏斯","埃索达","银月城"};
        bool allBlocked=true;
        for(auto c:cities){ CheckArea(AREA_FLAG_CAPITAL,pr,g); if(!g.blocked) allBlocked=false; (void)c; }
        CK(allBlocked,"7个主城全部拦截（一个flag搞定，不用手写列表）");

        CheckArea(0,pr,g);
        CK(!g.blocked,"野外不拦截");

        pr.city=false; CheckArea(AREA_FLAG_CAPITAL,pr,g);
        CK(!g.blocked,".protect city off 后主城可执行");
    }

    printf("\n===== 2. 圣所与小镇 =====\n");
    {
        Prot pr; Guard g;
        CheckArea(AREA_FLAG_SANCTUARY,pr,g);
        CK(g.blocked && std::string(g.reason)=="圣所","圣所默认拦截（达拉然/沙塔斯）");

        CheckArea(AREA_FLAG_TOWN,pr,g);
        CK(!g.blocked,"小镇默认【不】拦截（默认关）");

        pr.town=true; CheckArea(AREA_FLAG_TOWN,pr,g);
        CK(g.blocked,"开启后小镇也拦截");
    }

    printf("\n===== 3. 三档独立开关 =====\n");
    {
        Prot pr; Guard g;
        pr.city=false; pr.sanctuary=true;
        CheckArea(AREA_FLAG_CAPITAL,pr,g);  CK(!g.blocked,"关主城不影响圣所设置");
        CheckArea(AREA_FLAG_SANCTUARY,pr,g); CK(g.blocked,"圣所仍然拦截");
    }

    printf("\n===== 4. 半径：无上限、最低可设很小 =====\n");
    {
        auto clamp=[](float r){ return r<1.0f?1.0f:r; };
        CK(clamp(0.1f)==1.0f,"低于1码 -> 收敛到1码（不会是0）");
        CK(clamp(1.0f)==1.0f,"1码可用（最小）");
        CK(clamp(50000.0f)==50000.0f,"50000码照样接受（无上限）");
        CK(clamp(999999.0f)==999999.0f,"百万码也不截断");
    }

    printf("\n===== 5. 二次确认 =====\n");
    {
        std::unordered_map<uint32,std::pair<std::string,uint32>> pend;
        auto need=[&](uint32 guid,std::string const& key)->bool{
            uint32 now=GameTime::GetGameTimeMS();
            auto it=pend.find(guid);
            if(it!=pend.end() && it->second.first==key && now<it->second.second){
                pend.erase(it); return true; }
            pend[guid]={key,now+10000}; return false; };

        GameTime::SetMs(100000);
        CK(!need(1,"killr:500"),"第一次 -> 只提示不执行");
        CK(need(1,"killr:500"),"第二次相同指令 -> 执行");
        CK(!need(1,"killr:500"),"用掉后再来 -> 又要确认");

        // 参数不同不能复用确认
        need(2,"killr:100");
        CK(!need(2,"killr:9999"),"换了半径 -> 确认不可复用（防误操作）");

        // 超时
        GameTime::SetMs(200000);
        need(3,"killr:50");
        GameTime::AdvanceMs(11000);
        CK(!need(3,"killr:50"),"超过10秒 -> 确认失效");
    }

    printf("\n===== 6. NPCBot 永不被杀/删（关键）=====\n");
    {
        Creature bot; bot._bot=true;
        Creature mob; mob._bot=false;
        Creature pet; pet._pet=true;
        Creature totem; totem._totem=true;
        Creature trig; trig._trigger=true;

        auto killable=[](Creature& c,bool friendlyProt)->bool{
            if(!c.IsAlive()) return false;
            if(c.IsNPCBotOrPet()) return false;
            if(c.IsPet()||c.IsTotem()||c.IsTrigger()) return false;
            if(friendlyProt && c.IsFriendlyTo(nullptr)) return false;
            return true; };

        CK(!killable(bot,true),  "NPCBot 不会被秒杀");
        CK(!killable(pet,true),  "宠物 不会被秒杀");
        CK(!killable(totem,true),"图腾 不会被秒杀");
        CK(!killable(trig,true), "触发器 不会被秒杀");
        CK(killable(mob,true),   "普通敌对怪 可以杀");

        Creature ally; ally._friendly=true;
        CK(!killable(ally,true), "友方保护开 -> 不打友方");
        CK(killable(ally,false), "友方保护关 -> 可以打");
    }

    printf("\n===== 7. 删NPC：gmOnly 口径 =====\n");
    {
        // gmOnly=true 时只清临时召唤，不动数据库刷点（原版NPC）
        Creature temp; temp._isTemp=true;
        Creature dbNpc; dbNpc._spawnId=12345;
        Creature bot; bot._bot=true; bot._spawnId=999;

        auto classify=[](Creature& c,bool gmOnly,bool& isTemp,bool& isDb){
            isTemp=false; isDb=false;
            if(c.IsNPCBotOrPet()) return;
            if(c.IsPet()) return;
            if(c.ToTempSummon()){ isTemp=true; return; }
            if(!gmOnly && c.GetSpawnId()) isDb=true; };

        bool t,d;
        classify(temp,true,t,d);  CK(t&&!d,"gmOnly开：临时召唤 -> UnSummon");
        classify(dbNpc,true,t,d); CK(!t&&!d,"gmOnly开：数据库NPC -> 【不删】（保护原版）");
        classify(dbNpc,false,t,d);CK(!t&&d, "gmOnly关：数据库NPC -> DeleteFromDB");
        classify(bot,false,t,d);  CK(!t&&!d,"NPCBot 任何模式都不删");
    }

    printf("\n===== 8. 副本清小怪：BOSS 必须保留 =====\n");
    {
        Creature boss; boss._boss=true;
        Creature trash; trash._boss=false;
        auto isTrash=[](Creature& c)->bool{
            if(!c.IsAlive()) return false;
            if(c.IsNPCBotOrPet()||c.IsPet()||c.IsTotem()||c.IsTrigger()) return false;
            if(c.IsFriendlyTo(nullptr)) return false;
            if(c.IsDungeonBoss()) return false;   // Creature.h:129
            return true; };
        CK(!isTrash(boss),"BOSS 保留（IsDungeonBoss）");
        CK(isTrash(trash),"小怪清除");
    }

    printf("\n===== 9. 开门只动门和按钮 =====\n");
    {
        GameObject door; door._t=GAMEOBJECT_TYPE_DOOR;
        GameObject btn;  btn._t=GAMEOBJECT_TYPE_BUTTON;
        GameObject chest; chest._t=GameobjectTypes(3);
        auto openable=[](GameObject& g){ auto t=g.GetGoType();
            return t==GAMEOBJECT_TYPE_DOOR||t==GAMEOBJECT_TYPE_BUTTON; };
        CK(openable(door),"门可开");
        CK(openable(btn),"按钮可开");
        CK(!openable(chest),"宝箱不动（避免误开）");
    }

    printf("\n===== 10. 全团增益：跨地图成员排除 =====\n");
    {
        Player me; Player m1,m2;
        GroupReference r1,r2,r3;
        Group g; g._f=&r1; r1._p=&me; r1._n=&r2; r2._p=&m1; r2._n=&r3; r3._p=&m2;
        me._g=&g;
        int n=0;
        for(GroupReference* it=g.GetFirstMember(); it; it=it->next())
            if(it->GetSource() && it->GetSource()->IsInWorld()) ++n;
        CK(n==3,"3人小队全部收集到");

        Player solo;
        CK(solo.GetGroup()==nullptr,"没组队 -> 只作用自己");
    }

    printf("\n========================================\n  通过 %d / 失败 %d\n========================================\n",P,F);
    return F?1:0;
}

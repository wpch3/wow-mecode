#include "mock.h"
ObjectMgrMock* sObjectMgr = new ObjectMgrMock();
#include "body.cpp"
static void AddItem(uint32 id,const char*n,uint32 cls,uint32 sub,uint32 q,uint32 inv,uint32 il,uint32 armor,int32 s1){
    ItemTemplate t; t.ItemId=id; t.Name1=n; t.Class=cls; t.SubClass=sub; t.Quality=q;
    t.InventoryType=inv; t.ItemLevel=il; t.Armor=armor; t.StatsCount=1;
    t.ItemStat[0].ItemStatType=4; t.ItemStat[0].ItemStatValue=s1;
    sObjectMgr->items[id]=t;
}
static void AddCre(uint32 id,const char*n,uint8 lv,uint32 rank,uint32 model){
    CreatureTemplate c; c.Name=n; c.minlevel=lv; c.maxlevel=lv; c.rank=rank; c.Modelid1=model;
    sObjectMgr->creatures[id]=c;
    CreatureLocale cl; cl.Name.resize(9); cl.Name[4]=n; sObjectMgr->clocales[id]=cl;
}
int main(){
    // 埃辛诺斯战刃：主手/副手，仅 InventoryType 不同 -> 应列候选
    AddItem(32837,"埃辛诺斯战刃",2,7,5,21,156,0,50);
    AddItem(32838,"埃辛诺斯战刃",2,7,5,22,156,0,50);
    // 完全相同的克隆条目 -> 应直接给
    AddItem(50000,"测试之剑",2,7,4,21,200,0,100);
    AddItem(50001,"测试之剑",2,7,4,21,200,0,100);
    // 同名但装等不同 -> 应列候选，新版排前
    AddItem(60000,"符文之剑",2,7,3,21,100,0,20);
    AddItem(60001,"符文之剑",2,7,5,21,264,0,500);
    AddItem(17182,"奥金斧",2,1,5,17,80,0,30);
    AddCre(299,"石爪豺狼人",10,0,111);
    AddCre(300,"石爪豺狼人",10,0,111);      // 完全相同
    AddCre(400,"狂暴豺狼人",25,1,222);
    ChatHandler h;
    auto RUN=[&](const char* label, bool(*fn)(ChatHandler*,char const*), const char* a){
        printf("\n=========== %s ===========\n", label);
        char buf[256]; strcpy(buf,a); fn(&h,buf);
    };
    RUN(".add 32837 (按ID)",        smartadd_commandscript::HandleSmartAddCommand, "32837");
    RUN(".add 埃辛诺斯战刃 (主副手有别)", smartadd_commandscript::HandleSmartAddCommand, "埃辛诺斯战刃");
    RUN(".add 测试之剑 (真重复)",    smartadd_commandscript::HandleSmartAddCommand, "测试之剑");
    RUN(".add 符文之剑 (装等不同)",  smartadd_commandscript::HandleSmartAddCommand, "符文之剑");
    RUN(".add 奥金斧 3 (带数量)",    smartadd_commandscript::HandleSmartAddCommand, "奥金斧 3");
    RUN(".add 测试之剑, 奥金斧 (批量)", smartadd_commandscript::HandleSmartAddCommand, "测试之剑, 奥金斧");
    RUN(".add 99999 (无效ID)",       smartadd_commandscript::HandleSmartAddCommand, "99999");
    RUN(".spawn 299 (按ID)",         smartadd_commandscript::HandleSmartSpawnCommand, "299");
    RUN(".spawn 石爪豺狼人 x5",      smartadd_commandscript::HandleSmartSpawnCommand, "石爪豺狼人 x5");
    RUN(".spawn 豺狼人 (有差异)",    smartadd_commandscript::HandleSmartSpawnCommand, "豺狼人");
    return 0;
}

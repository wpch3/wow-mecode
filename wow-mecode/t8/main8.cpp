#include "mock.h"
ObjectMgrMock* sObjectMgr = new ObjectMgrMock();
#include "body8.cpp"
using namespace SmartAdd;
int main(){
    // 造 137 件同名不同属性的物品 -> 强制分页
    for (uint32 i=0;i<137;++i){
        ItemTemplate t; t.ItemId=70000+i; t.Name1="测试剑"+std::to_string(i);
        t.Class=2; t.SubClass=7; t.Quality=(i%6); t.InventoryType=21;
        t.ItemLevel=100+i; t.StatsCount=1; t.ItemStat[0].ItemStatValue=int32(i*10);
        sObjectMgr->items[70000+i]=t;
    }
    ChatHandler h;
    Player* p = h.GetSession()->GetPlayer();

    printf("========= 分页容量验证 =========\n");
    std::vector<uint32> all;
    for (auto&kv : sObjectMgr->items) all.push_back(kv.first);
    printf("结果总数: %zu\n", all.size());

    StartPicker(p, &h, PICKER_ITEM, all, 1, "测试剑");
    auto& ss = s_sessions[1];
    printf("总页数: %u  每页: %u\n\n", ss.TotalPages(), ITEMS_PER_PAGE);

    // 遍历所有页，验证每页都不超 32
    uint32 maxSeen = 0; bool ok = true;
    for (uint32 pg = 0; pg < ss.TotalPages(); ++pg) {
        ss.page = pg;
        p->PlayerTalkClass->ClearMenus();
        // 复刻渲染逻辑计数
        uint32 start = pg*ITEMS_PER_PAGE;
        uint32 end = std::min(start+ITEMS_PER_PAGE, uint32(ss.results.size()));
        uint32 cnt = (end-start) + (pg>0?1u:0u) + (pg+1<ss.TotalPages()?1u:0u) + 1u;
        if (cnt > maxSeen) maxSeen = cnt;
        if (cnt > GOSSIP_HARD_LIMIT) { printf("!!! 第%u页 %u项 超限\n", pg+1, cnt); ok=false; }
    }
    printf("所有 %u 页中，单页最大项数 = %u  (硬上限 %u)\n", ss.TotalPages(), maxSeen, GOSSIP_HARD_LIMIT);
    printf("结果: %s\n\n", ok ? "[通过] 永不超限" : "[失败] 会崩服");

    printf("========= 首页实际渲染 =========\n");
    ss.page = 0;
    SendPickerMenu(p, &h);

    printf("\n========= 末页渲染（第%u页）=========\n", ss.TotalPages());
    ss.page = ss.TotalPages()-1;
    SendPickerMenu(p, &h);
    return 0;
}

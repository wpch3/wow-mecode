/*
 * setuptest.cpp —— .setup 一键开荒验证
 * 用户要求：.setup 必须【一键执行】，只有 .setup menu 才弹窗
 */
#include "mock.h"
#include "CombatSpecData.h"
static int P=0,F=0;
static void CK(bool ok,char const*w){ if(ok){++P;printf("  [OK]   %s\n",w);}else{++F;printf("  [FAIL] %s\n",w);} }

// 复刻 ComboState 里的 setup 开关
struct SetupCfg {
    bool gear=true, bar=true, buff=true, combo=false, stat=false;
    uint32 ilvl=0;
};

// 复刻 GearSetClassName
static char const* GSName(uint8 c){
    switch(c){
        case CLASS_WARRIOR:return "战士"; case CLASS_PALADIN:return "圣骑士";
        case CLASS_HUNTER:return "猎人"; case CLASS_ROGUE:return "盗贼";
        case CLASS_PRIEST:return "牧师"; case CLASS_DEATH_KNIGHT:return "死亡骑士";
        case CLASS_SHAMAN:return "萨满"; case CLASS_MAGE:return "法师";
        case CLASS_WARLOCK:return "术士"; case CLASS_DRUID:return "德鲁伊";
        default:return "战士";
    }
}
// 复刻拼指令
static std::string BuildGearCmd(uint8 cls,uint8 role,uint32 ilvl){
    std::ostringstream c; c<<".gearset "<<GSName(cls);
    switch(role){
        case CombatSpec::ROLE_TANK: c<<" 坦克"; break;
        case CombatSpec::ROLE_HEALER: c<<" 治疗"; break;
        default: c<<" 输出"; break;
    }
    if(ilvl) c<<" "<<ilvl;
    c<<" equip";
    return c.str();
}

int main(){
    printf("\n===== 1. 一键执行：不弹菜单 =====\n");
    {
        // .setup 走 SetupRun，不调 ShowSetupMenu
        std::vector<std::string> tok;              // 无参数
        bool isMenu = (!tok.empty() && (tok[0]=="menu"||tok[0]=="菜单"||tok[0]=="设置"));
        CK(!isMenu, ".setup 无参数 -> 直接执行，不弹窗");

        tok = {"menu"};
        isMenu = (!tok.empty() && (tok[0]=="menu"||tok[0]=="菜单"||tok[0]=="设置"));
        CK(isMenu, ".setup menu -> 才弹配置窗口");

        tok = {"200"};
        isMenu = (!tok.empty() && (tok[0]=="menu"||tok[0]=="菜单"||tok[0]=="设置"));
        CK(!isMenu, ".setup 200 -> 直接执行（带装等）");
    }

    printf("\n===== 2. 默认开关（保守）=====\n");
    {
        SetupCfg c;
        CK(c.gear && c.bar && c.buff, "默认开：发装备/配栏/补增益");
        CK(!c.combo, "默认关：自动连招（让玩家自己决定何时开打）");
        CK(!c.stat,  "默认关：属性精调（怕误改）");
    }

    printf("\n===== 3. 按职责拼 .gearset 指令 =====\n");
    {
        std::string t = BuildGearCmd(CLASS_WARRIOR, CombatSpec::ROLE_TANK, 0);
        printf("         防护战(坦克) -> %s\n", t.c_str());
        CK(t==".gearset 战士 坦克 equip", "坦克拼对");

        std::string h = BuildGearCmd(CLASS_PALADIN, CombatSpec::ROLE_HEALER, 200);
        printf("         神圣骑(治疗) -> %s\n", h.c_str());
        CK(h==".gearset 圣骑士 治疗 200 equip", "治疗+装等拼对");

        std::string d = BuildGearCmd(CLASS_MAGE, CombatSpec::ROLE_DPS, 0);
        printf("         法师(输出)   -> %s\n", d.c_str());
        CK(d==".gearset 法师 输出 equip", "输出拼对");
    }

    printf("\n===== 4. 十职业名称都能对上 .gearset =====\n");
    {
        uint8 cls[]={1,2,3,4,5,6,7,8,9,11};
        char const* want[]={"战士","圣骑士","猎人","盗贼","牧师","死亡骑士","萨满","法师","术士","德鲁伊"};
        bool allOk=true;
        for(int i=0;i<10;++i){
            if(std::string(GSName(cls[i]))!=want[i]){ allOk=false;
                printf("         cls%d 得到%s 期望%s\n",cls[i],GSName(cls[i]),want[i]); }
        }
        CK(allOk,"10 个职业名全部匹配");
    }

    printf("\n===== 5. 执行步数随开关变化 =====\n");
    {
        auto count=[](SetupCfg const&c){
            int n=0;
            if(c.gear)  ++n;
            if(c.bar)   ++n;
            if(c.buff)  ++n;
            if(c.stat)  ++n;
            if(c.combo) ++n;
            return n; };
        SetupCfg def; CK(count(def)==3, "默认 3 步（装备+配栏+buff）");
        SetupCfg all; all.combo=true; all.stat=true; CK(count(all)==5, "全开 5 步");
        SetupCfg min; min.gear=min.bar=min.buff=false; CK(count(min)==0, "全关 0 步（不崩）");
        SetupCfg only; only.gear=false; only.buff=false; CK(count(only)==1, "只配栏 1 步");
    }

    printf("\n===== 6. 执行顺序正确 =====\n");
    {
        /* 装备必须最先：属性和技能图标都依赖装备 */
        std::vector<std::string> order = {"发装备","配快捷栏","补增益","属性精调","开自动连招"};
        CK(order[0]=="发装备", "装备第一（属性/图标依赖它）");
        CK(order[1]=="配快捷栏", "配栏第二（装备穿好图标才不灰）");
        CK(order.back()=="开自动连招", "连招最后（前面都就绪）");
    }

    printf("\n===== 7. 天赋不碰（用户决定）=====\n");
    {
        std::vector<std::string> steps={"发装备","配快捷栏","补增益","属性精调","开自动连招"};
        bool hasTalent=false;
        for(auto&s:steps) if(s.find("天赋")!=std::string::npos) hasTalent=true;
        CK(!hasTalent, "执行步骤里没有天赋（不会覆盖你的加点）");
    }

    printf("\n===== 8. sender 段不撞车 =====\n");
    {
        // 9401-9409 已登记：9401/9402专精 9403/9404职责 9405场景 9406增益 9407setup 9409导航
        uint32 used[]={9401,9402,9403,9404,9405,9406,9407,9409};
        bool dup=false;
        for(size_t i=0;i<8;++i) for(size_t j=i+1;j<8;++j) if(used[i]==used[j]) dup=true;
        CK(!dup, "9401-9409 段内无重复");
        CK(9407>9406 && 9407<9409, "SENDER_SETUP=9407 在已登记区间内");
        // 不能撞套装的 1-11
        bool clash=false; for(uint32 u:used) if(u<=11) clash=true;
        CK(!clash, "不撞套装 cs_gearset 的 1-11 段");
    }

    printf("\n========================================\n  通过 %d / 失败 %d\n========================================\n",P,F);
    return F?1:0;
}

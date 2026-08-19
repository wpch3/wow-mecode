#include <cstdio>
#include <cstdint>
// 复刻 Player::GetHealthBonusFromStamina() + UpdateMaxHealth()
uint32_t calc(double stam, double baseHP){
    double base = (20.0 > stam) ? stam : 20.0;
    double bonus = base + (stam - base) * 10.0;
    double v = baseHP + bonus;
    return (uint32_t)v;
}
int main(){
    printf("=== 验证用户报告的 1.2B 血量 ===\n");
    printf("装备耐力 = 100,000,000 (1亿)\n\n");
    uint32_t hp = calc(100000000.0, 20000.0);
    printf("  耐力转血量: 20 + (1亿-20)*10 = %.0f\n", 20.0+(100000000.0-20.0)*10.0);
    printf("  加基础血量后 (uint32): %u\n", hp);
    printf("  换算: %.2f B (十亿)\n\n", hp/1e9);
    printf("用户报告: 1.2B  -> %s\n\n",
        (hp/1e9 > 0.9 && hp/1e9 < 1.3) ? "吻合 (含角色自身耐力/BUFF加成)" : "不符");
    printf("=== 安全边界: uint32 上限 4,294,967,295 (约4.29B) ===\n");
    double stams[] = {1e8, 2e8, 4e8, 4.3e8, 5e8, 1e9};
    for(double s: stams){
        double raw = 20.0 + (s-20.0)*10.0;
        printf("  耐力 %-12.0f -> 血量 %-14.0f %s\n", s, raw,
            raw < 4294967295.0 ? "安全" : "*** 溢出! 会回绕成小数字或0 ***");
    }
    return 0;
}

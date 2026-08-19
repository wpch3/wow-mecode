#include <cstdio>
int main(){
    const int MAXITEM = 32;
    const int NAV = 3;
    const int PER = MAXITEM - NAV;
    printf("Gossip 硬上限: %d 项/页 (超过 ASSERT 崩服)\n", MAXITEM);
    printf("预留导航按钮: %d 个 (上一页/下一页/取消)\n", NAV);
    printf("每页实际结果: %d 条\n\n", PER);
    int pages[] = {1,2,5,10,20,50,100};
    for (int i = 0; i < 7; ++i)
        printf("  %3d 页 -> 可容纳 %5d 条结果\n", pages[i], pages[i]*PER);
    printf("\n用户要求 50 页 -> %d 条\n", 50*PER);
    printf("参考: 3.3.5 物品总数约 38000\n");
    return 0;
}

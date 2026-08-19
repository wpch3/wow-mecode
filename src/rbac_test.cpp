#include <cstdio>
// 复刻用户的 enum 片段，验证语法
enum RBACPermissions
{
    RBAC_PERM_COMMAND_NPCBOT_SEND                            = 70037,

    //End NPCBot


    RBAC_PERM_COMMAND_MODIFY_ALLSTATS                        = 71001,

    RBAC_PERM_COMMAND_MODIFY_STAT                            = 71002,

    RBAC_PERM_MAX
};
int main(){
    printf("NPCBOT_SEND    = %d\n", RBAC_PERM_COMMAND_NPCBOT_SEND);
    printf("MODIFY_ALLSTATS= %d\n", RBAC_PERM_COMMAND_MODIFY_ALLSTATS);
    printf("MODIFY_STAT    = %d\n", RBAC_PERM_COMMAND_MODIFY_STAT);
    printf("RBAC_PERM_MAX  = %d  <- 自动递增为 71003\n", RBAC_PERM_MAX);
    return 0;
}

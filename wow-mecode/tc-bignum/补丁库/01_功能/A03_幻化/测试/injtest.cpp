#include <string>
#include <cstdio>
bool IsValidSetName(std::string const& name)
{
    if (name.empty() || name.length() > 32) return false;
    for (char c : name)
        if (c=='\''||c=='"'||c=='`'||c=='\\'||c==';'||c=='-'||c=='#'||c=='\n'||c=='\r')
            return false;
    return true;
}
int p=0,f=0;
void CK(bool c,char const*m){ if(c){++p;printf("  [OK] %s\n",m);} else {++f;printf("  [!!] %s <<<<\n",m);} }
int main(){
    printf("=== SQL 注入防护 ===\n");
    CK(!IsValidSetName("'; DROP TABLE characters; --"), "经典注入 -> 拒绝");
    CK(!IsValidSetName("a' OR '1'='1"),                 "OR 注入 -> 拒绝");
    CK(!IsValidSetName("test--comment"),                "SQL 注释 -- -> 拒绝");
    CK(!IsValidSetName("test#comment"),                 "MySQL 注释 # -> 拒绝");
    CK(!IsValidSetName("a\\'b"),                        "反斜杠转义 -> 拒绝");
    CK(!IsValidSetName("a;b"),                          "分号 -> 拒绝");
    CK(!IsValidSetName("`table`"),                      "反引号 -> 拒绝");
    CK(!IsValidSetName("a\"b"),                         "双引号 -> 拒绝");
    CK(!IsValidSetName("a\nb"),                         "换行 -> 拒绝");
    CK(!IsValidSetName(""),                             "空串 -> 拒绝");
    CK(!IsValidSetName(std::string(33,'a')),            "超长33字符 -> 拒绝");
    printf("\n=== 正常名字要能过 ===\n");
    CK(IsValidSetName("战斗套"),        "中文 战斗套 -> 通过");
    CK(IsValidSetName("震金套"),        "中文 震金套 -> 通过");
    CK(IsValidSetName("pvp"),           "英文 pvp -> 通过");
    CK(IsValidSetName("套装1"),         "中英数混合 -> 通过");
    CK(IsValidSetName("T6_Set"),        "下划线 -> 通过");
    CK(IsValidSetName(std::string(32,'a')), "刚好32字符 -> 通过");
    printf("\n通过 %d / 失败 %d\n",p,f);
    return f?1:0;
}

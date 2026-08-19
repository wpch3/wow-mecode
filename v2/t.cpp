#include <cstdio>
#include <cstdint>
// 复刻 TrinityCore FieldValueConverters.h 的 GetNumericValue<T>()
template<typename DBType, typename T>
bool WouldAssert(DBType src){ T r=static_cast<T>(src); return static_cast<DBType>(r)!=src; }
int main(){
  printf("=== 你的库现状: armor 列是 int(有符号), C++ 用 GetUInt32() 读 ===\n");
  int32_t vals[]={0, 9262, 1000000000, 2147483647};
  for(auto v: vals)
    printf("  armor=%-12d -> assert? %s\n", v,
      WouldAssert<int32_t,uint32_t>(v) ? "YES 崩溃" : "no  安全");
  printf("\n=== 关键: 若列是 int 但 C++ 还用旧的 GetInt16() 读 ===\n");
  int32_t big[]={1709, 32767, 32768, 100000};
  for(auto v: big)
    printf("  stat_value=%-8d -> assert? %s\n", v,
      WouldAssert<int32_t,int16_t>(v) ? "YES 崩溃!" : "no  安全");
  return 0;
}

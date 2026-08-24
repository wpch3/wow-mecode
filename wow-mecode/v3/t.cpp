#include <cstdio>
#include <cstdint>
template<typename DB,typename T> bool A(DB s){T r=(T)s;return (DB)r!=s;}
int main(){
  printf("=== 你的库现在有一件 stat_value1=100000000 的物品 ===\n\n");
  printf("[旧 exe] GetInt16() 读 100000000 -> %s\n",
         A<int32_t,int16_t>(100000000)?"ASSERT 触发 -> worldserver 启动即崩溃":"安全");
  printf("[新 exe] GetInt32() 读 100000000 -> %s\n",
         A<int32_t,int32_t>(100000000)?"崩溃":"安全，正常加载");
  printf("\n=== 抗性列（原 tinyint，现 int）holy_res=500000000 ===\n");
  printf("[旧 exe] GetUInt8()  读 500000000 -> %s\n",
         A<int32_t,uint8_t>(500000000)?"ASSERT 触发 -> 崩溃":"安全");
  printf("[新 exe] GetUInt32() 读 500000000 -> %s\n",
         A<int32_t,uint32_t>(500000000)?"崩溃":"安全");
  printf("\n=== 结论：数据已就位，必须用新 exe，旧 exe 会在加载物品表时 abort ===\n");
  return 0;
}

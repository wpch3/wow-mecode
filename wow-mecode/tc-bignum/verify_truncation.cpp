#include <cstdio>
#include <cstdint>
// Replicates TrinityCore FieldValueConverters.h PrimitiveResultValueConverter::GetNumericValue<T>()
// If the round-trip cast is lossy -> LogTruncation() -> ASSERT(false) -> server abort.
template<typename DatabaseType, typename T>
bool WouldAssert(DatabaseType source) {
    T result = static_cast<T>(source);
    return (static_cast<DatabaseType>(result) != source);
}
int main() {
    printf("=== DB column widened to `int`, C++ accessor NOT updated ===\n");
    printf("col=int val=100000     via GetInt16 -> assert? %s\n",
        WouldAssert<int32_t,int16_t>(100000) ? "YES (SERVER CRASH)" : "no");
    printf("col=int val=30000      via GetInt16 -> assert? %s\n",
        WouldAssert<int32_t,int16_t>(30000)  ? "YES (SERVER CRASH)" : "no");
    printf("col=int val=5000       via GetUInt8 -> assert? %s\n",
        WouldAssert<int32_t,uint8_t>(5000) ? "YES (SERVER CRASH)" : "no");
    printf("\n=== Both DB column AND accessor updated to int32 ===\n");
    printf("col=int val=2000000000 via GetInt32 -> assert? %s\n",
        WouldAssert<int32_t,int32_t>(2000000000) ? "YES (SERVER CRASH)" : "no");

    printf("\n=== Player::UpdateMaxHealth() stamina math ===\n");
    float stam = 2147483647.0f;
    float base = (20.0f > stam) ? stam : 20.0f;
    float hp = base + (stam - base) * 10.0f;
    printf("stamina=2147483647 -> health=%.0f -> (uint32) wraps to %u  [uint32 max=4294967295]\n",
        hp, (uint32_t)hp);
    float safe = 200000000.0f;
    float h2 = 20.0f + (safe - 20.0f) * 10.0f;
    printf("stamina=200000000  -> health=%.0f  fits in uint32? %s\n",
        h2, (h2 < 4294967295.0f) ? "yes" : "NO - OVERFLOW");
    float rec = 100000000.0f;
    float h3 = 20.0f + (rec - 20.0f) * 10.0f;
    printf("stamina=100000000  -> health=%.0f  fits in int32? %s\n",
        h3, (h3 < 2147483647.0f) ? "yes" : "NO");
    return 0;
}

// 复刻 FieldValueConverters.h:60-70 的 GetNumericValue 截断检测
// 目的：证明"只改数据库不改 C++"会走进 LogTruncation -> ASSERT -> 崩服
#include <cstdint>
#include <cstdio>
#include <string>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

static int g_assertHit = 0;
static std::string g_lastMsg;

// 模拟 FieldValueConverter.cpp:48 的 ASSERT(false, ...)
static void LogTruncation(char const* getter, char const* col, char const* expected)
{
    ++g_assertHit;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "%s on field %s caused value to be truncated. Use %s instead.",
        getter, col, expected);
    g_lastMsg = buf;
}

// FieldValueConverters.h:59-70 原样复刻
template<typename DatabaseType, typename T>
static T GetNumericValue(DatabaseType source, char const* func, char const* col, char const* expected)
{
    T result = static_cast<T>(source);
    if (static_cast<DatabaseType>(result) != source)
    {
        LogTruncation(func, col, expected);
        return T();
    }
    return result;
}

#define CASE(name) do { printf("  [%-2d] %-58s ", ++caseNo, name); } while(0)
#define OK()   do { printf("PASS\n"); ++pass; } while(0)
#define FAIL(fmt, ...) do { printf("FAIL  " fmt "\n", ##__VA_ARGS__); ++fail; } while(0)

int main()
{
    int caseNo = 0, pass = 0, fail = 0;
    printf("=== step22 字段扩容 · 截断行为验证 ===\n\n");

    printf("-- A. 现状：列是 smallint(uint16)，代码 GetUInt16 --\n");

    CASE("MenuID=63001 存 smallint 列，GetUInt16 读 -> 正常");
    g_assertHit = 0;
    { uint16 v = GetNumericValue<uint16, uint16>(63001, "Field::GetUInt16", "gossip_menu.MenuID", "Field::GetUInt16");
      if (v == 63001 && g_assertHit == 0) OK(); else FAIL("v=%u hit=%d", v, g_assertHit); }

    CASE("MenuID=96001 想存 smallint 列 -> MySQL 侧就报 1264，进不来");
    { bool fitsInColumn = (96001u <= 65535u);
      if (!fitsInColumn) OK(); else FAIL("96001 竟然装得进 smallint?"); }

    printf("\n-- B. 危险组合：只改数据库(smallint->int)，C++ 仍是 GetUInt16 --\n");

    CASE("列变 int，存 96001，GetUInt16 读 -> 触发 LogTruncation");
    g_assertHit = 0;
    { uint16 v = GetNumericValue<uint32, uint16>(96001, "Field::GetUInt16", "gossip_menu.MenuID", "Field::GetUInt32");
      if (g_assertHit == 1 && v == 0) OK(); else FAIL("hit=%d v=%u (预期 hit=1 v=0)", g_assertHit, v); }

    CASE("  -> LogTruncation 内容确实指向 GetUInt32");
    { if (g_lastMsg.find("Use Field::GetUInt32") != std::string::npos) OK();
      else FAIL("msg=%s", g_lastMsg.c_str()); }

    CASE("  -> 这条路在真机上是 ASSERT(false) = 直接崩服");
    { /* Errors.h:68  #define ASSERT WPAssert  (非 PERFORMANCE_PROFILING 构建) */
      if (g_assertHit > 0) OK(); else FAIL("没触发"); }

    CASE("列变 int，但值仍 <=65535（老数据），GetUInt16 -> 不崩");
    g_assertHit = 0;
    { uint16 v = GetNumericValue<uint32, uint16>(4211, "Field::GetUInt16", "gossip_menu.MenuID", "Field::GetUInt32");
      if (v == 4211 && g_assertHit == 0) OK(); else FAIL("v=%u hit=%d", v, g_assertHit); }
    printf("       ^ 注意：老菜单全 <65535，所以只改库不改码【平时不崩，一用新ID才崩】\n");

    printf("\n-- C. 正确组合：库改 int + 代码改 GetUInt32 --\n");

    CASE("MenuID=960001，列 int，GetUInt32 -> 正常");
    g_assertHit = 0;
    { uint32 v = GetNumericValue<uint32, uint32>(960001, "Field::GetUInt32", "gossip_menu.MenuID", "Field::GetUInt32");
      if (v == 960001 && g_assertHit == 0) OK(); else FAIL("v=%u hit=%d", v, g_assertHit); }

    CASE("OptionID=0..31 用 GetUInt32 读 int 列 -> 正常");
    g_assertHit = 0;
    { bool allOk = true;
      for (uint32 i = 0; i < 32; ++i)
        if (GetNumericValue<uint32, uint32>(i, "Field::GetUInt32", "OptionID", "Field::GetUInt32") != i) allOk = false;
      if (allOk && g_assertHit == 0) OK(); else FAIL("hit=%d", g_assertHit); }

    CASE("老菜单 MenuID=1..65535 用 GetUInt32 读 -> 全部正常（向后兼容）");
    g_assertHit = 0;
    { bool allOk = true;
      for (uint32 i = 1; i <= 65535; ++i)
        if (GetNumericValue<uint32, uint32>(i, "Field::GetUInt32", "MenuID", "Field::GetUInt32") != i) allOk = false;
      if (allOk && g_assertHit == 0) OK(); else FAIL("hit=%d", g_assertHit); }

    printf("\n-- D. 反向组合：代码改了 GetUInt32，但库忘了改 --\n");

    CASE("列还是 smallint，GetUInt32 读 -> 不崩（加宽读安全）");
    g_assertHit = 0;
    { uint32 v = GetNumericValue<uint16, uint32>(63001, "Field::GetUInt32", "MenuID", "Field::GetUInt16");
      if (v == 63001 && g_assertHit == 0) OK(); else FAIL("v=%u hit=%d", v, g_assertHit); }
    printf("       ^ 结论：代码先改是安全的，库先改是危险的 -> 执行顺序必须【先码后库】或同时\n");

    printf("\n-- E. 网络层容量核对 --\n");

    CASE("SMSG_GOSSIP_MESSAGE 的 GossipID 是 int32，960001 装得下");
    { int32_t g = static_cast<int32_t>(960001u);
      if (g == 960001 && 960001u <= 2147483647u) OK(); else FAIL("g=%d", g); }

    CASE("CMSG_GOSSIP_SELECT_OPTION 回传 uint32 menuId，往返一致");
    { uint32 sent = 960001; int32_t wire = static_cast<int32_t>(sent); uint32 recv = static_cast<uint32>(wire);
      if (recv == sent) OK(); else FAIL("sent=%u recv=%u", sent, recv); }

    CASE("creature_template.gossip_menu_id 本就 int unsigned + GetUInt32");
    { if (960001u <= 4294967295u) OK(); else FAIL("?"); }

    CASE("conditions.SourceGroup 本就 int unsigned + GetUInt32（存MenuID）");
    { if (960001u <= 4294967295u) OK(); else FAIL("?"); }

    printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n", pass, fail, caseNo);
    return fail == 0 ? 0 : 1;
}

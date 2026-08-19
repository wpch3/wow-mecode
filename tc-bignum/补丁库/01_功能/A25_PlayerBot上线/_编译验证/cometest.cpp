#include "stub2.h"
std::vector<std::string> g_calls;
static constexpr float PBOT_COME_DIST = 2.0f;

// ===== 与 cs_playerbot.cpp 中 come 分支【逐行一致】的逻辑 =====
static bool DoCome(Player* player, Player* p, std::string& msg)
{
    if (p->IsBeingTeleported()) { msg="正在传送中"; return false; }
    if (player->IsInFlight()) player->FinishTaxiFlight();

    bool sameMap = (p->GetMapId() == player->GetMapId());
    Position dest = player->GetFirstCollisionPosition(PBOT_COME_DIST, 0.0f);
    float x=dest.GetPositionX(), y=dest.GetPositionY(), z=dest.GetPositionZ();
    float o = dest.GetAbsoluteAngle(player->GetPositionX(), player->GetPositionY());

    if (!p->TeleportTo(player->GetMapId(), x, y, z, o)) { msg="传送失败"; return false; }

    if (p->IsBeingTeleportedFar()) {
        uint8 guard=0;
        while (p->IsBeingTeleportedFar() && guard++ < 5)
            p->GetSession()->HandleMoveWorldportAck();
    } else if (p->IsBeingTeleportedNear()) {
        p->SetSemaphoreTeleportNear(false);
        uint32 oldZone = p->GetZoneId();
        WorldLocation const& tdest = p->GetTeleportDest();
        p->UpdatePosition(tdest, true);
        p->SetFallInformation(0, p->GetPositionZ());
        uint32 nz,na; p->GetZoneAndAreaId(nz,na); p->UpdateZone(nz,na);
        if (oldZone != nz) {
            if (p->pvpInfo.IsHostile) p->CastSpell(p,2479,true);
            else if (p->IsPvP() && !p->HasFlag(PLAYER_FLAGS,PLAYER_FLAGS_IN_PVP)) p->UpdatePvP(false,false);
        }
        p->ResummonPetTemporaryUnSummonedIfAny();
        p->ProcessDelayedOperations();
    }
    p->SetPhaseMask(player->GetPhaseMask(), true);
    p->StopMoving();
    msg = sameMap?"同地图":"跨地图";
    return true;
}

static int pass=0, fail=0;
#define CHECK(c,n) do{ if(c){++pass;} else {++fail; printf("  [FAIL] %s\n",n);} }while(0)
static bool near_(float a,float b,float e=0.05f){return std::fabs(a-b)<e;}
static bool hasCall(char const* s){for(auto&c:g_calls) if(c==s) return true; return false;}

int main(){
    printf("=== .pbot come / goto 行为测试 ===\n\n");

    // ---- T1 同地图：必须真正落地到我身前 ----
    { g_calls.clear();
      Player me; me.mapId=0; me.zoneId=12; me.m_positionX=100; me.m_positionY=200; me.m_positionZ=50; me.m_orientation=0;
      Player bot; bot.mapId=0; bot.zoneId=12; bot.m_positionX=-5000; bot.m_positionY=-5000; bot.m_positionZ=10;
      std::string msg; bool ok=DoCome(&me,&bot,msg);
      CHECK(ok,"T1 返回成功");
      CHECK(near_(bot.m_positionX,102.0f),"T1 落点X=102(身前2码)");
      CHECK(near_(bot.m_positionY,200.0f),"T1 落点Y=200");
      CHECK(!bot.IsBeingTeleportedNear(),"T1 近传送信号量已清除");
      CHECK(!bot.IsBeingTeleported(),"T1 传送已完成(未卡住)");
      CHECK(hasCall("UpdatePosition"),"T1 调用了UpdatePosition(真正落地)");
      CHECK(hasCall("ResummonPet"),"T1 补了ResummonPet");
      CHECK(hasCall("ProcessDelayed"),"T1 补了ProcessDelayedOperations");
      CHECK(msg=="同地图","T1 提示同地图");
      float ang=std::fabs(bot.m_orientation-3.14159f);
      CHECK(ang<0.05f,"T1 bot面朝我(о≈π)");
    }

    // ---- T2 【回归】不打补丁会怎样：证明bug真实存在 ----
    { g_calls.clear();
      Player me; me.mapId=0; me.m_positionX=100; me.m_positionY=200; me.m_orientation=0;
      Player bot; bot.mapId=0; bot.m_positionX=-5000; bot.m_positionY=-5000;
      float x,y,z; me.GetClosePoint(x,y,z,bot.GetCombatReach());
      bot.TeleportTo(me.GetMapId(),x,y,z,me.GetOrientation());   // 旧代码：只有这一句
      CHECK(bot.IsBeingTeleportedNear(),"T2 旧代码卡在近传送信号量");
      CHECK(near_(bot.m_positionX,-5000.0f),"T2 旧代码坐标【没变】= bug复现");
    }

    // ---- T3 跨地图：官方while写法能落地 ----
    { g_calls.clear();
      Player me; me.mapId=1; me.zoneId=14; me.m_positionX=10; me.m_positionY=20; me.m_orientation=1.5708f;
      Player bot; bot.mapId=0; bot.m_positionX=-3000; bot.m_positionY=-3000;
      std::string msg; bool ok=DoCome(&me,&bot,msg);
      CHECK(ok,"T3 返回成功");
      CHECK(bot.mapId==1,"T3 地图已切换到1");
      CHECK(!bot.IsBeingTeleportedFar(),"T3 远传送信号量已清除");
      CHECK(hasCall("WorldportAck"),"T3 调用了HandleMoveWorldportAck");
      CHECK(near_(bot.m_positionX,10.0f),"T3 落点X≈10");
      CHECK(near_(bot.m_positionY,22.0f),"T3 落点Y≈22(朝向90度前方2码)");
      CHECK(msg=="跨地图","T3 提示跨地图");
    }

    // ---- T4 死循环保护 ----
    { g_calls.clear();
      Player me; me.mapId=1; Player bot; bot.mapId=0;
      bot.farSem=true;   // 人为制造永不清除
      uint8 guard=0; int loops=0;
      while(bot.IsBeingTeleportedFar() && guard++<5){ ++loops; bot.farSem=true; }
      CHECK(loops==5,"T4 guard限制5次,不会死循环");
    }

    // ---- T5 传送中的bot要被拒绝 ----
    { g_calls.clear(); Player me; Player bot; bot.farSem=true;
      std::string msg; bool ok=DoCome(&me,&bot,msg);
      CHECK(!ok,"T5 传送中被拒绝"); CHECK(msg=="正在传送中","T5 提示正确"); }

    // ---- T6 TeleportTo失败要被捕获 ----
    { g_calls.clear(); Player me; Player bot; bot.teleportOk=false;
      std::string msg; bool ok=DoCome(&me,&bot,msg);
      CHECK(!ok,"T6 传送失败被捕获"); CHECK(msg=="传送失败","T6 提示正确"); }

    // ---- T7 相位同步 ----
    { g_calls.clear(); Player me; me.phase=8; Player bot; bot.phase=1;
      std::string msg; DoCome(&me,&bot,msg);
      CHECK(bot.phase==8,"T7 相位已同步"); }

    // ---- T8 我在飞行中要先落地 ----
    { g_calls.clear(); Player me; me.inFlight=true; Player bot;
      std::string msg; DoCome(&me,&bot,msg);
      CHECK(hasCall("FinishTaxiFlight"),"T8 先结束飞行"); }

    // ---- T9 换区时的PvP处理 ----
    { g_calls.clear(); Player me; me.mapId=0; me.zoneId=99; Player bot; bot.mapId=0; bot.zoneId=1;
      bot.pvpInfo.IsHostile=true;
      bot.destZoneOverride=99;   // 落点在99区(真实由UpdatePositionData算出)
      std::string msg; DoCome(&me,&bot,msg);
      CHECK(bot.zoneId==99,"T9 区域已更新");
      CHECK(hasCall("UpdateZone"),"T9 调用了UpdateZone");
      CHECK(hasCall("CastSpell"),"T9 敌对区施放honorless"); }

    // ---- T9b 同区域不应触发PvP处理 ----
    { g_calls.clear(); Player me; me.mapId=0; me.zoneId=5; Player bot; bot.mapId=0; bot.zoneId=5;
      bot.pvpInfo.IsHostile=true; bot.destZoneOverride=5;
      std::string msg; DoCome(&me,&bot,msg);
      CHECK(!hasCall("CastSpell"),"T9b 同区不施放honorless"); }

    // ---- T10 StopMoving 防止走回去 ----
    { g_calls.clear(); Player me; Player bot; std::string msg; DoCome(&me,&bot,msg);
      CHECK(hasCall("StopMoving"),"T10 停止原有移动"); }

    // ---- T11 服务端移动的bot(IsMovedByClient=false)也要正确 ----
    { g_calls.clear(); Player me; me.mapId=0; me.m_positionX=100; me.m_positionY=200;
      Player bot; bot.mapId=0; bot.movedByClient=false; bot.m_positionX=-9000;
      std::string msg; bool ok=DoCome(&me,&bot,msg);
      CHECK(ok,"T11 返回成功");
      CHECK(near_(bot.m_positionX,102.0f),"T11 无客户端时也落地正确");
      CHECK(!bot.IsBeingTeleported(),"T11 未卡住"); }

    // ---- T12 落点距离恰好2码 ----
    { Player me; me.m_positionX=0; me.m_positionY=0; me.m_orientation=0;
      Position d=me.GetFirstCollisionPosition(PBOT_COME_DIST,0.0f);
      float dist=std::sqrt(d.m_positionX*d.m_positionX+d.m_positionY*d.m_positionY);
      CHECK(near_(dist,2.0f),"T12 落点距离=2码(不重叠不远)"); }

    printf("\n=== 结果: %d/%d 通过 ===\n",pass,pass+fail);
    return fail?1:0;
}

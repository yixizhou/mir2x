/*
 * =====================================================================================
 *
 *       Filename: charobject.cpp
 *        Created: 04/07/2016 03:48:41 AM
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */
#include <cinttypes>
#include "motion.hpp"
#include "player.hpp"
#include "monster.hpp"
#include "mathfunc.hpp"
#include "actorpod.hpp"
#include "condcheck.hpp"
#include "monoserver.hpp"
#include "charobject.hpp"
#include "messagepack.hpp"
#include "protocoldef.hpp"
#include "eventtaskhub.hpp"

extern MonoServer *g_MonoServer;

CharObject::COPathFinder::COPathFinder(const CharObject *pCO, int nCheckCO)
    : AStarPathFinder([this](int nSrcX, int nSrcY, int nDstX, int nDstY) -> double
      {
          // we pass lambda to ctor of AStarPathFinder()
          // only capture *this*, this helps std::function to be not expensive

          if(0){
              if(true
                      && MaxStep() != 1
                      && MaxStep() != 2
                      && MaxStep() != 3){

                    g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid MaxStep provided: %d, should be (1, 2, 3)", MaxStep());
                    return 10000.00;
              }

              int nDistance2 = LDistance2(nSrcX, nSrcY, nDstX, nDstY);
              if(true
                      && nDistance2 != 1
                      && nDistance2 != 2
                      && nDistance2 != MaxStep() * MaxStep()
                      && nDistance2 != MaxStep() * MaxStep() * 2){

                  g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid step checked: (%d, %d) -> (%d, %d)", nSrcX, nSrcY, nDstX, nDstY);
                  return 10000.00;
              }
          }

          return m_CO->OneStepCost(this, m_CheckCO, nSrcX, nSrcY, nDstX, nDstY);
      }, pCO->MaxStep())
    , m_CO(pCO)
    , m_CheckCO(nCheckCO)
    , m_Cache()
{
    if(!m_CO){
        g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid argument: CO = %p, CheckCO = %d", m_CO, m_CheckCO);
    }

    switch(m_CheckCO){
        case 0:
        case 1:
        case 2:
            {
                break;
            }
        default:
            {
                g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid argument: CO = %p, CheckCO = %d", m_CO, m_CheckCO);
                break;
            }
    }

    switch(m_CO->MaxStep()){
        case 1:
        case 2:
        case 3:
            {
                break;
            }
        default:
            {
                g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid MaxStep provided: %d, should be (1, 2, 3)", m_CO->MaxStep());
                break;
            }
    }
}

int CharObject::COPathFinder::GetGrid(int nX, int nY) const
{
    if(!m_CO->GetServerMap()->ValidC(nX, nY)){
        return PathFind::INVALID;
    }

    uint32_t nKey = ((uint32_t)(nX) << 16) | (uint32_t)(nY);
    if(auto p = m_Cache.find(nKey); p != m_Cache.end()){
        return p->second;
    }

    auto nGrid = m_CO->CheckPathGrid(nX, nY);
    m_Cache[nKey] = nGrid;
    return nGrid;
}

CharObject::CharObject(ServiceCore *pServiceCore,
        ServerMap                  *pServerMap,
        uint64_t                    nUID,
        int                         nMapX,
        int                         nMapY,
        int                         nDirection)
    : ServerObject(nUID)
    , m_ServiceCore(pServiceCore)
    , m_Map(pServerMap)
    , m_LocationList()
    , m_X(nMapX)
    , m_Y(nMapY)
    , m_Direction(nDirection)
    , m_HP(0)
    , m_HPMax(0)
    , m_MP(0)
    , m_MPMax(0)
    , m_MoveLock(false)
    , m_AttackLock(false)
    , m_LastMoveTime(0)
    , m_LastAttackTime(0)
    , m_HitterUIDRecord()
    , m_TargetQueue()
    , m_Ability()
    , m_WAbility()
    , m_AddAbility()
{
    condcheck(m_Map);
    m_StateHook.Install("RemoveDeadUIDLocation", [this, nLastCheckTick = (uint32_t)(0)]() mutable -> bool
    {
        if(auto nCurrCheckTick = g_MonoServer->GetTimeTick(); nLastCheckTick + 5000 < nCurrCheckTick){
            if(ActorPodValid()){
                // remove all dead ones
                // dispatch action requires check location list
                DispatchAction(ActionStand(X(), Y(), Direction()));
            }
            nLastCheckTick = nCurrCheckTick;
        }
        return false;
    });
}

bool CharObject::NextLocation(int *pX, int *pY, int nDirection, int nDistance)
{
    int nDX = 0;
    int nDY = 0;
    switch(nDirection){
        case DIR_UP:        { nDX =  0; nDY = -1; break; }
        case DIR_UPRIGHT:   { nDX = +1; nDY = -1; break; }
        case DIR_RIGHT:     { nDX = +1; nDY =  0; break; }
        case DIR_DOWNRIGHT: { nDX = +1; nDY = +1; break; }
        case DIR_DOWN:      { nDX =  0; nDY = +1; break; }
        case DIR_DOWNLEFT:  { nDX = -1; nDY = +1; break; }
        case DIR_LEFT:      { nDX = -1; nDY =  0; break; }
        case DIR_UPLEFT:    { nDX = -1; nDY = -1; break; }
        default:            { return false;              }
    }

    if(pX){ *pX = X() + (nDX * nDistance); }
    if(pY){ *pY = Y() + (nDY * nDistance); }

    return true;
}

uint64_t CharObject::Activate()
{
    if(auto nUID = ServerObject::Activate(); nUID){
        DispatchAction(ActionSpawn(X(), Y(), Direction()));
        return nUID;
    }
    return 0;
}

void CharObject::ReportAction(uint64_t, const ActionNode &)
{
}

void CharObject::DispatchAction(const ActionNode &rstAction)
{
    // should check to avoid dead CO call this function
    // this would cause zombies

    if(!ActorPodValid()){
        g_MonoServer->AddLog(LOGTYPE_WARNING, "Can't dispatch action: %s", rstAction.ActionName());
        return;
    }

    AMAction stAMA;
    std::memset(&stAMA, 0, sizeof(stAMA));

    stAMA.UID   = UID();
    stAMA.MapID = MapID();

    stAMA.Action    = rstAction.Action;
    stAMA.Speed     = rstAction.Speed;
    stAMA.Direction = rstAction.Direction;

    stAMA.X    = rstAction.X;
    stAMA.Y    = rstAction.Y;
    stAMA.AimX = rstAction.AimX;
    stAMA.AimY = rstAction.AimY;

    stAMA.AimUID      = rstAction.AimUID;
    stAMA.ActionParam = rstAction.ActionParam;

    switch(stAMA.Action){
        case ACTION_MOVE:
        case ACTION_PUSHMOVE:
        case ACTION_SPACEMOVE1:
        case ACTION_SPACEMOVE2:
        case ACTION_SPAWN:
            {
                m_ActorPod->Forward(m_Map->UID(), {MPK_ACTION, stAMA});
                return;
            }
        default:
            {
                for(auto pLoc = m_LocationList.begin(); pLoc != m_LocationList.end();){
                    auto pNext  = std::next(pLoc);
                    auto nX     = pLoc->second.X;
                    auto nY     = pLoc->second.Y;
                    auto nMapID = pLoc->second.MapID;

                    if(m_Map->In(nMapID, nX, nY) && LDistance2(nX, nY, X(), Y()) < 10 * 10){
                        // if one co becomes my new neighbor
                        // is should be included in the list already
                        // but if we find a neighbor in the cache list we need to refresh it
                        RetrieveLocation(pLoc->first, [this, stAMA](const COLocation &rstLocation)
                        {
                            auto nX = rstLocation.X;
                            auto nY = rstLocation.Y;
                            auto nMapID = rstLocation.MapID;
                            if(m_Map->In(nMapID, nX, nY) && LDistance2(nX, nY, X(), Y()) < 100){
                                m_ActorPod->Forward(rstLocation.UID, {MPK_ACTION, stAMA});
                            }
                        });

                        // we need to keep pNext
                        // since RetrieveLocation may remove current node
                        pLoc = pNext;
                    }else{
                        pLoc = m_LocationList.erase(pLoc);
                    }
                }
                return;
            }
    }
}

void CharObject::DispatchAction(uint64_t nUID, const ActionNode &rstAction)
{
    // should check to avoid dead CO call this function
    // this would cause zombies

    condcheck(nUID);
    condcheck(ActorPodValid());

    AMAction stAMA;
    std::memset(&stAMA, 0, sizeof(stAMA));

    stAMA.UID   = UID();
    stAMA.MapID = MapID();

    stAMA.Action    = rstAction.Action;
    stAMA.Speed     = rstAction.Speed;
    stAMA.Direction = rstAction.Direction;

    stAMA.X    = rstAction.X;
    stAMA.Y    = rstAction.Y;
    stAMA.AimX = rstAction.AimX;
    stAMA.AimY = rstAction.AimY;

    stAMA.AimUID      = rstAction.AimUID;
    stAMA.ActionParam = rstAction.ActionParam;

    m_ActorPod->Forward(nUID, {MPK_ACTION, stAMA});
}

bool CharObject::RequestMove(int nX, int nY, int nSpeed, bool bAllowHalfMove, std::function<void()> fnOnMoveOK, std::function<void()> fnOnMoveError)
{
    if(!CanMove()){
        if(fnOnMoveError){
            fnOnMoveError();
        }
        return false;
    }

    if(EstimateHop(nX, nY) != 1){
        if(fnOnMoveError){
            fnOnMoveError();
        }
        return false;
    }

    switch(LDistance2(X(), Y(), nX, nY)){
        case 1:
        case 2:
            {
                switch(CheckPathGrid(nX, nY, 0)){
                    case PathFind::FREE:
                        {
                            break;
                        }
                    case PathFind::OCCUPIED:
                    default:
                        {
                            if(fnOnMoveError){
                                fnOnMoveError();
                            }
                            return false;
                        }
                }
                break;
            }
        default:
            {
                // one-hop distance but has internal steps
                // need to check if there is co blocking the path
                int nXm = -1;
                int nYm = -1;
                PathFind::GetFrontLocation(&nXm, &nYm, X(), Y(), PathFind::GetDirection(X(), Y(), nX, nY), 1);

                // for strict co check
                // need to skip the current (X(), Y())

                if(OneStepCost(nullptr, 2, nXm, nYm, nX, nY) < 0.00){
                    if(!bAllowHalfMove){
                        if(fnOnMoveError){
                            fnOnMoveError();
                        }
                        return false;
                    }
                }
                break;
            }
    }

    AMTryMove stAMTM;
    std::memset(&stAMTM, 0, sizeof(stAMTM));

    stAMTM.UID           = UID();
    stAMTM.MapID         = MapID();
    stAMTM.X             = X();
    stAMTM.Y             = Y();
    stAMTM.EndX          = nX;
    stAMTM.EndY          = nY;
    stAMTM.AllowHalfMove = bAllowHalfMove;

    m_MoveLock = true;
    auto bForwardOK = m_ActorPod->Forward(m_Map->UID(), {MPK_TRYMOVE, stAMTM}, [this, nX, nY, nSpeed, fnOnMoveOK, fnOnMoveError](const MessagePack &rstMPK)
    {
        if(!m_MoveLock){
            g_MonoServer->AddLog(LOGTYPE_WARNING, "MoveLock released before map responds: ClassName = %s", UIDName());
            g_MonoServer->Restart();
        }

        // 1. release the move lock no matter what kind of message we get
        m_MoveLock = false;

        // 2. handle move, CO may be dead
        //    need to check if current CO can move

        auto nDir = PathFind::GetDirection(X(), Y(), nX, nY);
        switch(rstMPK.Type()){
            case MPK_MOVEOK:
                {
                    AMMoveOK stAMMOK;
                    std::memcpy(&stAMMOK, rstMPK.Data(), sizeof(stAMMOK));

                    // since we may allow half move
                    // servermap permitted dst may not be (nX, nY)

                    if(true
                            && CanMove()
                            && m_Map->ValidC(stAMMOK.EndX, stAMMOK.EndY)){

                        auto nOldX = m_X;
                        auto nOldY = m_Y;

                        m_X = stAMMOK.EndX;
                        m_Y = stAMMOK.EndY;

                        m_Direction = nDir;

                        m_LastMoveTime = g_MonoServer->GetTimeTick();

                        m_ActorPod->Forward(rstMPK.From(), MPK_OK, rstMPK.ID());
                        DispatchAction(ActionMove(nOldX, nOldY, X(), Y(), nSpeed, Horse()));

                        if(fnOnMoveOK){
                            fnOnMoveOK();
                        }
                    }else{
                        m_ActorPod->Forward(rstMPK.From(), MPK_ERROR, rstMPK.ID());
                        if(fnOnMoveError){
                            fnOnMoveError();
                        }
                    }

                    break;
                }
            case MPK_ERROR:
                {
                    // should add a new function: CanTurn()
                    // for stone state we can't even make a turn

                    if(CanMove() && m_Direction != nDir){
                        m_Direction = nDir;
                        DispatchAction(ActionStand(X(), Y(), Direction()));
                    }

                    if(fnOnMoveError){
                        fnOnMoveError();
                    }

                    break;
                }
            default:
                {
                    break;
                }
        }
    });

    // send failure should trigger fnOnMoveError
    // but a successful send doesn't mean move successfully

    if(!bForwardOK){
        if(fnOnMoveError){
            fnOnMoveError();
        }
    }

    return bForwardOK;
}

bool CharObject::RequestSpaceMove(uint32_t nMapID, int nX, int nY, bool bStrictMove, std::function<void()> fnOnMoveOK, std::function<void()> fnOnMoveError)
{
    if(!CanMove()){
        return false;
    }

    if(!UIDFunc::GetMapUID(nMapID)){
        g_MonoServer->AddLog(LOGTYPE_WARNING, "Invalid MapID: %" PRIu32, nMapID);
        return false;
    }

    AMTrySpaceMove stAMTSM;
    std::memset(&stAMTSM, 0, sizeof(stAMTSM));

    stAMTSM.UID        = UID();
    stAMTSM.X          = nX;
    stAMTSM.Y          = nY;
    stAMTSM.StrictMove = bStrictMove;

    m_MoveLock = true;
    return m_ActorPod->Forward(UIDFunc::GetMapUID(nMapID), {MPK_TRYSPACEMOVE, stAMTSM}, [this, nX, nY, fnOnMoveOK, fnOnMoveError](const MessagePack &rstRMPK)
    {
        // 1. check if lock released
        //    shouldn't release before get map's response

        if(!m_MoveLock){
            g_MonoServer->AddLog(LOGTYPE_WARNING, "MoveLock released before map responds: UIDName = %s", UIDName());
            g_MonoServer->Restart();
        }

        m_MoveLock = false;

        // 2. handle move
        //    need to check if current CO can move even we checked before

        switch(rstRMPK.Type()){
            case MPK_SPACEMOVEOK:
                {
                    // need to leave src map
                    // dst map already says OK for current move

                    // was to decleare a new function CharObject::LeaveMap(fnLeaveOK)
                    // but it could cause the issue that m_Map may stay invalid if after MPK_TRYLEAVE succeeds but
                    // fnLeaveOK doesn't provide a new map pointer

                    if(!CanMove()){
                        m_ActorPod->Forward(rstRMPK.From(), MPK_ERROR, rstRMPK.ID());
                        if(fnOnMoveError){
                            fnOnMoveError();
                        }
                        return;
                    }

                    AMTryLeave stAMTL;
                    std::memset(&stAMTL, 0, sizeof(stAMTL));

                    stAMTL.UID   = UID();
                    stAMTL.MapID = m_Map->ID();
                    stAMTL.X     = X();
                    stAMTL.Y     = Y();

                    m_MoveLock = true;
                    m_ActorPod->Forward(m_Map->UID(), {MPK_TRYLEAVE, stAMTL}, [this, rstRMPK, nX, nY, fnOnMoveOK, fnOnMoveError](const MessagePack &rstLeaveRMPK)
                    {
                        if(!m_MoveLock){
                            g_MonoServer->AddLog(LOGTYPE_WARNING, "MoveLock released before map responds: UIDName = %s", UIDName());
                            g_MonoServer->Restart();
                        }

                        m_MoveLock = false;
                        switch(rstLeaveRMPK.Type()){
                            case MPK_OK:
                                {
                                    AMSpaceMoveOK stAMSMOK;
                                    std::memcpy(&stAMSMOK, rstRMPK.Data(), sizeof(stAMSMOK));

                                    if(!CanMove()){
                                        m_ActorPod->Forward(rstRMPK.From(), MPK_ERROR, rstRMPK.ID());
                                        if(fnOnMoveError){
                                            fnOnMoveError();
                                        }
                                    }else{
                                        // 1. dispatch space move part 1 on old map
                                        DispatchAction(ActionSpaceMove1(X(), Y(), Direction()));

                                        // 2. setup new map
                                        m_X   = nX;
                                        m_Y   = nY;
                                        m_Map = (ServerMap *)(stAMSMOK.Ptr);

                                        m_LastMoveTime = g_MonoServer->GetTimeTick();
                                        m_ActorPod->Forward(rstRMPK.From(), MPK_OK, rstRMPK.ID());

                                        // 3. dispatch/report space move part 2 on new map
                                        DispatchAction(ActionSpaceMove2(X(), Y(), Direction()));
                                        ReportAction(UID(), ActionSpaceMove2(X(), Y(), Direction()));

                                        if(fnOnMoveOK){
                                            fnOnMoveOK();
                                        }
                                    }
                                    break;
                                }
                            default:
                                {
                                    if(fnOnMoveError){
                                        fnOnMoveError();
                                    }
                                    break;
                                }
                        }
                    });
                    break;
                }
            default:
                {
                    if(fnOnMoveError){
                        fnOnMoveError();
                    }
                    break;
                }
        }
    });
}

bool CharObject::CanMove()
{
    switch(GetState(STATE_DEAD)){
        case 0:
            {
                return !m_MoveLock;
            }
        default:
            {
                return false;
            }
    }
}

bool CharObject::CanAttack()
{
    switch(GetState(STATE_DEAD)){
        case 0:
            {
                return !m_AttackLock;
            }
        default:
            {
                return false;
            }
    }
}

bool CharObject::RetrieveLocation(uint64_t nUID, std::function<void(const COLocation &)> fnOnLocationOK, std::function<void()> fnOnLocationError)
{
    if(!nUID){
        throw std::invalid_argument(str_ffl() + ": Query location with zero UID");
    }

    if(nUID == UID()){
        throw std::invalid_argument(str_ffl() + ": Query UID to CO itself: " + UIDFunc::GetUIDString(nUID));
    }

    // current the UID is valid
    // lambda captured fnOnLocationOK then can be delayed by one step
    // 1.     valid cache : call fnOnLocationOK
    // 2. not valid cache : call fnOnLocationOK after refresh

    auto fnQueryLocation = [this, nUID, fnOnLocationOK, fnOnLocationError]() -> bool
    {
        AMQueryLocation stAMQL;
        std::memset(&stAMQL, 0, sizeof(stAMQL));

        stAMQL.UID   = UID();
        stAMQL.MapID = MapID();

        return m_ActorPod->Forward(nUID, {MPK_QUERYLOCATION, stAMQL}, [this, nUID, fnOnLocationOK, fnOnLocationError](const MessagePack &rstRMPK)
        {
            switch(rstRMPK.Type()){
                case MPK_LOCATION:
                    {
                        AMLocation stAML;
                        std::memcpy(&stAML, rstRMPK.Data(), sizeof(stAML));

                        // TODO
                        // when we get this response
                        // it's possible that the co has switched map

                        if(m_Map->In(stAML.MapID, stAML.X, stAML.Y) && stAML.UID == nUID){
                            m_LocationList[nUID] = COLocation
                            {
                                stAML.UID,
                                stAML.MapID,
                                stAML.RecordTime,
                                stAML.X,
                                stAML.Y,
                                stAML.Direction
                            };
                        }else{
                            m_LocationList.erase(nUID);
                        }

                        if(fnOnLocationOK){
                            fnOnLocationOK(m_LocationList[nUID]);
                        }
                        break;
                    }
                default:
                    {
                        m_LocationList.erase(nUID);
                        if(fnOnLocationError){
                            fnOnLocationError();
                        }
                        break;
                    }
            }
        });
    };

    // no entry found
    // do query and invocation, delay fnOnLocationOK by one step
    if(m_LocationList.find(nUID) == m_LocationList.end()){
        return fnQueryLocation();
    }

    // we find an entry
    // valid the entry, could be expired
    auto &rstRecord = m_LocationList[nUID];
    if(m_Map->In(rstRecord.MapID, rstRecord.X, rstRecord.Y)
            && g_MonoServer->GetTimeTick() <= rstRecord.RecordTime + 2 * 1000){

        if(fnOnLocationOK){
            fnOnLocationOK(rstRecord);
        }
        return true;
    }

    // found record is out of time
    // if query failed means this UID doesn't exist

    if(!fnQueryLocation()){
        m_LocationList.erase(nUID);
        return false;
    }
    return true;
}

bool CharObject::AddHitterUID(uint64_t nUID, int nDamage)
{
    if(nUID){
        for(auto rstRecord: m_HitterUIDRecord){
            if(rstRecord.UID == nUID){
                rstRecord.Damage += std::max<int>(0, nDamage);
                rstRecord.ActiveTime = g_MonoServer->GetTimeTick();
                return true;
            }
        }

        // new entry
        m_HitterUIDRecord.emplace_back(nUID, std::max<int>(0, nDamage), g_MonoServer->GetTimeTick());
        return true;
    }
    return false;
}

bool CharObject::DispatchHitterExp()
{
    auto nNowTick = g_MonoServer->GetTimeTick();
    for(size_t nIndex = 0; nIndex < m_HitterUIDRecord.size();){
        if(true
                && m_HitterUIDRecord[nIndex].UID
                && m_HitterUIDRecord[nIndex].ActiveTime + 2 * 60 * 1000 >= nNowTick){
            if(auto nType = UIDFunc::GetUIDType(m_HitterUIDRecord[nIndex].UID); nType == UID_MON || nType == UID_PLY){
                // record is valid
                // record is not time-out
                // record is monster or player
                nIndex++;
                continue;
            }
        }

        // remove it
        // we shouldn't cout this record
        m_HitterUIDRecord[nIndex] = m_HitterUIDRecord.back();
        m_HitterUIDRecord.pop_back();
    }

    auto fnCalcExp = [this](int nDamage) -> int
    {
        return nDamage * m_HitterUIDRecord.size();
    };

    for(auto rstRecord: m_HitterUIDRecord){
        AMExp stAME;
        std::memset(&stAME, 0, sizeof(stAME));
        stAME.Exp = fnCalcExp(rstRecord.Damage);
        m_ActorPod->Forward(rstRecord.UID, {MPK_EXP, stAME});
    }

    return true;
}

int CharObject::OneStepReach(int nDirection, int nMaxDistance, int *pX, int *pY)
{
    if(m_Map){
        if(nMaxDistance > 0){
            int nLastX = -1;
            int nLastY = -1;
            int nLastD =  0;
            for(int nDistance = 1; nDistance <= nMaxDistance; ++nDistance){
                int nX = -1;
                int nY = -1;
                if(true
                        && NextLocation(&nX, &nY, nDirection, nDistance)
                        && m_Map->GroundValid(nX, nY)){
                    nLastX = nX;
                    nLastY = nY;
                    nLastD = nDistance;
                }else{ break; }
            }

            if(nLastD && pX){ *pX = nLastX; }
            if(nLastD && pY){ *pY = nLastY; }

            // return could be 0 : keep pX and pY un-touched
            //                 N : return (*pX, *pY) and maximal distance
            return nLastD;
        }
        return nMaxDistance;
    }
    return -1;
}

void CharObject::DispatchHealth()
{
    AMUpdateHP stAMUHP;
    std::memset(&stAMUHP, 0, sizeof(stAMUHP));

    stAMUHP.UID   = UID();
    stAMUHP.MapID = MapID();
    stAMUHP.X     = X();
    stAMUHP.Y     = Y();
    stAMUHP.HP    = HP();
    stAMUHP.HPMax = HPMax();

    if(true
            && ActorPodValid()
            && m_Map
            && m_Map->ActorPodValid()){
        m_ActorPod->Forward(m_Map->UID(), {MPK_UPDATEHP, stAMUHP});
    }
}

void CharObject::DispatchAttack(uint64_t nUID, int nDC)
{
    if(nUID && DCValid(nDC, true)){
        AMAttack stAMA;
        std::memset(&stAMA, 0, sizeof(stAMA));

        stAMA.UID   = UID();
        stAMA.MapID = MapID();

        stAMA.X = X();
        stAMA.Y = Y();

        auto stDamage = GetAttackDamage(nDC);
        stAMA.Type    = stDamage.Type;
        stAMA.Damage  = stDamage.Damage;
        stAMA.Element = stDamage.Element;

        for(size_t nIndex = 0; nIndex < sizeof(stAMA.Effect) / sizeof(stAMA.Effect[0]); ++nIndex){
            if(nIndex < stDamage.EffectArray.EffectLen()){
                stAMA.Effect[nIndex] = stDamage.EffectArray.Effect()[nIndex];
            }else{
                stAMA.Effect[nIndex] = EFF_NONE;
            }
        }
        m_ActorPod->Forward(nUID, {MPK_ATTACK, stAMA});
    }
}

int CharObject::Speed(int nSpeedType) const
{
    switch(nSpeedType){
        case SPEED_MOVE:
            {
                return SYS_DEFSPEED;
            }
        case SPEED_ATTACK:
            {
                return SYS_DEFSPEED;
            }
        default:
            {
                return -1;
            }
    }
}

void CharObject::AddMonster(uint32_t nMonsterID, int nX, int nY, bool bRandom)
{
    AMAddCharObject stAMACO;
    std::memset(&stAMACO, 0, sizeof(stAMACO));

    stAMACO.Type = TYPE_MONSTER;

    stAMACO.Common.MapID  = m_Map->ID();
    stAMACO.Common.X      = nX;
    stAMACO.Common.Y      = nY;
    stAMACO.Common.Random = bRandom;

    stAMACO.Monster.MonsterID = nMonsterID;
    stAMACO.Monster.MasterUID = UID();

    m_ActorPod->Forward(m_ServiceCore->UID(), {MPK_ADDCHAROBJECT, stAMACO}, [](const MessagePack &rstRMPK)
    {
        switch(rstRMPK.ID()){
            default:
                {
                    break;
                }
        }
    });
}

int CharObject::EstimateHop(int nX, int nY)
{
    condcheck(m_Map);
    if(!m_Map->ValidC(nX, nY)){
        return -1;
    }

    int nLDistance2 = LDistance2(nX, nY, X(), Y());
    switch(nLDistance2){
        case 0:
            {
                return 0;
            }
        case 1:
        case 2:
            {
                return 1;
            }
        default:
            {
                auto nMaxStep = MaxStep();
                condcheck(false
                        || nMaxStep == 1
                        || nMaxStep == 2
                        || nMaxStep == 3);

                if(false
                        || nLDistance2 == 1 * nMaxStep * nMaxStep
                        || nLDistance2 == 2 * nMaxStep * nMaxStep){

                    auto nDir = PathFind::GetDirection(X(), Y(), nX, nY);
                    auto nMaxReach = OneStepReach(nDir, nMaxStep, nullptr, nullptr);

                    if(nMaxReach == nMaxStep){
                        return 1;
                    }
                }
                return 2;
            }
    }
}

int CharObject::CheckPathGrid(int nX, int nY, uint32_t nTimeOut) const
{
    condcheck(m_Map);
    if(!m_Map->GetMir2xMapData().ValidC(nX, nY)){
        return PathFind::INVALID;
    }

    if(!m_Map->GetMir2xMapData().Cell(nX, nY).CanThrough()){
        return PathFind::OBSTACLE;
    }

    if(X() == nX && Y() == nY){
        return PathFind::OCCUPIED;
    }

    for(auto stLocation: m_LocationList){
        if(nTimeOut){
            if(g_MonoServer->GetTimeTick() > stLocation.second.RecordTime + nTimeOut){
                continue;
            }
        }

        if(true
                && stLocation.second.X == nX
                && stLocation.second.Y == nY){
            return PathFind::OCCUPIED;
        }
    }

    return PathFind::FREE;
}

std::array<PathFind::PathNode, 3> CharObject::GetChaseGrid(int nX, int nY, int nDLen) const
{
    // always get the next step to chase
    // this function won't check if (nX, nY) is valid

    int nX0 = X();
    int nY0 = Y();

    std::array<PathFind::PathNode, 3> stvPathNode
    {{
        {-1, -1},
        {-1, -1},
        {-1, -1},
    }};

    int nDX = ((nX > nX0) - (nX < nX0));
    int nDY = ((nY > nY0) - (nY < nY0));

    switch(std::abs(nDX) + std::abs(nDY)){
        case 1:
            {
                if(nDY){
                    stvPathNode[0] = {nX0        , nY0 + nDY * nDLen};
                    stvPathNode[1] = {nX0 - nDLen, nY0 + nDY * nDLen};
                    stvPathNode[2] = {nX0 + nDLen, nY0 + nDY * nDLen};
                }else{
                    stvPathNode[0] = {nX0 + nDX * nDLen, nY0        };
                    stvPathNode[1] = {nX0 + nDX * nDLen, nY0 - nDLen};
                    stvPathNode[2] = {nX0 + nDX * nDLen, nY0 + nDLen};
                }
                break;
            }
        case 2:
            {
                stvPathNode[0] = {nX0 + nDX * nDLen, nY0 + nDY * nDLen};
                stvPathNode[1] = {nX0              , nY0 + nDY * nDLen};
                stvPathNode[2] = {nX0 + nDX * nDLen, nY0              };
                break;
            }
        default:
            {
                break;
            }
    }
    return stvPathNode;
}

double CharObject::OneStepCost(const CharObject::COPathFinder *pFinder, int nCheckCO, int nX0, int nY0, int nX1, int nY1) const
{
    switch(nCheckCO){
        case 0:
        case 1:
        case 2:
            {
                break;
            }
        default:
            {
                g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid argument: COPathFinder = %p, CheckCO = %d", pFinder, nCheckCO);
                break;
            }
    }

    int nMaxIndex = -1;
    switch(LDistance2(nX0, nY0, nX1, nY1)){
        case 0:
            {
                nMaxIndex = 0;
                break;
            }
        case 1:
        case 2:
            {
                nMaxIndex = 1;
                break;
            }
        case 4:
        case 8:
            {
                nMaxIndex = 2;
                break;
            }
        case  9:
        case 18:
            {
                nMaxIndex = 3;
                break;
            }
        default:
            {
                return -1.00;
            }
    }

    int nDX = (nX1 > nX0) - (nX1 < nX0);
    int nDY = (nY1 > nY0) - (nY1 < nY0);

    double fExtraPen = 0.00;
    for(int nIndex = 0; nIndex <= nMaxIndex; ++nIndex){
        int nCurrX = nX0 + nDX * nIndex;
        int nCurrY = nY0 + nDY * nIndex;
        switch(auto nGrid = pFinder ? pFinder->GetGrid(nCurrX, nCurrY) : CheckPathGrid(nCurrX, nCurrY)){
            case PathFind::FREE:
                {
                    break;
                }
            case PathFind::OCCUPIED:
                {
                    switch(nCheckCO){
                        case 1:
                            {
                                fExtraPen += 100.00;
                                break;
                            }
                        case 2:
                            {
                                return -1.00;
                            }
                        default:
                            {
                                break;
                            }
                    }
                    break;
                }
            case PathFind::INVALID:
            case PathFind::OBSTACLE:
                {
                    return -1.00;
                }
            default:
                {
                    g_MonoServer->AddLog(LOGTYPE_FATAL, "Invalid grid provided: %d at (%d, %d)", nGrid, nCurrX, nCurrY);
                    break;
                }
        }
    }

    return 1.00 + nMaxIndex * 0.10 + fExtraPen;
}

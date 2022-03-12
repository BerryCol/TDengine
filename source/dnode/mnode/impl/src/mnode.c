/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "mndAcct.h"
#include "mndAuth.h"
#include "mndBnode.h"
#include "mndCluster.h"
#include "mndConsumer.h"
#include "mndDb.h"
#include "mndDnode.h"
#include "mndFunc.h"
#include "mndInfoSchema.h"
#include "mndMnode.h"
#include "mndOffset.h"
#include "mndProfile.h"
#include "mndQnode.h"
#include "mndShow.h"
#include "mndSnode.h"
#include "mndStb.h"
#include "mndSubscribe.h"
#include "mndSync.h"
#include "mndTelem.h"
#include "mndTopic.h"
#include "mndTrans.h"
#include "mndUser.h"
#include "mndVgroup.h"

#define MQ_TIMER_MS    3000
#define TRNAS_TIMER_MS 6000
#define TELEM_TIMER_MS 86400000

int32_t mndSendReqToDnode(SMnode *pMnode, SEpSet *pEpSet, SRpcMsg *pMsg) {
  if (pMnode == NULL || pMnode->sendReqToDnodeFp == NULL) {
    terrno = TSDB_CODE_MND_NOT_READY;
    return -1;
  }

  return (*pMnode->sendReqToDnodeFp)(pMnode->pDnode, pEpSet, pMsg);
}

int32_t mndSendReqToMnode(SMnode *pMnode, SRpcMsg *pMsg) {
  if (pMnode == NULL || pMnode->sendReqToDnodeFp == NULL) {
    terrno = TSDB_CODE_MND_NOT_READY;
    return -1;
  }

  return (*pMnode->sendReqToMnodeFp)(pMnode->pDnode, pMsg);
}

void mndSendRedirectRsp(SMnode *pMnode, SRpcMsg *pMsg) {
  if (pMnode != NULL && pMnode->sendRedirectRspFp != NULL) {
    (*pMnode->sendRedirectRspFp)(pMnode->pDnode, pMsg);
  }
}

static void *mndBuildTimerMsg(int32_t *pContLen) {
  SMTimerReq timerReq = {0};

  int32_t contLen = tSerializeSMTimerMsg(NULL, 0, &timerReq);
  if (contLen <= 0) return NULL;
  void *pReq = rpcMallocCont(contLen);
  if (pReq == NULL) return NULL;

  tSerializeSMTimerMsg(pReq, contLen, &timerReq);
  *pContLen = contLen;
  return pReq;
}

static void mndPullupTrans(void *param, void *tmrId) {
  SMnode *pMnode = param;
  if (mndIsMaster(pMnode)) {
    int32_t contLen = 0;
    void   *pReq = mndBuildTimerMsg(&contLen);
    SRpcMsg rpcMsg = {.msgType = TDMT_MND_TRANS_TIMER, .pCont = pReq, .contLen = contLen};
    pMnode->putReqToMWriteQFp(pMnode->pDnode, &rpcMsg);
  }

  taosTmrReset(mndPullupTrans, TRNAS_TIMER_MS, pMnode, pMnode->timer, &pMnode->transTimer);
}

static void mndCalMqRebalance(void *param, void *tmrId) {
  SMnode *pMnode = param;
  if (mndIsMaster(pMnode)) {
    int32_t contLen = 0;
    void   *pReq = mndBuildTimerMsg(&contLen);
    SRpcMsg rpcMsg = {
        .msgType = TDMT_MND_MQ_TIMER,
        .pCont = pReq,
        .contLen = contLen,
    };
    pMnode->putReqToMReadQFp(pMnode->pDnode, &rpcMsg);
  }

  taosTmrReset(mndCalMqRebalance, MQ_TIMER_MS, pMnode, pMnode->timer, &pMnode->mqTimer);
}

static void mndPullupTelem(void *param, void *tmrId) {
  SMnode *pMnode = param;
  if (mndIsMaster(pMnode)) {
    int32_t contLen = 0;
    void   *pReq = mndBuildTimerMsg(&contLen);
    SRpcMsg rpcMsg = {.msgType = TDMT_MND_TELEM_TIMER, .pCont = pReq, .contLen = contLen};
    pMnode->putReqToMReadQFp(pMnode->pDnode, &rpcMsg);
  }

  taosTmrReset(mndPullupTelem, TELEM_TIMER_MS, pMnode, pMnode->timer, &pMnode->telemTimer);
}

static int32_t mndInitTimer(SMnode *pMnode) {
  pMnode->timer = taosTmrInit(5000, 200, 3600000, "MND");
  if (pMnode->timer == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  if (taosTmrReset(mndPullupTrans, TRNAS_TIMER_MS, pMnode, pMnode->timer, &pMnode->transTimer)) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  if (taosTmrReset(mndCalMqRebalance, MQ_TIMER_MS, pMnode, pMnode->timer, &pMnode->mqTimer)) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  if (taosTmrReset(mndPullupTelem, 60000, pMnode, pMnode->timer, &pMnode->telemTimer)) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  return 0;
}

static void mndCleanupTimer(SMnode *pMnode) {
  if (pMnode->timer != NULL) {
    taosTmrStop(pMnode->transTimer);
    pMnode->transTimer = NULL;
    taosTmrStop(pMnode->mqTimer);
    pMnode->mqTimer = NULL;
    taosTmrStop(pMnode->telemTimer);
    pMnode->telemTimer = NULL;
    taosTmrCleanUp(pMnode->timer);
    pMnode->timer = NULL;
  }
}

static int32_t mndCreateDir(SMnode *pMnode, const char *path) {
  pMnode->path = strdup(path);
  if (pMnode->path == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  if (taosMkDir(pMnode->path) != 0) {
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  }

  return 0;
}

static int32_t mndInitSdb(SMnode *pMnode) {
  SSdbOpt opt = {0};
  opt.path = pMnode->path;
  opt.pMnode = pMnode;

  pMnode->pSdb = sdbInit(&opt);
  if (pMnode->pSdb == NULL) {
    return -1;
  }

  return 0;
}

static int32_t mndDeploySdb(SMnode *pMnode) { return sdbDeploy(pMnode->pSdb); }
static int32_t mndReadSdb(SMnode *pMnode) { return sdbReadFile(pMnode->pSdb); }

static void mndCleanupSdb(SMnode *pMnode) {
  if (pMnode->pSdb) {
    sdbCleanup(pMnode->pSdb);
    pMnode->pSdb = NULL;
  }
}

static int32_t mndAllocStep(SMnode *pMnode, char *name, MndInitFp initFp, MndCleanupFp cleanupFp) {
  SMnodeStep step = {0};
  step.name = name;
  step.initFp = initFp;
  step.cleanupFp = cleanupFp;
  if (taosArrayPush(pMnode->pSteps, &step) == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  return 0;
}

static int32_t mndInitSteps(SMnode *pMnode) {
  if (mndAllocStep(pMnode, "mnode-sdb", mndInitSdb, mndCleanupSdb) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-trans", mndInitTrans, mndCleanupTrans) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-cluster", mndInitCluster, mndCleanupCluster) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-mnode", mndInitMnode, mndCleanupMnode) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-qnode", mndInitQnode, mndCleanupQnode) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-qnode", mndInitSnode, mndCleanupSnode) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-qnode", mndInitBnode, mndCleanupBnode) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-dnode", mndInitDnode, mndCleanupDnode) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-user", mndInitUser, mndCleanupUser) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-auth", mndInitAuth, mndCleanupAuth) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-acct", mndInitAcct, mndCleanupAcct) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-topic", mndInitTopic, mndCleanupTopic) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-consumer", mndInitConsumer, mndCleanupConsumer) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-subscribe", mndInitSubscribe, mndCleanupSubscribe) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-offset", mndInitOffset, mndCleanupOffset) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-vgroup", mndInitVgroup, mndCleanupVgroup) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-stb", mndInitStb, mndCleanupStb) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-infos", mndInitInfos, mndCleanupInfos) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-db", mndInitDb, mndCleanupDb) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-func", mndInitFunc, mndCleanupFunc) != 0) return -1;
  if (pMnode->clusterId <= 0) {
    if (mndAllocStep(pMnode, "mnode-sdb-deploy", mndDeploySdb, NULL) != 0) return -1;
  } else {
    if (mndAllocStep(pMnode, "mnode-sdb-read", mndReadSdb, NULL) != 0) return -1;
  }
  if (mndAllocStep(pMnode, "mnode-timer", mndInitTimer, NULL) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-profile", mndInitProfile, mndCleanupProfile) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-show", mndInitShow, mndCleanupShow) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-sync", mndInitSync, mndCleanupSync) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-telem", mndInitTelem, mndCleanupTelem) != 0) return -1;
  if (mndAllocStep(pMnode, "mnode-timer", NULL, mndCleanupTimer) != 0) return -1;

  return 0;
}

static void mndCleanupSteps(SMnode *pMnode, int32_t pos) {
  if (pMnode->pSteps == NULL) return;

  if (pos == -1) {
    pos = taosArrayGetSize(pMnode->pSteps) - 1;
  }

  for (int32_t s = pos; s >= 0; s--) {
    SMnodeStep *pStep = taosArrayGet(pMnode->pSteps, s);
    mDebug("%s will cleanup", pStep->name);
    if (pStep->cleanupFp != NULL) {
      (*pStep->cleanupFp)(pMnode);
    }
  }

  taosArrayClear(pMnode->pSteps);
  taosArrayDestroy(pMnode->pSteps);
  pMnode->pSteps = NULL;
}

static int32_t mndExecSteps(SMnode *pMnode) {
  int32_t size = taosArrayGetSize(pMnode->pSteps);
  for (int32_t pos = 0; pos < size; pos++) {
    SMnodeStep *pStep = taosArrayGet(pMnode->pSteps, pos);
    if (pStep->initFp == NULL) continue;

    if ((*pStep->initFp)(pMnode) != 0) {
      int32_t code = terrno;
      mError("%s exec failed since %s, start to cleanup", pStep->name, terrstr());
      mndCleanupSteps(pMnode, pos);
      terrno = code;
      return -1;
    } else {
      mDebug("%s is initialized", pStep->name);
    }
  }

  return 0;
}

static int32_t mndSetOptions(SMnode *pMnode, const SMnodeOpt *pOption) {
  pMnode->dnodeId = pOption->dnodeId;
  pMnode->clusterId = pOption->clusterId;
  pMnode->replica = pOption->replica;
  pMnode->selfIndex = pOption->selfIndex;
  memcpy(&pMnode->replicas, pOption->replicas, sizeof(SReplica) * TSDB_MAX_REPLICA);
  pMnode->pDnode = pOption->pDnode;
  pMnode->putReqToMWriteQFp = pOption->putReqToMWriteQFp;
  pMnode->putReqToMReadQFp = pOption->putReqToMReadQFp;
  pMnode->sendReqToDnodeFp = pOption->sendReqToDnodeFp;
  pMnode->sendReqToMnodeFp = pOption->sendReqToMnodeFp;
  pMnode->sendRedirectRspFp = pOption->sendRedirectRspFp;

  if (pMnode->sendReqToDnodeFp == NULL || pMnode->sendReqToMnodeFp == NULL || pMnode->sendRedirectRspFp == NULL ||
      pMnode->putReqToMWriteQFp == NULL || pMnode->dnodeId < 0 || pMnode->clusterId < 0) {
    terrno = TSDB_CODE_MND_INVALID_OPTIONS;
    return -1;
  }

  return 0;
}

SMnode *mndOpen(const char *path, const SMnodeOpt *pOption) {
  mDebug("start to open mnode in %s", path);

  SMnode *pMnode = calloc(1, sizeof(SMnode));
  if (pMnode == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    mError("failed to open mnode since %s", terrstr());
    return NULL;
  }

  char timestr[24] = "1970-01-01 00:00:00.00";
  (void)taosParseTime(timestr, &pMnode->checkTime, (int32_t)strlen(timestr), TSDB_TIME_PRECISION_MILLI, 0);

  pMnode->pSteps = taosArrayInit(24, sizeof(SMnodeStep));
  if (pMnode->pSteps == NULL) {
    free(pMnode);
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    mError("failed to open mnode since %s", terrstr());
    return NULL;
  }

  int32_t code = mndCreateDir(pMnode, path);
  if (code != 0) {
    code = terrno;
    mError("failed to open mnode since %s", terrstr());
    mndClose(pMnode);
    terrno = code;
    return NULL;
  }

  code = mndSetOptions(pMnode, pOption);
  if (code != 0) {
    code = terrno;
    mError("failed to open mnode since %s", terrstr());
    mndClose(pMnode);
    terrno = code;
    return NULL;
  }

  code = mndInitSteps(pMnode);
  if (code != 0) {
    code = terrno;
    mError("failed to open mnode since %s", terrstr());
    mndClose(pMnode);
    terrno = code;
    return NULL;
  }

  code = mndExecSteps(pMnode);
  if (code != 0) {
    code = terrno;
    mError("failed to open mnode since %s", terrstr());
    mndClose(pMnode);
    terrno = code;
    return NULL;
  }

  mndUpdateMnodeRole(pMnode);
  mDebug("mnode open successfully ");
  return pMnode;
}

void mndClose(SMnode *pMnode) {
  if (pMnode != NULL) {
    mDebug("start to close mnode");
    mndCleanupSteps(pMnode, -1);
    tfree(pMnode->path);
    tfree(pMnode);
    mDebug("mnode is closed");
  }
}

int32_t mndAlter(SMnode *pMnode, const SMnodeOpt *pOption) {
  mDebug("start to alter mnode");
  mDebug("mnode is altered");
  return 0;
}

void mndDestroy(const char *path) {
  mDebug("start to destroy mnode at %s", path);
  taosRemoveDir(path);
  mDebug("mnode is destroyed");
}

SMnodeMsg *mndInitMsg(SMnode *pMnode, SRpcMsg *pRpcMsg) {
  SMnodeMsg *pMsg = taosAllocateQitem(sizeof(SMnodeMsg));
  if (pMsg == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    mError("failed to create msg since %s, app:%p RPC:%p", terrstr(), pRpcMsg->ahandle, pRpcMsg->handle);
    return NULL;
  }

  if (pRpcMsg->msgType != TDMT_MND_TRANS_TIMER && pRpcMsg->msgType != TDMT_MND_MQ_TIMER &&
      pRpcMsg->msgType != TDMT_MND_MQ_DO_REBALANCE && pRpcMsg->msgType != TDMT_MND_TELEM_TIMER) {
    SRpcConnInfo connInfo = {0};
    if ((pRpcMsg->msgType & 1U) && rpcGetConnInfo(pRpcMsg->handle, &connInfo) != 0) {
      taosFreeQitem(pMsg);
      terrno = TSDB_CODE_MND_NO_USER_FROM_CONN;
      mError("failed to create msg since %s, app:%p RPC:%p", terrstr(), pRpcMsg->ahandle, pRpcMsg->handle);
      return NULL;
    }
    memcpy(pMsg->user, connInfo.user, TSDB_USER_LEN);
  }

  pMsg->pMnode = pMnode;
  pMsg->rpcMsg = *pRpcMsg;
  pMsg->createdTime = taosGetTimestampSec();

  if (pRpcMsg != NULL) {
    mTrace("msg:%p, is created, app:%p RPC:%p user:%s", pMsg, pRpcMsg->ahandle, pRpcMsg->handle, pMsg->user);
  }
  return pMsg;
}

void mndCleanupMsg(SMnodeMsg *pMsg) {
  mTrace("msg:%p, is destroyed", pMsg);
  rpcFreeCont(pMsg->rpcMsg.pCont);
  pMsg->rpcMsg.pCont = NULL;
  taosFreeQitem(pMsg);
}

void mndSendRsp(SMnodeMsg *pMsg, int32_t code) {
  SRpcMsg rpcRsp = {.handle = pMsg->rpcMsg.handle, .code = code};
  rpcSendResponse(&rpcRsp);
}

void mndProcessMsg(SMnodeMsg *pMsg) {
  SMnode *pMnode = pMsg->pMnode;
  int32_t code = 0;
  tmsg_t  msgType = pMsg->rpcMsg.msgType;
  void   *ahandle = pMsg->rpcMsg.ahandle;
  bool    isReq = (msgType & 1U);

  mTrace("msg:%p, type:%s will be processed, app:%p", pMsg, TMSG_INFO(msgType), ahandle);

  if (isReq && !mndIsMaster(pMnode)) {
    code = TSDB_CODE_APP_NOT_READY;
    mDebug("msg:%p, failed to process since %s, app:%p", pMsg, terrstr(), ahandle);
    goto PROCESS_RPC_END;
  }

  if (isReq && pMsg->rpcMsg.pCont == NULL) {
    code = TSDB_CODE_MND_INVALID_MSG_LEN;
    mError("msg:%p, failed to process since %s, app:%p", pMsg, terrstr(), ahandle);
    goto PROCESS_RPC_END;
  }

  MndMsgFp fp = pMnode->msgFp[TMSG_INDEX(msgType)];
  if (fp == NULL) {
    code = TSDB_CODE_MSG_NOT_PROCESSED;
    mError("msg:%p, failed to process since no msg handle, app:%p", pMsg, ahandle);
    goto PROCESS_RPC_END;
  }

  code = (*fp)(pMsg);
  if (code == TSDB_CODE_MND_ACTION_IN_PROGRESS) {
    mTrace("msg:%p, in progress, app:%p", pMsg, ahandle);
    return;
  } else if (code != 0) {
    code = terrno;
    mError("msg:%p, failed to process since %s, app:%p", pMsg, terrstr(), ahandle);
    goto PROCESS_RPC_END;
  } else {
    mTrace("msg:%p, is processed, app:%p", pMsg, ahandle);
  }

PROCESS_RPC_END:
  if (isReq) {
    if (pMsg->rpcMsg.handle == NULL) return;

    if (code == TSDB_CODE_APP_NOT_READY) {
      mndSendRedirectRsp(pMnode, &pMsg->rpcMsg);
    } else if (code != 0) {
      SRpcMsg rpcRsp = {.handle = pMsg->rpcMsg.handle, .contLen = pMsg->contLen, .pCont = pMsg->pCont, .code = code};
      rpcSendResponse(&rpcRsp);
    } else {
      SRpcMsg rpcRsp = {.handle = pMsg->rpcMsg.handle, .contLen = pMsg->contLen, .pCont = pMsg->pCont};
      rpcSendResponse(&rpcRsp);
    }
  }
}

void mndSetMsgHandle(SMnode *pMnode, tmsg_t msgType, MndMsgFp fp) {
  tmsg_t type = TMSG_INDEX(msgType);
  if (type >= 0 && type < TDMT_MAX) {
    pMnode->msgFp[type] = fp;
  }
}

// Note: uid 0 is reserved
uint64_t mndGenerateUid(char *name, int32_t len) {
  int32_t hashval = MurmurHash3_32(name, len);

  do {
    int64_t  us = taosGetTimestampUs();
    uint64_t x = (us & 0x000000FFFFFFFFFF) << 24;
    uint64_t uuid = x + ((hashval & ((1ul << 16) - 1ul)) << 8) + (taosRand() & ((1ul << 8) - 1ul));
    if (uuid) {
      return uuid;
    }
  } while (true);
}

void mndGetLoad(SMnode *pMnode, SMnodeLoad *pLoad) {
  memset(pLoad, 0, sizeof(SMnodeLoad));

  SSdb *pSdb = pMnode->pSdb;
  pLoad->numOfDnode = sdbGetSize(pSdb, SDB_DNODE);
  pLoad->numOfMnode = sdbGetSize(pSdb, SDB_MNODE);
  pLoad->numOfVgroup = sdbGetSize(pSdb, SDB_VGROUP);
  pLoad->numOfDatabase = sdbGetSize(pSdb, SDB_DB);
  pLoad->numOfSuperTable = sdbGetSize(pSdb, SDB_STB);

  void *pIter = NULL;
  while (1) {
    SVgObj *pVgroup = NULL;
    pIter = sdbFetch(pSdb, SDB_VGROUP, pIter, (void **)&pVgroup);
    if (pIter == NULL) break;

    pLoad->numOfChildTable += pVgroup->numOfTables;
    pLoad->numOfColumn += pVgroup->numOfTimeSeries;
    pLoad->totalPoints += pVgroup->pointsWritten;
    pLoad->totalStorage += pVgroup->totalStorage;
    pLoad->compStorage += pVgroup->compStorage;

    sdbRelease(pSdb, pVgroup);
  }
}

int32_t mndGetMonitorInfo(SMnode *pMnode, SMonClusterInfo *pClusterInfo, SMonVgroupInfo *pVgroupInfo,
                          SMonGrantInfo *pGrantInfo) {
  if (!mndIsMaster(pMnode)) return -1;

  SSdb   *pSdb = pMnode->pSdb;
  int64_t ms = taosGetTimestampMs();

  pClusterInfo->dnodes = taosArrayInit(sdbGetSize(pSdb, SDB_DNODE), sizeof(SMonDnodeDesc));
  pClusterInfo->mnodes = taosArrayInit(sdbGetSize(pSdb, SDB_MNODE), sizeof(SMonMnodeDesc));
  pVgroupInfo->vgroups = taosArrayInit(sdbGetSize(pSdb, SDB_VGROUP), sizeof(SMonVgroupDesc));
  if (pClusterInfo->dnodes == NULL || pClusterInfo->mnodes == NULL || pVgroupInfo->vgroups == NULL) {
    return -1;
  }

  // cluster info
  tstrncpy(pClusterInfo->version, version, sizeof(pClusterInfo->version));
  pClusterInfo->monitor_interval = tsMonitorInterval;
  pClusterInfo->connections_total = mndGetNumOfConnections(pMnode);

  void *pIter = NULL;
  while (1) {
    SDnodeObj *pObj = NULL;
    pIter = sdbFetch(pSdb, SDB_DNODE, pIter, (void **)&pObj);
    if (pIter == NULL) break;

    SMonDnodeDesc desc = {0};
    desc.dnode_id = pObj->id;
    tstrncpy(desc.dnode_ep, pObj->ep, sizeof(desc.dnode_ep));
    if (mndIsDnodeOnline(pMnode, pObj, ms)) {
      tstrncpy(desc.status, "ready", sizeof(desc.status));
    } else {
      tstrncpy(desc.status, "offline", sizeof(desc.status));
    }
    taosArrayPush(pClusterInfo->dnodes, &desc);
    sdbRelease(pSdb, pObj);
  }

  pIter = NULL;
  while (1) {
    SMnodeObj *pObj = NULL;
    pIter = sdbFetch(pSdb, SDB_MNODE, pIter, (void **)&pObj);
    if (pIter == NULL) break;

    SMonMnodeDesc desc = {0};
    desc.mnode_id = pObj->id;
    tstrncpy(desc.mnode_ep, pObj->pDnode->ep, sizeof(desc.mnode_ep));
    tstrncpy(desc.role, mndGetRoleStr(pObj->role), sizeof(desc.role));
    taosArrayPush(pClusterInfo->mnodes, &desc);
    sdbRelease(pSdb, pObj);

    if (pObj->role == TAOS_SYNC_STATE_LEADER) {
      pClusterInfo->first_ep_dnode_id = pObj->id;
      tstrncpy(pClusterInfo->first_ep, pObj->pDnode->ep, sizeof(pClusterInfo->first_ep));
      pClusterInfo->master_uptime = (ms - pObj->roleTime) / (86400000.0f);
    }
  }

  // vgroup info
  pIter = NULL;
  while (1) {
    SVgObj *pVgroup = NULL;
    pIter = sdbFetch(pSdb, SDB_VGROUP, pIter, (void **)&pVgroup);
    if (pIter == NULL) break;

    pClusterInfo->vgroups_total++;

    SMonVgroupDesc desc = {0};
    desc.vgroup_id = pVgroup->vgId;
    strncpy(desc.database_name, pVgroup->dbName, sizeof(desc.database_name));
    desc.tables_num = pVgroup->numOfTables;
    pGrantInfo->timeseries_used += pVgroup->numOfTimeSeries;
    tstrncpy(desc.status, "unsynced", sizeof(desc.status));
    for (int32_t i = 0; i < pVgroup->replica; ++i) {
      SVnodeGid     *pVgid = &pVgroup->vnodeGid[i];
      SMonVnodeDesc *pVnDesc = &desc.vnodes[i];
      pVnDesc->dnode_id = pVgid->dnodeId;
      tstrncpy(pVnDesc->vnode_role, mndGetRoleStr(pVgid->role), sizeof(pVnDesc->vnode_role));
      if (pVgid->role == TAOS_SYNC_STATE_LEADER) {
        tstrncpy(desc.status, "ready", sizeof(desc.status));
        pClusterInfo->vgroups_alive++;
      }
      if (pVgid->role == TAOS_SYNC_STATE_LEADER || pVgid->role == TAOS_SYNC_STATE_CANDIDATE) {
        pClusterInfo->vnodes_alive++;
      }
      pClusterInfo->vnodes_total++;
    }

    taosArrayPush(pVgroupInfo->vgroups, &desc);
    sdbRelease(pSdb, pVgroup);
  }

  // grant info
  pGrantInfo->expire_time = (pMnode->grant.expireTimeMS - ms) / 86400000.0f;
  pGrantInfo->timeseries_total = pMnode->grant.timeseriesAllowed;
  if (pMnode->grant.expireTimeMS == 0) {
    pGrantInfo->expire_time = INT32_MAX;
    pGrantInfo->timeseries_total = INT32_MAX;
  }

  return 0;
}

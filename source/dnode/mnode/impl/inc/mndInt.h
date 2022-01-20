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

#ifndef _TD_MND_INT_H_
#define _TD_MND_INT_H_

#include "mndDef.h"

#include "sdb.h"
#include "tcache.h"
#include "tep.h"
#include "tqueue.h"
#include "ttime.h"
#include "wal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*MndMsgFp)(SMnodeMsg *pMsg);
typedef int32_t (*MndInitFp)(SMnode *pMnode);
typedef void (*MndCleanupFp)(SMnode *pMnode);
typedef int32_t (*ShowMetaFp)(SMnodeMsg *pMsg, SShowObj *pShow, STableMetaRsp *pMeta);
typedef int32_t (*ShowRetrieveFp)(SMnodeMsg *pMsg, SShowObj *pShow, char *data, int32_t rows);
typedef void (*ShowFreeIterFp)(SMnode *pMnode, void *pIter);

typedef struct {
  const char  *name;
  MndInitFp    initFp;
  MndCleanupFp cleanupFp;
} SMnodeStep;

typedef struct {
  int64_t        showId;
  ShowMetaFp     metaFps[TSDB_MGMT_TABLE_MAX];
  ShowRetrieveFp retrieveFps[TSDB_MGMT_TABLE_MAX];
  ShowFreeIterFp freeIterFps[TSDB_MGMT_TABLE_MAX];
  SCacheObj     *cache;
} SShowMgmt;

typedef struct {
  int32_t    connId;
  SCacheObj *cache;
} SProfileMgmt;

typedef struct {
  int8_t           enable;
  pthread_mutex_t  lock;
  pthread_cond_t   cond;
  volatile int32_t exit;
  pthread_t        thread;
  char             email[TSDB_FQDN_LEN];
} STelemMgmt;

typedef struct {
  int32_t    errCode;
  sem_t      syncSem;
  SWal      *pWal;
  SSyncNode *pSyncNode;
  ESyncState state;
} SSyncMgmt;

typedef struct SMnode {
  int32_t           dnodeId;
  int64_t           clusterId;
  int8_t            replica;
  int8_t            selfIndex;
  SReplica          replicas[TSDB_MAX_REPLICA];
  tmr_h             timer;
  tmr_h             transTimer;
  char             *path;
  SMnodeCfg         cfg;
  int64_t           checkTime;
  SSdb             *pSdb;
  SDnode           *pDnode;
  SArray           *pSteps;
  SShowMgmt         showMgmt;
  SProfileMgmt      profileMgmt;
  STelemMgmt        telemMgmt;
  SSyncMgmt         syncMgmt;
  MndMsgFp          msgFp[TDMT_MAX];
  SendReqToDnodeFp  sendReqToDnodeFp;
  SendReqToMnodeFp  sendReqToMnodeFp;
  SendRedirectRspFp sendRedirectRspFp;
  PutReqToMWriteQFp putReqToMWriteQFp;
} SMnode;

int32_t mndSendReqToDnode(SMnode *pMnode, SEpSet *pEpSet, SRpcMsg *rpcMsg);
int32_t mndSendReqToMnode(SMnode *pMnode, SRpcMsg *pMsg);
void    mndSendRedirectRsp(SMnode *pMnode, SRpcMsg *pMsg);
void    mndSetMsgHandle(SMnode *pMnode, tmsg_t msgType, MndMsgFp fp);

uint64_t mndGenerateUid(char *name, int32_t len) ;

#ifdef __cplusplus
}
#endif

#endif /*_TD_MND_INT_H_*/

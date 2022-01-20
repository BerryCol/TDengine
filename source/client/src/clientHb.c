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

#include "clientHb.h"
#include "trpc.h"

static SClientHbMgr clientHbMgr = {0};

static int32_t hbCreateThread();
static void    hbStopThread();

static int32_t hbMqHbRspHandle(SClientHbRsp* pReq) {
  return 0;
}

void hbMgrInitMqHbRspHandle() {
  clientHbMgr.handle[HEARTBEAT_TYPE_MQ] = hbMqHbRspHandle;
}

static FORCE_INLINE void hbMgrInitHandle() {
  // init all handle
  hbMgrInitMqHbRspHandle();
}

SClientHbBatchReq* hbGatherAllInfo(SAppHbMgr *pAppHbMgr) {
  SClientHbBatchReq* pReq = malloc(sizeof(SClientHbBatchReq));
  if (pReq == NULL) {
    terrno = TSDB_CODE_TSC_OUT_OF_MEMORY;
    return NULL;
  }
  int32_t connKeyCnt = atomic_load_32(&pAppHbMgr->connKeyCnt);
  pReq->reqs = taosArrayInit(connKeyCnt, sizeof(SClientHbReq));

  void *pIter = taosHashIterate(pAppHbMgr->activeInfo, NULL);
  while (pIter != NULL) {
    taosArrayPush(pReq->reqs, pIter);
    SClientHbReq* pOneReq = pIter;
    taosHashClear(pOneReq->info);

    pIter = taosHashIterate(pAppHbMgr->activeInfo, pIter);
  }

  pIter = taosHashIterate(pAppHbMgr->getInfoFuncs, NULL);
  while (pIter != NULL) {
    FGetConnInfo getConnInfoFp = (FGetConnInfo)pIter;
    SClientHbKey connKey;
    taosHashCopyKey(pIter, &connKey);
    getConnInfoFp(connKey, NULL);

    pIter = taosHashIterate(pAppHbMgr->activeInfo, pIter);
  }

  return pReq;
}

static void* hbThreadFunc(void* param) {
  setThreadName("hb");
  while (1) {
    int8_t threadStop = atomic_load_8(&clientHbMgr.threadStop);
    if(threadStop) {
      break;
    }

    int sz = taosArrayGetSize(clientHbMgr.appHbMgrs);
    for(int i = 0; i < sz; i++) {
      SAppHbMgr* pAppHbMgr = taosArrayGet(clientHbMgr.appHbMgrs, i);
      SClientHbBatchReq* pReq = hbGatherAllInfo(pAppHbMgr);
      void* reqStr = NULL;
      int tlen = tSerializeSClientHbBatchReq(&reqStr, pReq);
      SMsgSendInfo info;
      /*info.fp = hbHandleRsp;*/

      int64_t transporterId = 0;
      asyncSendMsgToServer(pAppHbMgr->transporter, &pAppHbMgr->epSet, &transporterId, &info);
      tFreeClientHbBatchReq(pReq);

      atomic_add_fetch_32(&pAppHbMgr->reportCnt, 1);
      taosMsleep(HEARTBEAT_INTERVAL);
    }
  }
  return NULL;
}

static int32_t hbCreateThread() {
  pthread_attr_t thAttr;
  pthread_attr_init(&thAttr);
  pthread_attr_setdetachstate(&thAttr, PTHREAD_CREATE_JOINABLE);

  if (pthread_create(&clientHbMgr.thread, &thAttr, hbThreadFunc, NULL) != 0) {
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  }
  pthread_attr_destroy(&thAttr);
  return 0;
}

static void hbStopThread() {
  atomic_store_8(&clientHbMgr.threadStop, 1);
}

SAppHbMgr* appHbMgrInit(void* transporter, SEpSet epSet) {
  SAppHbMgr* pAppHbMgr = malloc(sizeof(SAppHbMgr)); 
  if (pAppHbMgr == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return NULL;
  }
  // init stat
  pAppHbMgr->startTime = taosGetTimestampMs();

  // init connection info
  pAppHbMgr->transporter = transporter;
  pAppHbMgr->epSet = epSet;

  // init hash info
  pAppHbMgr->activeInfo = taosHashInit(64, hbKeyHashFunc, 1, HASH_ENTRY_LOCK);
  pAppHbMgr->activeInfo->freeFp = tFreeClientHbReq;
  // init getInfoFunc
  pAppHbMgr->getInfoFuncs = taosHashInit(64, hbKeyHashFunc, 1, HASH_ENTRY_LOCK);

  taosArrayPush(clientHbMgr.appHbMgrs, &pAppHbMgr);
  return pAppHbMgr;
}

void appHbMgrCleanup(SAppHbMgr* pAppHbMgr) {
  pthread_mutex_lock(&clientHbMgr.lock);

  int sz = taosArrayGetSize(clientHbMgr.appHbMgrs);
  for (int i = 0; i < sz; i++) {
    SAppHbMgr* pTarget = taosArrayGet(clientHbMgr.appHbMgrs, i);
    if (pAppHbMgr == pTarget) {
      taosHashCleanup(pTarget->activeInfo);
      taosHashCleanup(pTarget->getInfoFuncs);
    }
  }

  pthread_mutex_unlock(&clientHbMgr.lock);
}

int hbMgrInit() {
  // init once
  int8_t old = atomic_val_compare_exchange_8(&clientHbMgr.inited, 0, 1);
  if (old == 1) return 0;

  clientHbMgr.appHbMgrs = taosArrayInit(0, sizeof(void*));
  pthread_mutex_init(&clientHbMgr.lock, NULL);

  // init handle funcs
  hbMgrInitHandle();

  // init backgroud thread
  hbCreateThread();

  return 0;
}

void hbMgrCleanUp() {
  // destroy all appHbMgr
  int8_t old = atomic_val_compare_exchange_8(&clientHbMgr.inited, 1, 0);
  if (old == 0) return;

  taosArrayDestroy(clientHbMgr.appHbMgrs);

}

int hbHandleRsp(SClientHbBatchRsp* hbRsp) {
  int64_t reqId = hbRsp->reqId;
  int64_t rspId = hbRsp->rspId;

  SArray* rsps = hbRsp->rsps;
  int32_t sz = taosArrayGetSize(rsps);
  for (int i = 0; i < sz; i++) {
    SClientHbRsp* pRsp = taosArrayGet(rsps, i);
    if (pRsp->connKey.hbType < HEARTBEAT_TYPE_MAX) {
      clientHbMgr.handle[pRsp->connKey.hbType](pRsp);
    } else {
      // discard rsp
    }
  }
  return 0;
}

int hbRegisterConn(SAppHbMgr* pAppHbMgr, SClientHbKey connKey, FGetConnInfo func) {
  // init hash in activeinfo
  void* data = taosHashGet(pAppHbMgr->activeInfo, &connKey, sizeof(SClientHbKey));
  if (data != NULL) {
    return 0;
  }
  SClientHbReq hbReq;
  hbReq.connKey = connKey;
  hbReq.info = taosHashInit(64, hbKeyHashFunc, 1, HASH_ENTRY_LOCK);
  taosHashPut(pAppHbMgr->activeInfo, &connKey, sizeof(SClientHbKey), &hbReq, sizeof(SClientHbReq));
  // init hash
  if (func != NULL) {
    taosHashPut(pAppHbMgr->getInfoFuncs, &connKey, sizeof(SClientHbKey), func, sizeof(FGetConnInfo));
  }

  atomic_add_fetch_32(&pAppHbMgr->connKeyCnt, 1);
  return 0;
}

void hbDeregisterConn(SAppHbMgr* pAppHbMgr, SClientHbKey connKey) {
  taosHashRemove(pAppHbMgr->activeInfo, &connKey, sizeof(SClientHbKey));
  taosHashRemove(pAppHbMgr->getInfoFuncs, &connKey, sizeof(SClientHbKey));
  atomic_sub_fetch_32(&pAppHbMgr->connKeyCnt, 1);
}

int hbAddConnInfo(SAppHbMgr *pAppHbMgr, SClientHbKey connKey, void* key, void* value, int32_t keyLen, int32_t valueLen) {
  // find req by connection id
  SClientHbReq* pReq = taosHashGet(pAppHbMgr->activeInfo, &connKey, sizeof(SClientHbKey));
  ASSERT(pReq != NULL);

  taosHashPut(pReq->info, key, keyLen, value, valueLen);

  return 0;
}

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

#include "os.h"
#include "tcache.h"
#include "tglobal.h"
#include "tmsg.h"
#include "exception.h"

#include "thash.h"
#include "executorimpl.h"
#include "executor.h"
#include "tlosertree.h"
#include "ttypes.h"
#include "query.h"

typedef struct STaskMgmt {
  pthread_mutex_t lock;
  SCacheObj      *qinfoPool;      // query handle pool
  int32_t         vgId;
  bool            closed;
} STaskMgmt;

static void taskMgmtKillTaskFn(void* handle, void* param1) {
  void** fp = (void**)handle;
  qKillTask(*fp);
}

static void freeqinfoFn(void *qhandle) {
  void** handle = qhandle;
  if (handle == NULL || *handle == NULL) {
    return;
  }

  qKillTask(*handle);
  qDestroyTask(*handle);
}

void freeParam(STaskParam *param) {
  tfree(param->sql);
  tfree(param->tagCond);
  tfree(param->tbnameCond);
  tfree(param->pTableIdList);
  taosArrayDestroy(param->pOperator);
  tfree(param->pExprs);
  tfree(param->pSecExprs);

  tfree(param->pExpr);
  tfree(param->pSecExpr);

  tfree(param->pGroupColIndex);
  tfree(param->pTagColumnInfo);
  tfree(param->pGroupbyExpr);
  tfree(param->prevResult);
}

// todo parse json to get the operator tree.

int32_t qCreateTask(void* tsdb, int32_t vgId, void* pQueryMsg, qTaskInfo_t* pTaskInfo, uint64_t taskId) {
  assert(pQueryMsg != NULL && tsdb != NULL);

  int32_t code = TSDB_CODE_SUCCESS;
#if 0
  STaskParam param = {0};
  code = convertQueryMsg(pQueryMsg, &param);
  if (code != TSDB_CODE_SUCCESS) {
    goto _over;
  }

  if (pQueryMsg->numOfTables <= 0) {
    qError("Invalid number of tables to query, numOfTables:%d", pQueryMsg->numOfTables);
    code = TSDB_CODE_QRY_INVALID_MSG;
    goto _over;
  }

  if (param.pTableIdList == NULL || taosArrayGetSize(param.pTableIdList) == 0) {
    qError("qmsg:%p, SQueryTableReq wrong format", pQueryMsg);
    code = TSDB_CODE_QRY_INVALID_MSG;
    goto _over;
  }

  SQueriedTableInfo info = { .numOfTags = pQueryMsg->numOfTags, .numOfCols = pQueryMsg->numOfCols, .colList = pQueryMsg->tableCols};
  if ((code = createQueryFunc(&info, pQueryMsg->numOfOutput, &param.pExprs, param.pExpr, param.pTagColumnInfo,
                              pQueryMsg->queryType, pQueryMsg, param.pUdfInfo)) != TSDB_CODE_SUCCESS) {
    goto _over;
  }

  if (param.pSecExpr != NULL) {
    if ((code = createIndirectQueryFuncExprFromMsg(pQueryMsg, pQueryMsg->secondStageOutput, &param.pSecExprs, param.pSecExpr, param.pExprs, param.pUdfInfo)) != TSDB_CODE_SUCCESS) {
      goto _over;
    }
  }

  if (param.colCond != NULL) {
    if ((code = createQueryFilter(param.colCond, pQueryMsg->colCondLen, &param.pFilters)) != TSDB_CODE_SUCCESS) {
      goto _over;
    }
  }

  param.pGroupbyExpr = createGroupbyExprFromMsg(pQueryMsg, param.pGroupColIndex, &code);
  if ((param.pGroupbyExpr == NULL && pQueryMsg->numOfGroupCols != 0) || code != TSDB_CODE_SUCCESS) {
    goto _over;
  }

  bool isSTableQuery = false;
  STableGroupInfo tableGroupInfo = {0};
  int64_t st = taosGetTimestampUs();

  if (TSDB_QUERY_HAS_TYPE(pQueryMsg->queryType, TSDB_QUERY_TYPE_TABLE_QUERY)) {
    STableIdInfo *id = taosArrayGet(param.pTableIdList, 0);

    qDebug("qmsg:%p query normal table, uid:%"PRId64", tid:%d", pQueryMsg, id->uid, id->tid);
    if ((code = tsdbGetOneTableGroup(tsdb, id->uid, pQueryMsg->window.skey, &tableGroupInfo)) != TSDB_CODE_SUCCESS) {
      goto _over;
    }
  } else if (TSDB_QUERY_HAS_TYPE(pQueryMsg->queryType, TSDB_QUERY_TYPE_MULTITABLE_QUERY|TSDB_QUERY_TYPE_STABLE_QUERY)) {
    isSTableQuery = true;

    // also note there's possibility that only one table in the super table
    if (!TSDB_QUERY_HAS_TYPE(pQueryMsg->queryType, TSDB_QUERY_TYPE_MULTITABLE_QUERY)) {
      STableIdInfo *id = taosArrayGet(param.pTableIdList, 0);

      // group by normal column, do not pass the group by condition to tsdb to group table into different group
      int32_t numOfGroupByCols = pQueryMsg->numOfGroupCols;
      if (pQueryMsg->numOfGroupCols == 1 && !TSDB_COL_IS_TAG(param.pGroupColIndex->flag)) {
        numOfGroupByCols = 0;
      }

      qDebug("qmsg:%p query stable, uid:%"PRIu64", tid:%d", pQueryMsg, id->uid, id->tid);
      code = tsdbQuerySTableByTagCond(tsdb, id->uid, pQueryMsg->window.skey, param.tagCond, pQueryMsg->tagCondLen,
                                      pQueryMsg->tagNameRelType, param.tbnameCond, &tableGroupInfo, param.pGroupColIndex, numOfGroupByCols);

      if (code != TSDB_CODE_SUCCESS) {
        qError("qmsg:%p failed to query stable, reason: %s", pQueryMsg, tstrerror(code));
        goto _over;
      }
    } else {
      code = tsdbGetTableGroupFromIdList(tsdb, param.pTableIdList, &tableGroupInfo);
      if (code != TSDB_CODE_SUCCESS) {
        goto _over;
      }

      qDebug("qmsg:%p query on %u tables in one group from client", pQueryMsg, tableGroupInfo.numOfTables);
    }

    int64_t el = taosGetTimestampUs() - st;
    qDebug("qmsg:%p tag filter completed, numOfTables:%u, elapsed time:%"PRId64"us", pQueryMsg, tableGroupInfo.numOfTables, el);
  } else {
    assert(0);
  }

  code = checkForQueryBuf(tableGroupInfo.numOfTables);
  if (code != TSDB_CODE_SUCCESS) {  // not enough query buffer, abort
    goto _over;
  }

  assert(pQueryMsg->stableQuery == isSTableQuery);
  (*pTaskInfo) = createQInfoImpl(pQueryMsg, param.pGroupbyExpr, param.pExprs, param.pSecExprs, &tableGroupInfo,
                              param.pTagColumnInfo, param.pFilters, vgId, param.sql, qId, param.pUdfInfo);

  param.sql    = NULL;
  param.pExprs = NULL;
  param.pSecExprs = NULL;
  param.pGroupbyExpr = NULL;
  param.pTagColumnInfo = NULL;
  param.pFilters = NULL;

  if ((*pTaskInfo) == NULL) {
    code = TSDB_CODE_QRY_OUT_OF_MEMORY;
    goto _over;
  }
  param.pUdfInfo = NULL;

  code = initQInfo(&pQueryMsg->tsBuf, tsdb, NULL, *pTaskInfo, &param, (char*)pQueryMsg, pQueryMsg->prevResultLen, NULL);

  _over:
  if (param.pGroupbyExpr != NULL) {
    taosArrayDestroy(param.pGroupbyExpr->columnInfo);
  }

  tfree(param.colCond);
  
  destroyUdfInfo(param.pUdfInfo);

  taosArrayDestroy(param.pTableIdList);
  param.pTableIdList = NULL;

  freeParam(&param);

  for (int32_t i = 0; i < pQueryMsg->numOfCols; i++) {
    SColumnInfo* column = pQueryMsg->tableCols + i;
    freeColumnFilterInfo(column->flist.filterInfo, column->flist.numOfFilters);
  }

  filterFreeInfo(param.pFilters);

  //pTaskInfo already freed in initQInfo, but *pTaskInfo may not pointer to null;
  if (code != TSDB_CODE_SUCCESS) {
    *pTaskInfo = NULL;
  }
#endif

  // if failed to add ref for all tables in this query, abort current query
  return code;
}

#ifdef TEST_IMPL
// wait moment
int waitMoment(SQInfo* pQInfo){
  if(pQInfo->sql) {
    int ms = 0;
    char* pcnt = strstr(pQInfo->sql, " count(*)");
    if(pcnt) return 0;
    
    char* pos = strstr(pQInfo->sql, " t_");
    if(pos){
      pos += 3;
      ms = atoi(pos);
      while(*pos >= '0' && *pos <= '9'){
        pos ++;
      }
      char unit_char = *pos;
      if(unit_char == 'h'){
        ms *= 3600*1000;
      } else if(unit_char == 'm'){
        ms *= 60*1000;
      } else if(unit_char == 's'){
        ms *= 1000;
      }
    }
    if(ms == 0) return 0;
    printf("test wait sleep %dms. sql=%s ...\n", ms, pQInfo->sql);
    
    if(ms < 1000) {
      taosMsleep(ms);
    } else {
      int used_ms = 0;
      while(used_ms < ms) {
        taosMsleep(1000);
        used_ms += 1000;
        if(isQueryKilled(pQInfo)){
          printf("test check query is canceled, sleep break.%s\n", pQInfo->sql);
          break;
        }
      }
    }
  }
  return 1;
}
#endif

bool qExecTask(qTaskInfo_t qinfo, uint64_t *qId) {
  SQInfo *pQInfo = (SQInfo *)qinfo;
  assert(pQInfo && pQInfo->signature == pQInfo);
  int64_t threadId = taosGetSelfPthreadId();

  int64_t curOwner = 0;
  if ((curOwner = atomic_val_compare_exchange_64(&pQInfo->owner, 0, threadId)) != 0) {
    qError("QInfo:0x%"PRIx64"-%p qhandle is now executed by thread:%p", pQInfo->qId, pQInfo, (void*) curOwner);
    pQInfo->code = TSDB_CODE_QRY_IN_EXEC;
    return false;
  }

  *qId = pQInfo->qId;
  if(pQInfo->startExecTs == 0)
    pQInfo->startExecTs = taosGetTimestampMs();

  if (isQueryKilled(pQInfo)) {
    qDebug("QInfo:0x%"PRIx64" it is already killed, abort", pQInfo->qId);
    return doBuildResCheck(pQInfo);
  }

  STaskRuntimeEnv* pRuntimeEnv = &pQInfo->runtimeEnv;
  if (pRuntimeEnv->tableqinfoGroupInfo.numOfTables == 0) {
    qDebug("QInfo:0x%"PRIx64" no table exists for query, abort", pQInfo->qId);
//    setTaskStatus(pRuntimeEnv, QUERY_COMPLETED);
    return doBuildResCheck(pQInfo);
  }

  // error occurs, record the error code and return to client
  int32_t ret = setjmp(pQInfo->runtimeEnv.env);
  if (ret != TSDB_CODE_SUCCESS) {
    publishQueryAbortEvent(pQInfo, ret);
    pQInfo->code = ret;
    qDebug("QInfo:0x%"PRIx64" query abort due to error/cancel occurs, code:%s", pQInfo->qId, tstrerror(pQInfo->code));
    return doBuildResCheck(pQInfo);
  }

  qDebug("QInfo:0x%"PRIx64" query task is launched", pQInfo->qId);

  bool newgroup = false;
  publishOperatorProfEvent(pRuntimeEnv->proot, QUERY_PROF_BEFORE_OPERATOR_EXEC);

  int64_t st = taosGetTimestampUs();
  pRuntimeEnv->outputBuf = pRuntimeEnv->proot->exec(pRuntimeEnv->proot, &newgroup);
  pQInfo->summary.elapsedTime += (taosGetTimestampUs() - st);
#ifdef TEST_IMPL
  waitMoment(pQInfo);
#endif
  publishOperatorProfEvent(pRuntimeEnv->proot, QUERY_PROF_AFTER_OPERATOR_EXEC);
  pRuntimeEnv->resultInfo.total += GET_NUM_OF_RESULTS(pRuntimeEnv);

  if (isQueryKilled(pQInfo)) {
    qDebug("QInfo:0x%"PRIx64" query is killed", pQInfo->qId);
  } else if (GET_NUM_OF_RESULTS(pRuntimeEnv) == 0) {
    qDebug("QInfo:0x%"PRIx64" over, %u tables queried, total %"PRId64" rows returned", pQInfo->qId, pRuntimeEnv->tableqinfoGroupInfo.numOfTables,
           pRuntimeEnv->resultInfo.total);
  } else {
    qDebug("QInfo:0x%"PRIx64" query paused, %d rows returned, total:%" PRId64 " rows", pQInfo->qId,
        GET_NUM_OF_RESULTS(pRuntimeEnv), pRuntimeEnv->resultInfo.total);
  }

  return doBuildResCheck(pQInfo);
}

int32_t qRetrieveQueryResultInfo(qTaskInfo_t qinfo, bool* buildRes, void* pRspContext) {
  SQInfo *pQInfo = (SQInfo *)qinfo;

  if (pQInfo == NULL || !isValidQInfo(pQInfo)) {
    qError("QInfo invalid qhandle");
    return TSDB_CODE_QRY_INVALID_QHANDLE;
  }

  *buildRes = false;
  if (IS_QUERY_KILLED(pQInfo)) {
    qDebug("QInfo:0x%"PRIx64" query is killed, code:0x%08x", pQInfo->qId, pQInfo->code);
    return pQInfo->code;
  }

  int32_t code = TSDB_CODE_SUCCESS;

  if (tsRetrieveBlockingModel) {
    pQInfo->rspContext = pRspContext;
    tsem_wait(&pQInfo->ready);
    *buildRes = true;
    code = pQInfo->code;
  } else {
    STaskRuntimeEnv* pRuntimeEnv = &pQInfo->runtimeEnv;
    STaskAttr *pQueryAttr = pQInfo->runtimeEnv.pQueryAttr;

    pthread_mutex_lock(&pQInfo->lock);

    assert(pQInfo->rspContext == NULL);
    if (pQInfo->dataReady == QUERY_RESULT_READY) {
      *buildRes = true;
      qDebug("QInfo:0x%"PRIx64" retrieve result info, rowsize:%d, rows:%d, code:%s", pQInfo->qId, pQueryAttr->resultRowSize,
             GET_NUM_OF_RESULTS(pRuntimeEnv), tstrerror(pQInfo->code));
    } else {
      *buildRes = false;
      qDebug("QInfo:0x%"PRIx64" retrieve req set query return result after paused", pQInfo->qId);
      pQInfo->rspContext = pRspContext;
      assert(pQInfo->rspContext != NULL);
    }

    code = pQInfo->code;
    pthread_mutex_unlock(&pQInfo->lock);
  }

  return code;
}

void* qGetResultRetrieveMsg(qTaskInfo_t qinfo) {
  SQInfo* pQInfo = (SQInfo*) qinfo;
  assert(pQInfo != NULL);

  return pQInfo->rspContext;
}

int32_t qKillTask(qTaskInfo_t qinfo) {
  SQInfo *pQInfo = (SQInfo *)qinfo;

  if (pQInfo == NULL || !isValidQInfo(pQInfo)) {
    return TSDB_CODE_QRY_INVALID_QHANDLE;
  }

  qDebug("QInfo:0x%"PRIx64" query killed", pQInfo->qId);
  setQueryKilled(pQInfo);

  // Wait for the query executing thread being stopped/
  // Once the query is stopped, the owner of qHandle will be cleared immediately.
  while (pQInfo->owner != 0) {
    taosMsleep(100);
  }

  return TSDB_CODE_SUCCESS;
}

int32_t qIsTaskCompleted(qTaskInfo_t qinfo) {
  SQInfo *pQInfo = (SQInfo *)qinfo;

  if (pQInfo == NULL || !isValidQInfo(pQInfo)) {
    return TSDB_CODE_QRY_INVALID_QHANDLE;
  }

  return isQueryKilled(pQInfo) || Q_STATUS_EQUAL(pQInfo->runtimeEnv.status, QUERY_OVER);
}

void qDestroyTask(qTaskInfo_t qHandle) {
  SQInfo* pQInfo = (SQInfo*) qHandle;
  if (!isValidQInfo(pQInfo)) {
    return;
  }

  qDebug("QInfo:0x%"PRIx64" query completed", pQInfo->qId);
  queryCostStatis(pQInfo);   // print the query cost summary
  doDestroyTask(pQInfo);
}

void* qOpenTaskMgmt(int32_t vgId) {
  const int32_t refreshHandleInterval = 30; // every 30 seconds, refresh handle pool

  char cacheName[128] = {0};
  sprintf(cacheName, "qhandle_%d", vgId);

  STaskMgmt* pTaskMgmt = calloc(1, sizeof(STaskMgmt));
  if (pTaskMgmt == NULL) {
    terrno = TSDB_CODE_QRY_OUT_OF_MEMORY;
    return NULL;
  }

  pTaskMgmt->qinfoPool = taosCacheInit(TSDB_CACHE_PTR_KEY, refreshHandleInterval, true, freeqinfoFn, cacheName);
  pTaskMgmt->closed    = false;
  pTaskMgmt->vgId      = vgId;

  pthread_mutex_init(&pTaskMgmt->lock, NULL);

  qDebug("vgId:%d, open queryTaskMgmt success", vgId);
  return pTaskMgmt;
}

void qTaskMgmtNotifyClosing(void* pQMgmt) {
  if (pQMgmt == NULL) {
    return;
  }

  STaskMgmt* pQueryMgmt = pQMgmt;
  qInfo("vgId:%d, set querymgmt closed, wait for all queries cancelled", pQueryMgmt->vgId);

  pthread_mutex_lock(&pQueryMgmt->lock);
  pQueryMgmt->closed = true;
  pthread_mutex_unlock(&pQueryMgmt->lock);

  taosCacheRefresh(pQueryMgmt->qinfoPool, taskMgmtKillTaskFn, NULL);
}

void qQueryMgmtReOpen(void *pQMgmt) {
  if (pQMgmt == NULL) {
    return;
  }

  STaskMgmt *pQueryMgmt = pQMgmt;
  qInfo("vgId:%d, set querymgmt reopen", pQueryMgmt->vgId);

  pthread_mutex_lock(&pQueryMgmt->lock);
  pQueryMgmt->closed = false;
  pthread_mutex_unlock(&pQueryMgmt->lock);
}

void qCleanupTaskMgmt(void* pQMgmt) {
  if (pQMgmt == NULL) {
    return;
  }

  STaskMgmt* pQueryMgmt = pQMgmt;
  int32_t vgId = pQueryMgmt->vgId;

  assert(pQueryMgmt->closed);

  SCacheObj* pqinfoPool = pQueryMgmt->qinfoPool;
  pQueryMgmt->qinfoPool = NULL;

  taosCacheCleanup(pqinfoPool);
  pthread_mutex_destroy(&pQueryMgmt->lock);
  tfree(pQueryMgmt);

  qDebug("vgId:%d, queryMgmt cleanup completed", vgId);
}

void** qRegisterTask(void* pMgmt, uint64_t qId, void *qInfo) {
  if (pMgmt == NULL) {
    terrno = TSDB_CODE_VND_INVALID_VGROUP_ID;
    return NULL;
  }

  STaskMgmt *pQueryMgmt = pMgmt;
  if (pQueryMgmt->qinfoPool == NULL) {
    qError("QInfo:0x%"PRIx64"-%p failed to add qhandle into qMgmt, since qMgmt is closed", qId, (void*)qInfo);
    terrno = TSDB_CODE_VND_INVALID_VGROUP_ID;
    return NULL;
  }

  pthread_mutex_lock(&pQueryMgmt->lock);
  if (pQueryMgmt->closed) {
    pthread_mutex_unlock(&pQueryMgmt->lock);
    qError("QInfo:0x%"PRIx64"-%p failed to add qhandle into cache, since qMgmt is colsing", qId, (void*)qInfo);
    terrno = TSDB_CODE_VND_INVALID_VGROUP_ID;
    return NULL;
  } else {
    void** handle = taosCachePut(pQueryMgmt->qinfoPool, &qId, sizeof(qId), &qInfo, sizeof(TSDB_CACHE_PTR_TYPE),
                                 (getMaximumIdleDurationSec()*1000));
    pthread_mutex_unlock(&pQueryMgmt->lock);

    return handle;
  }
}

void** qAcquireTask(void* pMgmt, uint64_t _key) {
  STaskMgmt *pQueryMgmt = pMgmt;

  if (pQueryMgmt->closed) {
    terrno = TSDB_CODE_VND_INVALID_VGROUP_ID;
    return NULL;
  }

  if (pQueryMgmt->qinfoPool == NULL) {
    terrno = TSDB_CODE_QRY_INVALID_QHANDLE;
    return NULL;
  }

  void** handle = taosCacheAcquireByKey(pQueryMgmt->qinfoPool, &_key, sizeof(_key));
  if (handle == NULL || *handle == NULL) {
    terrno = TSDB_CODE_QRY_INVALID_QHANDLE;
    return NULL;
  } else {
    return handle;
  }
}

void** qReleaseTask(void* pMgmt, void* pQInfo, bool freeHandle) {
  STaskMgmt *pQueryMgmt = pMgmt;
  if (pQueryMgmt->qinfoPool == NULL) {
    return NULL;
  }

  taosCacheRelease(pQueryMgmt->qinfoPool, pQInfo, freeHandle);
  return 0;
}

#if 0
//kill by qid
int32_t qKillQueryByQId(void* pMgmt, int64_t qId, int32_t waitMs, int32_t waitCount) {
  int32_t error = TSDB_CODE_SUCCESS;
  void** handle = qAcquireTask(pMgmt, qId);
  if(handle == NULL) return terrno;

  SQInfo* pQInfo = (SQInfo*)(*handle);
  if (pQInfo == NULL || !isValidQInfo(pQInfo)) {
    return TSDB_CODE_QRY_INVALID_QHANDLE;
  }
  qWarn("QId:0x%"PRIx64" be killed(no memory commit).", pQInfo->qId);
  setQueryKilled(pQInfo);

  // wait query stop
  int32_t loop = 0;
  while (pQInfo->owner != 0) {
    taosMsleep(waitMs);
    if(loop++ > waitCount){
      error = TSDB_CODE_FAILED;
      break;
    }
  }

  qReleaseTask(pMgmt, (void **)&handle, true);
  return error;
}

#endif
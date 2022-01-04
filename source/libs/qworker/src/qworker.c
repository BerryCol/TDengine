#include "tmsg.h"
#include "query.h"
#include "qworker.h"
#include "qworkerInt.h"
#include "planner.h"

int32_t qwValidateStatus(int8_t oriStatus, int8_t newStatus) {
  int32_t code = 0;

  if (oriStatus == newStatus) {
    QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
  }
  
  switch (oriStatus) {
    case JOB_TASK_STATUS_NULL:
      if (newStatus != JOB_TASK_STATUS_EXECUTING 
       && newStatus != JOB_TASK_STATUS_FAILED 
       && newStatus != JOB_TASK_STATUS_NOT_START) {
        QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      }
      
      break;
    case JOB_TASK_STATUS_NOT_START:
      if (newStatus != JOB_TASK_STATUS_CANCELLED) {
        QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      }
      
      break;
    case JOB_TASK_STATUS_EXECUTING:
      if (newStatus != JOB_TASK_STATUS_PARTIAL_SUCCEED 
       && newStatus != JOB_TASK_STATUS_FAILED 
       && newStatus != JOB_TASK_STATUS_CANCELLING 
       && newStatus != JOB_TASK_STATUS_CANCELLED 
       && newStatus != JOB_TASK_STATUS_DROPPING) {
        QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      }
      
      break;
    case JOB_TASK_STATUS_PARTIAL_SUCCEED:
      if (newStatus != JOB_TASK_STATUS_EXECUTING 
       && newStatus != JOB_TASK_STATUS_SUCCEED
       && newStatus != JOB_TASK_STATUS_CANCELLED) {
        QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      }
      
      break;
    case JOB_TASK_STATUS_SUCCEED:
    case JOB_TASK_STATUS_FAILED:
    case JOB_TASK_STATUS_CANCELLING:
      if (newStatus != JOB_TASK_STATUS_CANCELLED) {
        QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      }
      
      break;
    case JOB_TASK_STATUS_CANCELLED:
    case JOB_TASK_STATUS_DROPPING:
      QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
      break;
      
    default:
      qError("invalid task status:%d", oriStatus);
      return TSDB_CODE_QRY_APP_ERROR;
  }

  return TSDB_CODE_SUCCESS;

_return:

  qError("invalid task status, from %d to %d", oriStatus, newStatus);
  QW_ERR_RET(code);
}

int32_t qwUpdateTaskInfo(SQWTaskStatus *task, int8_t type, void *data) {
  int32_t code = 0;
  
  switch (type) {
    case QW_TASK_INFO_STATUS: {
      int8_t newStatus = *(int8_t *)data;
      QW_ERR_RET(qwValidateStatus(task->status, newStatus));
      task->status = newStatus;
      break;
    }
    default:
      qError("uknown task info type:%d", type);
      return TSDB_CODE_QRY_APP_ERROR;
  }
  
  return TSDB_CODE_SUCCESS;
}

int32_t qwAddTaskResCache(SQWorkerMgmt *mgmt, uint64_t qId, uint64_t tId, void *data) {
  char id[sizeof(qId) + sizeof(tId)] = {0};
  QW_SET_QTID(id, qId, tId);

  SQWorkerResCache resCache = {0};
  resCache.data = data;

  QW_LOCK(QW_WRITE, &mgmt->resLock);
  if (0 != taosHashPut(mgmt->resHash, id, sizeof(id), &resCache, sizeof(SQWorkerResCache))) {
    QW_UNLOCK(QW_WRITE, &mgmt->resLock);
    qError("taosHashPut queryId[%"PRIx64"] taskId[%"PRIx64"] to resHash failed", qId, tId);
    return TSDB_CODE_QRY_APP_ERROR;
  }

  QW_UNLOCK(QW_WRITE, &mgmt->resLock);

  return TSDB_CODE_SUCCESS;
}

static int32_t qwAddScheduler(int32_t rwType, SQWorkerMgmt *mgmt, uint64_t sId, SQWSchStatus **sch) {
  SQWSchStatus newSch = {0};
  newSch.tasksHash = taosHashInit(mgmt->cfg.maxSchTaskNum, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), false, HASH_NO_LOCK);
  if (NULL == newSch.tasksHash) {
    qError("taosHashInit %d failed", mgmt->cfg.maxSchTaskNum);
    return TSDB_CODE_QRY_OUT_OF_MEMORY;
  }

  while (true) {
    QW_LOCK(QW_WRITE, &mgmt->schLock);
    int32_t code = taosHashPut(mgmt->schHash, &sId, sizeof(sId), &newSch, sizeof(newSch));
    if (0 != code) {
      if (!HASH_NODE_EXIST(code)) {
        QW_UNLOCK(QW_WRITE, &mgmt->schLock);
        qError("taosHashPut sId[%"PRIx64"] to scheduleHash failed", sId);
        taosHashCleanup(newSch.tasksHash);
        return TSDB_CODE_QRY_APP_ERROR;
      }
    }
    
    QW_UNLOCK(QW_WRITE, &mgmt->schLock);
    if (TSDB_CODE_SUCCESS == qwAcquireScheduler(rwType, mgmt, sId, sch, QW_NOT_EXIST_ADD)) {
      return TSDB_CODE_SUCCESS;
    }
  }

  return TSDB_CODE_SUCCESS;
}


static int32_t qwAcquireScheduler(int32_t rwType, SQWorkerMgmt *mgmt, uint64_t sId, SQWSchStatus **sch, int32_t nOpt) {
  QW_LOCK(rwType, &mgmt->schLock);
  *sch = taosHashGet(mgmt->schHash, &sId, sizeof(sId));
  if (NULL == (*sch)) {
    QW_UNLOCK(rwType, &mgmt->schLock);
    
    if (QW_NOT_EXIST_ADD == nOpt) {
      return qwAddScheduler(rwType, mgmt, sId, sch);
    } else if (QW_NOT_EXIST_RET_ERR == nOpt) {
      return TSDB_CODE_QRY_SCH_NOT_EXIST;
    } else {
      assert(0);
    }
  }

  return TSDB_CODE_SUCCESS;
}



static FORCE_INLINE void qwReleaseScheduler(int32_t rwType, SQWorkerMgmt *mgmt) {
  QW_UNLOCK(rwType, &mgmt->schLock);
}

static int32_t qwAcquireTaskImpl(int32_t rwType, SQWSchStatus *sch, uint64_t qId, uint64_t tId, SQWTaskStatus **task) {
  char id[sizeof(qId) + sizeof(tId)] = {0};
  QW_SET_QTID(id, qId, tId);

  QW_LOCK(rwType, &sch->tasksLock);
  *task = taosHashGet(sch->tasksHash, id, sizeof(id));
  if (NULL == (*task)) {
    QW_UNLOCK(rwType, &sch->tasksLock);

    return TSDB_CODE_QRY_TASK_NOT_EXIST;
  }

  return TSDB_CODE_SUCCESS;
}

static int32_t qwAcquireTask(int32_t rwType, SQWSchStatus *sch, uint64_t qId, uint64_t tId, SQWTaskStatus **task) {
  return qwAcquireTaskImpl(rwType, sch, qId, tId, task);
}


static FORCE_INLINE void qwReleaseTask(int32_t rwType, SQWSchStatus *sch) {
  QW_UNLOCK(rwType, &sch->tasksLock);
}


int32_t qwAddTaskToSch(int32_t rwType, SQWSchStatus *sch, uint64_t qId, uint64_t tId, int8_t status, int32_t eOpt, SQWTaskStatus **task) {
  int32_t code = 0;

  char id[sizeof(qId) + sizeof(tId)] = {0};
  QW_SET_QTID(id, qId, tId);

  SQWTaskStatus ntask = {0};
  ntask.status = status;

  while (true) {
    QW_LOCK(QW_WRITE, &sch->tasksLock);
    int32_t code = taosHashPut(sch->tasksHash, id, sizeof(id), &ntask, sizeof(ntask));
    if (0 != code) {
      QW_UNLOCK(QW_WRITE, &sch->tasksLock);
      if (HASH_NODE_EXIST(code)) {
        if (QW_EXIST_ACQUIRE == eOpt && rwType && task) {
          if (qwAcquireTask(rwType, sch, qId, tId, task)) {
            continue;
          }
        } else if (QW_EXIST_RET_ERR == eOpt) {
          return TSDB_CODE_QRY_TASK_ALREADY_EXIST;
        } else {
          assert(0);
        }

        break;
      } else {
        qError("taosHashPut queryId[%"PRIx64"] taskId[%"PRIx64"] to scheduleHash failed", qId, tId);
        return TSDB_CODE_QRY_APP_ERROR;
      }
    }
    
    QW_UNLOCK(QW_WRITE, &sch->tasksLock);

    if (rwType && task) {
      if (TSDB_CODE_SUCCESS == qwAcquireTask(rwType, sch, qId, tId, task)) {
        return TSDB_CODE_SUCCESS;
      }
    } else {
      break;
    }
  }  

  return TSDB_CODE_SUCCESS;
}


static int32_t qwAddTask(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t qId, uint64_t tId, int32_t status, int32_t eOpt, SQWSchStatus **sch, SQWTaskStatus **task) {
  SQWSchStatus *tsch = NULL;
  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &tsch, QW_NOT_EXIST_ADD));

  int32_t code = qwAddTaskToSch(QW_READ, tsch, qId, tId, status, eOpt, task);
  if (code) {
    qwReleaseScheduler(QW_WRITE, mgmt);
  }

  if (NULL == task) {
    qwReleaseScheduler(QW_READ, mgmt);
  } else if (sch) {
    *sch = tsch;
  }

  QW_RET(code);
}



static FORCE_INLINE int32_t qwAcquireTaskResCache(int32_t rwType, SQWorkerMgmt *mgmt, uint64_t queryId, uint64_t taskId, SQWorkerResCache **res) {
  char id[sizeof(queryId) + sizeof(taskId)] = {0};
  QW_SET_QTID(id, queryId, taskId);
  
  QW_LOCK(rwType, &mgmt->resLock);
  *res = taosHashGet(mgmt->resHash, id, sizeof(id));
  if (NULL == (*res)) {
    QW_UNLOCK(rwType, &mgmt->resLock);
    return TSDB_CODE_QRY_RES_CACHE_NOT_EXIST;
  }

  return TSDB_CODE_SUCCESS;
}

static FORCE_INLINE void qwReleaseTaskResCache(int32_t rwType, SQWorkerMgmt *mgmt) {
  QW_UNLOCK(rwType, &mgmt->resLock);
}


int32_t qwGetSchTasksStatus(SQWorkerMgmt *mgmt, uint64_t sId, SSchedulerStatusRsp **rsp) {
  SQWSchStatus *sch = NULL;
  int32_t taskNum = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));
  
  sch->lastAccessTs = taosGetTimestampSec();

  QW_LOCK(QW_READ, &sch->tasksLock);
  
  taskNum = taosHashGetSize(sch->tasksHash);
  
  int32_t size = sizeof(SSchedulerStatusRsp) + sizeof((*rsp)->status[0]) * taskNum;
  *rsp = calloc(1, size);
  if (NULL == *rsp) {
    qError("calloc %d failed", size);
    QW_UNLOCK(QW_READ, &sch->tasksLock);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return TSDB_CODE_QRY_OUT_OF_MEMORY;
  }

  void *key = NULL;
  size_t keyLen = 0;
  int32_t i = 0;

  void *pIter = taosHashIterate(sch->tasksHash, NULL);
  while (pIter) {
    SQWTaskStatus *taskStatus = (SQWTaskStatus *)pIter;
    taosHashGetKey(pIter, &key, &keyLen);

    QW_GET_QTID(key, (*rsp)->status[i].queryId, (*rsp)->status[i].taskId);
    (*rsp)->status[i].status = taskStatus->status;
    
    pIter = taosHashIterate(sch->tasksHash, pIter);
  }  

  QW_UNLOCK(QW_READ, &sch->tasksLock);
  qwReleaseScheduler(QW_READ, mgmt);

  (*rsp)->num = taskNum;

  return TSDB_CODE_SUCCESS;
}



int32_t qwUpdateSchLastAccess(SQWorkerMgmt *mgmt, uint64_t sId) {
  SQWSchStatus *sch = NULL;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));

  sch->lastAccessTs = taosGetTimestampSec();

  qwReleaseScheduler(QW_READ, mgmt);

  return TSDB_CODE_SUCCESS;
}

int32_t qwUpdateTaskStatus(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t qId, uint64_t tId, int8_t status) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));

  QW_ERR_JRET(qwAcquireTask(QW_READ, sch, qId, tId, &task));

  QW_LOCK(QW_WRITE, &task->lock);
  qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &status);
  QW_UNLOCK(QW_WRITE, &task->lock);
  
_return:

  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  QW_RET(code);
}


int32_t qwGetTaskStatus(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId, int8_t *taskStatus) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;
  
  if (qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR)) {
    *taskStatus = JOB_TASK_STATUS_NULL;
    return TSDB_CODE_SUCCESS;
  }

  if (qwAcquireTask(QW_READ, sch, queryId, taskId, &task)) {
    qwReleaseScheduler(QW_READ, mgmt);
    
    *taskStatus = JOB_TASK_STATUS_NULL;
    return TSDB_CODE_SUCCESS;
  }

  *taskStatus = task->status;

  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  QW_RET(code);
}


int32_t qwCancelTask(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_ADD));

  if (qwAcquireTask(QW_READ, sch, queryId, taskId, &task)) {
    qwReleaseScheduler(QW_READ, mgmt);
    
    code = qwAddTask(mgmt, sId, queryId, taskId, JOB_TASK_STATUS_NOT_START, QW_EXIST_ACQUIRE, &sch, &task);
    if (code) {
      qwReleaseScheduler(QW_READ, mgmt);
      QW_ERR_RET(code);
    }
  }

  QW_LOCK(QW_WRITE, &task->lock);

  task->cancel = true;
  
  int8_t oriStatus = task->status;
  int8_t newStatus = 0;
  
  if (task->status == JOB_TASK_STATUS_CANCELLED || task->status == JOB_TASK_STATUS_NOT_START || task->status == JOB_TASK_STATUS_CANCELLING || task->status == JOB_TASK_STATUS_DROPPING) {
    QW_UNLOCK(QW_WRITE, &task->lock);

    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return TSDB_CODE_SUCCESS;
  } else if (task->status == JOB_TASK_STATUS_FAILED || task->status == JOB_TASK_STATUS_SUCCEED || task->status == JOB_TASK_STATUS_PARTIAL_SUCCEED) {
    newStatus = JOB_TASK_STATUS_CANCELLED;
    QW_ERR_JRET(qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &newStatus));
  } else {
    newStatus = JOB_TASK_STATUS_CANCELLING;
    QW_ERR_JRET(qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &newStatus));
  }

  QW_UNLOCK(QW_WRITE, &task->lock);
  
  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  if (oriStatus == JOB_TASK_STATUS_EXECUTING) {
    //TODO call executer to cancel subquery async
  }
  
  return TSDB_CODE_SUCCESS;

_return:

  if (task) {
    QW_UNLOCK(QW_WRITE, &task->lock);
    
    qwReleaseTask(QW_READ, sch);
  }

  if (sch) {
    qwReleaseScheduler(QW_READ, mgmt);
  }

  QW_RET(code);
}



int32_t qwDropTask(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;
  char id[sizeof(queryId) + sizeof(taskId)] = {0};
  QW_SET_QTID(id, queryId, taskId);

  QW_LOCK(QW_WRITE, &mgmt->resLock);
  if (mgmt->resHash) {
    taosHashRemove(mgmt->resHash, id, sizeof(id));
  }
  QW_UNLOCK(QW_WRITE, &mgmt->resLock);
  
  if (TSDB_CODE_SUCCESS != qwAcquireScheduler(QW_WRITE, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR)) {
    qWarn("scheduler %"PRIx64" doesn't exist", sId);
    return TSDB_CODE_SUCCESS;
  }

  if (qwAcquireTask(QW_WRITE, sch, queryId, taskId, &task)) {
    qwReleaseScheduler(QW_WRITE, mgmt);
    
    qWarn("scheduler %"PRIx64" queryId %"PRIx64" taskId:%"PRIx64" doesn't exist", sId, queryId, taskId);
    return TSDB_CODE_SUCCESS;
  }

  taosHashRemove(sch->tasksHash, id, sizeof(id));

  qwReleaseTask(QW_WRITE, sch);
  qwReleaseScheduler(QW_WRITE, mgmt);
  
  return TSDB_CODE_SUCCESS;
}


int32_t qwCancelDropTask(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_ADD));

  if (qwAcquireTask(QW_READ, sch, queryId, taskId, &task)) {
    qwReleaseScheduler(QW_READ, mgmt);
    
    code = qwAddTask(mgmt, sId, queryId, taskId, JOB_TASK_STATUS_NOT_START, QW_EXIST_ACQUIRE, &sch, &task);
    if (code) {
      qwReleaseScheduler(QW_READ, mgmt);
      QW_ERR_RET(code);
    }
  }

  QW_LOCK(QW_WRITE, &task->lock);

  task->drop = true;

  int8_t oriStatus = task->status;
  int8_t newStatus = 0;
  
  if (task->status == JOB_TASK_STATUS_EXECUTING) {
    newStatus = JOB_TASK_STATUS_DROPPING;
    QW_ERR_JRET(qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &newStatus));
  } else if (task->status == JOB_TASK_STATUS_CANCELLING || task->status == JOB_TASK_STATUS_DROPPING || task->status == JOB_TASK_STATUS_NOT_START) {    
    QW_UNLOCK(QW_WRITE, &task->lock);
    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return TSDB_CODE_SUCCESS;
  } else {
    QW_UNLOCK(QW_WRITE, &task->lock);
    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
  
    QW_ERR_RET(qwDropTask(mgmt, sId, queryId, taskId));
    return TSDB_CODE_SUCCESS;
  }

  QW_UNLOCK(QW_WRITE, &task->lock);
  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  if (oriStatus == JOB_TASK_STATUS_EXECUTING) {
    //TODO call executer to cancel subquery async
  }
  
  return TSDB_CODE_SUCCESS;

_return:

  if (task) {
    QW_UNLOCK(QW_WRITE, &task->lock);
    
    qwReleaseTask(QW_READ, sch);
  }

  if (sch) {
    qwReleaseScheduler(QW_READ, mgmt);
  }

  QW_RET(code);
}



int32_t qwBuildAndSendQueryRsp(SRpcMsg *pMsg, int32_t code) {
  SQueryTableRsp *pRsp = (SQueryTableRsp *)rpcMallocCont(sizeof(SQueryTableRsp));
  pRsp->code = code;

  SRpcMsg rpcRsp = {
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = sizeof(*pRsp),
    .code    = code,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}

int32_t qwBuildAndSendReadyRsp(SRpcMsg *pMsg, int32_t code) {
  SResReadyRsp *pRsp = (SResReadyRsp *)rpcMallocCont(sizeof(SResReadyRsp));
  pRsp->code = code;

  SRpcMsg rpcRsp = {
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = sizeof(*pRsp),
    .code    = code,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}

int32_t qwBuildAndSendStatusRsp(SRpcMsg *pMsg, SSchedulerStatusRsp *sStatus) {
  int32_t size = 0;
  
  if (sStatus) {
    size = sizeof(SSchedulerStatusRsp) + sizeof(sStatus->status[0]) * sStatus->num;
  } else {
    size = sizeof(SSchedulerStatusRsp);
  }
  
  SSchedulerStatusRsp *pRsp = (SSchedulerStatusRsp *)rpcMallocCont(size);

  if (sStatus) {
    memcpy(pRsp, sStatus, size);
  } else {
    pRsp->num = 0;
  }

  SRpcMsg rpcRsp = {
    .msgType = pMsg->msgType + 1,
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = size,
    .code    = 0,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}

int32_t qwBuildAndSendFetchRsp(SRpcMsg *pMsg, void *data) {
  SRetrieveTableRsp *pRsp = (SRetrieveTableRsp *)rpcMallocCont(sizeof(SRetrieveTableRsp));
  memset(pRsp, 0, sizeof(SRetrieveTableRsp));

  //TODO fill msg
  pRsp->completed = true;

  SRpcMsg rpcRsp = {
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = sizeof(*pRsp),
    .code    = 0,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}


int32_t qwBuildAndSendCancelRsp(SRpcMsg *pMsg, int32_t code) {
  STaskCancelRsp *pRsp = (STaskCancelRsp *)rpcMallocCont(sizeof(STaskCancelRsp));
  pRsp->code = code;

  SRpcMsg rpcRsp = {
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = sizeof(*pRsp),
    .code    = code,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}

int32_t qwBuildAndSendDropRsp(SRpcMsg *pMsg, int32_t code) {
  STaskDropRsp *pRsp = (STaskDropRsp *)rpcMallocCont(sizeof(STaskDropRsp));
  pRsp->code = code;

  SRpcMsg rpcRsp = {
    .handle  = pMsg->handle,
    .ahandle = pMsg->ahandle,
    .pCont   = pRsp,
    .contLen = sizeof(*pRsp),
    .code    = code,
  };

  rpcSendResponse(&rpcRsp);

  return TSDB_CODE_SUCCESS;
}



int32_t qwCheckAndSendReadyRsp(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId, SRpcMsg *pMsg, int32_t rspCode) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));

  QW_ERR_JRET(qwAcquireTask(QW_READ, sch, queryId, taskId, &task));

  QW_LOCK(QW_WRITE, &task->lock);

  if (QW_READY_NOT_RECEIVED == task->ready) {
    QW_UNLOCK(QW_WRITE, &task->lock);

    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return TSDB_CODE_SUCCESS;
  } else if (QW_READY_RECEIVED == task->ready) {
    QW_ERR_JRET(qwBuildAndSendReadyRsp(pMsg, rspCode));

    task->ready = QW_READY_RESPONSED;
  } else if (QW_READY_RESPONSED == task->ready) {
    qError("query response already send");
    QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
  } else {
    assert(0);
  }

_return:

  if (task) {
    QW_UNLOCK(QW_WRITE, &task->lock);
    qwReleaseTask(QW_READ, sch);

  }

  qwReleaseScheduler(QW_READ, mgmt);

  QW_RET(code);
}

int32_t qwSetAndSendReadyRsp(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId, SRpcMsg *pMsg) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;

  QW_ERR_RET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));

  QW_ERR_JRET(qwAcquireTask(QW_READ, sch, queryId, taskId, &task));

  QW_LOCK(QW_WRITE, &task->lock);
  if (QW_TASK_READY_RESP(task->status)) {
    QW_ERR_JRET(qwBuildAndSendReadyRsp(pMsg, task->code));

    task->ready = QW_READY_RESPONSED;
  } else {
    task->ready = QW_READY_RECEIVED;
    QW_UNLOCK(QW_WRITE, &task->lock);

    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return TSDB_CODE_SUCCESS;
  }

_return:

  if (task) {
    QW_UNLOCK(QW_WRITE, &task->lock);
    qwReleaseTask(QW_READ, sch);
  }

  qwReleaseScheduler(QW_READ, mgmt);

  QW_RET(code);
}

int32_t qwCheckTaskCancelDrop( SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId, bool *needStop) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;
  int8_t status = JOB_TASK_STATUS_CANCELLED;

  *needStop = false;

  if (qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR)) {
    return TSDB_CODE_SUCCESS;
  }

  if (qwAcquireTask(QW_READ, sch, queryId, taskId, &task)) {
    qwReleaseScheduler(QW_READ, mgmt);
    return TSDB_CODE_SUCCESS;
  }

  QW_LOCK(QW_READ, &task->lock);
  
  if ((!task->cancel) && (!task->drop)) {
    qError("no cancel or drop, but task:%"PRIx64" exists", taskId);
    
    QW_UNLOCK(QW_READ, &task->lock);
    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);

    QW_RET(TSDB_CODE_QRY_APP_ERROR);
  }

  QW_UNLOCK(QW_READ, &task->lock);

  *needStop = true;
  
  if (task->cancel) {
    QW_LOCK(QW_WRITE, &task->lock);
    qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &status);
    QW_UNLOCK(QW_WRITE, &task->lock);
  }

  if (task->drop) {
    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    return qwDropTask(mgmt, sId, queryId, taskId);
  }

  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  return TSDB_CODE_SUCCESS;
}


int32_t qwHandleFetch(SQWorkerResCache *res, SQWorkerMgmt *mgmt, uint64_t sId, uint64_t queryId, uint64_t taskId, SRpcMsg *pMsg) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;
  int32_t needRsp = true;
  void *data = NULL;

  QW_ERR_JRET(qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_RET_ERR));
  QW_ERR_JRET(qwAcquireTask(QW_READ, sch, queryId, taskId, &task));

  QW_LOCK(QW_READ, &task->lock);

  if (task->cancel || task->drop) {
    qError("task is already cancelled or dropped");
    QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
  }

  if (task->status != JOB_TASK_STATUS_EXECUTING && task->status != JOB_TASK_STATUS_PARTIAL_SUCCEED) {
    qError("invalid status %d for fetch", task->status);
    QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
  }
  
  if (QW_GOT_RES_DATA(res->data)) {
    data = res->data;
    if (QW_LOW_RES_DATA(res->data)) {
      if (task->status == JOB_TASK_STATUS_PARTIAL_SUCCEED) {
        //TODO add query back to queue
      }
    }
  } else {
    if (task->status != JOB_TASK_STATUS_EXECUTING) {
      qError("invalid status %d for fetch without res", task->status);
      QW_ERR_JRET(TSDB_CODE_QRY_APP_ERROR);
    }
    
    //TODO SET FLAG FOR QUERY TO SEND RSP WHEN RES READY

    needRsp = false;
  }

_return:
  if (task) {
    QW_UNLOCK(QW_READ, &task->lock);
    qwReleaseTask(QW_READ, sch);    
  }
  
  if (sch) {
    qwReleaseScheduler(QW_READ, mgmt);
  }

  if (needRsp) {
    qwBuildAndSendFetchRsp(pMsg, res->data);
  }

  QW_RET(code);
}

int32_t qwQueryPostProcess(SQWorkerMgmt *mgmt, uint64_t sId, uint64_t qId, uint64_t tId, int8_t status, int32_t errCode) {
  SQWSchStatus *sch = NULL;
  SQWTaskStatus *task = NULL;
  int32_t code = 0;
  int8_t newStatus = JOB_TASK_STATUS_CANCELLED;

  code = qwAcquireScheduler(QW_READ, mgmt, sId, &sch, QW_NOT_EXIST_ADD);
  if (code) {
    qError("sId:%"PRIx64" not in cache", sId);
    QW_ERR_RET(code);
  }

  code = qwAcquireTask(QW_READ, sch, qId, tId, &task);
  if (code) {
    qwReleaseScheduler(QW_READ, mgmt);
    
    if (JOB_TASK_STATUS_PARTIAL_SUCCEED == status || JOB_TASK_STATUS_SUCCEED == status) {
      qError("sId:%"PRIx64" queryId:%"PRIx64" taskId:%"PRIx64" not in cache", sId, qId, tId);
      QW_ERR_RET(code);
    }

    QW_ERR_RET(qwAddTask(mgmt, sId, qId, tId, status, QW_EXIST_ACQUIRE, &sch, &task));
  }

  if (task->cancel) {
    QW_LOCK(QW_WRITE, &task->lock);
    qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &newStatus);
    QW_UNLOCK(QW_WRITE, &task->lock);
  }

  if (task->drop) {
    qwReleaseTask(QW_READ, sch);
    qwReleaseScheduler(QW_READ, mgmt);
    
    qwDropTask(mgmt, sId, qId, tId);

    return TSDB_CODE_SUCCESS;
  }

  if (!(task->cancel || task->drop)) {
    QW_LOCK(QW_WRITE, &task->lock);
    qwUpdateTaskInfo(task, QW_TASK_INFO_STATUS, &status);
    task->code = errCode;
    QW_UNLOCK(QW_WRITE, &task->lock);
  }
  
  qwReleaseTask(QW_READ, sch);
  qwReleaseScheduler(QW_READ, mgmt);

  return TSDB_CODE_SUCCESS;
}


int32_t qWorkerInit(SQWorkerCfg *cfg, void **qWorkerMgmt) {
  SQWorkerMgmt *mgmt = calloc(1, sizeof(SQWorkerMgmt));
  if (NULL == mgmt) {
    qError("calloc %d failed", (int32_t)sizeof(SQWorkerMgmt));
    QW_RET(TSDB_CODE_QRY_OUT_OF_MEMORY);
  }

  if (cfg) {
    mgmt->cfg = *cfg;
  } else {
    mgmt->cfg.maxSchedulerNum = QWORKER_DEFAULT_SCHEDULER_NUMBER;
    mgmt->cfg.maxResCacheNum = QWORKER_DEFAULT_RES_CACHE_NUMBER;
    mgmt->cfg.maxSchTaskNum = QWORKER_DEFAULT_SCH_TASK_NUMBER;
  }

  mgmt->schHash = taosHashInit(mgmt->cfg.maxSchedulerNum, taosGetDefaultHashFunction(TSDB_DATA_TYPE_UBIGINT), false, HASH_NO_LOCK);
  if (NULL == mgmt->schHash) {
    tfree(mgmt);
    QW_ERR_LRET(TSDB_CODE_QRY_OUT_OF_MEMORY, "init %d schduler hash failed", mgmt->cfg.maxSchedulerNum);
  }

  mgmt->resHash = taosHashInit(mgmt->cfg.maxResCacheNum, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), false, HASH_NO_LOCK);
  if (NULL == mgmt->resHash) {
    taosHashCleanup(mgmt->schHash);
    mgmt->schHash = NULL;
    tfree(mgmt);
    
    QW_ERR_LRET(TSDB_CODE_QRY_OUT_OF_MEMORY, "init %d res cache hash failed", mgmt->cfg.maxResCacheNum);
  }

  *qWorkerMgmt = mgmt;

  return TSDB_CODE_SUCCESS;
}

int32_t qWorkerProcessQueryMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }

  int32_t code = 0;
  SSubQueryMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen <= sizeof(*msg)) {
    qError("invalid query msg");
    QW_ERR_JRET(TSDB_CODE_QRY_INVALID_INPUT);
  }

  msg->sId = htobe64(msg->sId);
  msg->queryId = htobe64(msg->queryId);
  msg->taskId = htobe64(msg->taskId);
  msg->contentLen = ntohl(msg->contentLen);
  
  bool queryDone = false;
  bool queryRsped = false;
  bool needStop = false;
  SSubplan *plan = NULL;

  QW_ERR_JRET(qwCheckTaskCancelDrop(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, &needStop));
  if (needStop) {
    qWarn("task need stop");
    QW_ERR_JRET(TSDB_CODE_QRY_TASK_CANCELLED);
  }
  
  code = qStringToSubplan(msg->msg, &plan);
  if (TSDB_CODE_SUCCESS != code) {
    qError("schId:%"PRIx64",qId:%"PRIx64",taskId:%"PRIx64" string to subplan failed, code:%d", msg->sId, msg->queryId, msg->taskId, code);
    QW_ERR_JRET(code);
  }

  //TODO call executer to init subquery
  code = 0; // return error directly
  //TODO call executer to init subquery
  
  if (code) {
    QW_ERR_JRET(code);
  } else {
    QW_ERR_JRET(qwAddTask(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, JOB_TASK_STATUS_EXECUTING, QW_EXIST_RET_ERR, NULL, NULL));
  }

  QW_ERR_JRET(qwBuildAndSendQueryRsp(pMsg, TSDB_CODE_SUCCESS));

  queryRsped = true;
 
  //TODO call executer to execute subquery
  code = 0; 
  void *data = NULL;
  queryDone = false;
  //TODO call executer to execute subquery

  if (code) {
    QW_ERR_JRET(code);
  } else {
    QW_ERR_JRET(qwAddTaskResCache(qWorkerMgmt, msg->queryId, msg->taskId, data));

    QW_ERR_JRET(qwUpdateTaskStatus(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, JOB_TASK_STATUS_PARTIAL_SUCCEED));
  } 

_return:

  if (queryRsped) {
    code = qwCheckAndSendReadyRsp(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, pMsg, code);
  } else {
    code = qwBuildAndSendQueryRsp(pMsg, code);
  }
  
  int8_t status = 0;
  if (TSDB_CODE_SUCCESS != code) {
    status = JOB_TASK_STATUS_FAILED;
  } else if (queryDone) {
    status = JOB_TASK_STATUS_SUCCEED;
  } else {
    status = JOB_TASK_STATUS_PARTIAL_SUCCEED;
  }

  qwQueryPostProcess(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, status, code);
  
  QW_RET(code);
}

int32_t qWorkerProcessReadyMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg){
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    return TSDB_CODE_QRY_INVALID_INPUT;
  }

  SResReadyMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen < sizeof(*msg)) {
    qError("invalid task status msg");  
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }  

  msg->sId = htobe64(msg->sId);
  msg->queryId = htobe64(msg->queryId);
  msg->taskId = htobe64(msg->taskId);

  QW_ERR_RET(qwSetAndSendReadyRsp(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, pMsg));
  
  return TSDB_CODE_SUCCESS;
}

int32_t qWorkerProcessStatusMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    return TSDB_CODE_QRY_INVALID_INPUT;
  }

  int32_t code = 0;
  SSchTasksStatusMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen < sizeof(*msg)) {
    qError("invalid task status msg");
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }  

  msg->sId = htobe64(msg->sId);

  SSchedulerStatusRsp *sStatus = NULL;
  
  QW_ERR_JRET(qwGetSchTasksStatus(qWorkerMgmt, msg->sId, &sStatus));

_return:

  QW_ERR_RET(qwBuildAndSendStatusRsp(pMsg, sStatus));

  return TSDB_CODE_SUCCESS;
}

int32_t qWorkerProcessFetchMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    return TSDB_CODE_QRY_INVALID_INPUT;
  }

  SResFetchMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen < sizeof(*msg)) {
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }  

  msg->sId = htobe64(msg->sId);
  msg->queryId = htobe64(msg->queryId);
  msg->taskId = htobe64(msg->taskId);

  QW_ERR_RET(qwUpdateSchLastAccess(qWorkerMgmt, msg->sId));

  void *data = NULL;
  SQWorkerResCache *res = NULL;
  int32_t code = 0;
  
  QW_ERR_RET(qwAcquireTaskResCache(QW_READ, qWorkerMgmt, msg->queryId, msg->taskId, &res));

  QW_ERR_JRET(qwHandleFetch(res, qWorkerMgmt, msg->sId, msg->queryId, msg->taskId, pMsg));

_return:

  qwReleaseTaskResCache(QW_READ, qWorkerMgmt);
  
  QW_RET(code);
}

int32_t qWorkerProcessCancelMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    return TSDB_CODE_QRY_INVALID_INPUT;
  }

  int32_t code = 0;
  STaskCancelMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen < sizeof(*msg)) {
    qError("invalid task cancel msg");  
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }  

  msg->sId = htobe64(msg->sId);
  msg->queryId = htobe64(msg->queryId);
  msg->taskId = htobe64(msg->taskId);

  QW_ERR_JRET(qwCancelTask(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId));

_return:

  QW_ERR_RET(qwBuildAndSendCancelRsp(pMsg, code));

  return TSDB_CODE_SUCCESS;
}

int32_t qWorkerProcessDropMsg(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  if (NULL == node || NULL == qWorkerMgmt || NULL == pMsg) {
    return TSDB_CODE_QRY_INVALID_INPUT;
  }

  int32_t code = 0;
  STaskDropMsg *msg = pMsg->pCont;
  if (NULL == msg || pMsg->contLen < sizeof(*msg)) {
    qError("invalid task drop msg");
    QW_ERR_RET(TSDB_CODE_QRY_INVALID_INPUT);
  }  

  msg->sId = htobe64(msg->sId);
  msg->queryId = htobe64(msg->queryId);
  msg->taskId = htobe64(msg->taskId);

  QW_ERR_JRET(qwCancelDropTask(qWorkerMgmt, msg->sId, msg->queryId, msg->taskId));

_return:

  QW_ERR_RET(qwBuildAndSendDropRsp(pMsg, code));

  return TSDB_CODE_SUCCESS;
}

int32_t qWorkerContinueQuery(void *node, void *qWorkerMgmt, SRpcMsg *pMsg) {
  int32_t code = 0;
  int8_t status = 0;
  bool queryDone = false;
  uint64_t sId, qId, tId;

  //TODO call executer to continue execute subquery
  code = 0; 
  void *data = NULL;
  queryDone = false;
  //TODO call executer to continue execute subquery
  
  if (TSDB_CODE_SUCCESS != code) {
    status = JOB_TASK_STATUS_FAILED;
  } else if (queryDone) {
    status = JOB_TASK_STATUS_SUCCEED;
  } else {
    status = JOB_TASK_STATUS_PARTIAL_SUCCEED;
  }

  code = qwQueryPostProcess(qWorkerMgmt, sId, qId, tId, status, code);

  QW_RET(code);
}


void qWorkerDestroy(void **qWorkerMgmt) {
  if (NULL == qWorkerMgmt || NULL == *qWorkerMgmt) {
    return;
  }

  SQWorkerMgmt *mgmt = *qWorkerMgmt;
  
  //TODO STOP ALL QUERY

  //TODO FREE ALL

  tfree(*qWorkerMgmt);
}



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
#include "mndSnode.h"
#include "mndDnode.h"
#include "mndShow.h"
#include "mndTrans.h"

#define TSDB_SNODE_VER_NUMBER 1
#define TSDB_SNODE_RESERVE_SIZE 64

static SSdbRaw *mndSnodeActionEncode(SSnodeObj *pObj);
static SSdbRow *mndSnodeActionDecode(SSdbRaw *pRaw);
static int32_t  mndSnodeActionInsert(SSdb *pSdb, SSnodeObj *pObj);
static int32_t  mndSnodeActionDelete(SSdb *pSdb, SSnodeObj *pObj);
static int32_t  mndSnodeActionUpdate(SSdb *pSdb, SSnodeObj *pOld, SSnodeObj *pNew);
static int32_t  mndProcessCreateSnodeReq(SMnodeMsg *pReq);
static int32_t  mndProcessDropSnodeReq(SMnodeMsg *pReq);
static int32_t  mndProcessCreateSnodeRsp(SMnodeMsg *pRsp);
static int32_t  mndProcessDropSnodeRsp(SMnodeMsg *pRsp);
static int32_t  mndGetSnodeMeta(SMnodeMsg *pReq, SShowObj *pShow, STableMetaRsp *pMeta);
static int32_t  mndRetrieveSnodes(SMnodeMsg *pReq, SShowObj *pShow, char *data, int32_t rows);
static void     mndCancelGetNextSnode(SMnode *pMnode, void *pIter);

int32_t mndInitSnode(SMnode *pMnode) {
  SSdbTable table = {.sdbType = SDB_SNODE,
                     .keyType = SDB_KEY_INT32,
                     .encodeFp = (SdbEncodeFp)mndSnodeActionEncode,
                     .decodeFp = (SdbDecodeFp)mndSnodeActionDecode,
                     .insertFp = (SdbInsertFp)mndSnodeActionInsert,
                     .updateFp = (SdbUpdateFp)mndSnodeActionUpdate,
                     .deleteFp = (SdbDeleteFp)mndSnodeActionDelete};

  mndSetMsgHandle(pMnode, TDMT_MND_CREATE_SNODE, mndProcessCreateSnodeReq);
  mndSetMsgHandle(pMnode, TDMT_MND_DROP_SNODE, mndProcessDropSnodeReq);
  mndSetMsgHandle(pMnode, TDMT_DND_CREATE_SNODE_RSP, mndProcessCreateSnodeRsp);
  mndSetMsgHandle(pMnode, TDMT_DND_DROP_SNODE_RSP, mndProcessDropSnodeRsp);

  mndAddShowMetaHandle(pMnode, TSDB_MGMT_TABLE_SNODE, mndGetSnodeMeta);
  mndAddShowRetrieveHandle(pMnode, TSDB_MGMT_TABLE_SNODE, mndRetrieveSnodes);
  mndAddShowFreeIterHandle(pMnode, TSDB_MGMT_TABLE_SNODE, mndCancelGetNextSnode);

  return sdbSetTable(pMnode->pSdb, table);
}

void mndCleanupSnode(SMnode *pMnode) {}

static SSnodeObj *mndAcquireSnode(SMnode *pMnode, int32_t snodeId) {
  SSnodeObj *pObj = sdbAcquire(pMnode->pSdb, SDB_SNODE, &snodeId);
  if (pObj == NULL && terrno == TSDB_CODE_SDB_OBJ_NOT_THERE) {
    terrno = TSDB_CODE_MND_SNODE_NOT_EXIST;
  }
  return pObj;
}

static void mndReleaseSnode(SMnode *pMnode, SSnodeObj *pObj) {
  SSdb *pSdb = pMnode->pSdb;
  sdbRelease(pSdb, pObj);
}

static SSdbRaw *mndSnodeActionEncode(SSnodeObj *pObj) {
  terrno = TSDB_CODE_OUT_OF_MEMORY;

  SSdbRaw *pRaw = sdbAllocRaw(SDB_SNODE, TSDB_SNODE_VER_NUMBER, sizeof(SSnodeObj) + TSDB_SNODE_RESERVE_SIZE);
  if (pRaw == NULL) goto SNODE_ENCODE_OVER;

  int32_t dataPos = 0;
  SDB_SET_INT32(pRaw, dataPos, pObj->id, SNODE_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pObj->createdTime, SNODE_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pObj->updateTime, SNODE_ENCODE_OVER)
  SDB_SET_RESERVE(pRaw, dataPos, TSDB_SNODE_RESERVE_SIZE, SNODE_ENCODE_OVER)

  terrno = 0;

SNODE_ENCODE_OVER:
  if (terrno != 0) {
    mError("snode:%d, failed to encode to raw:%p since %s", pObj->id, pRaw, terrstr());
    sdbFreeRaw(pRaw);
    return NULL;
  }

  mTrace("snode:%d, encode to raw:%p, row:%p", pObj->id, pRaw, pObj);
  return pRaw;
}

static SSdbRow *mndSnodeActionDecode(SSdbRaw *pRaw) {
  terrno = TSDB_CODE_OUT_OF_MEMORY;

  int8_t sver = 0;
  if (sdbGetRawSoftVer(pRaw, &sver) != 0) goto SNODE_DECODE_OVER;

  if (sver != TSDB_SNODE_VER_NUMBER) {
    terrno = TSDB_CODE_SDB_INVALID_DATA_VER;
    goto SNODE_DECODE_OVER;
  }

  SSdbRow *pRow = sdbAllocRow(sizeof(SSnodeObj));
  if (pRow == NULL) goto SNODE_DECODE_OVER;

  SSnodeObj *pObj = sdbGetRowObj(pRow);
  if (pObj == NULL) goto SNODE_DECODE_OVER;

  int32_t dataPos = 0;
  SDB_GET_INT32(pRaw, dataPos, &pObj->id, SNODE_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pObj->createdTime, SNODE_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pObj->updateTime, SNODE_DECODE_OVER)
  SDB_GET_RESERVE(pRaw, dataPos, TSDB_SNODE_RESERVE_SIZE, SNODE_DECODE_OVER)

  terrno = 0;

SNODE_DECODE_OVER:
  if (terrno != 0) {
    mError("snode:%d, failed to decode from raw:%p since %s", pObj->id, pRaw, terrstr());
    tfree(pRow);
    return NULL;
  }

  mTrace("snode:%d, decode from raw:%p, row:%p", pObj->id, pRaw, pObj);
  return pRow;
}

static int32_t mndSnodeActionInsert(SSdb *pSdb, SSnodeObj *pObj) {
  mTrace("snode:%d, perform insert action, row:%p", pObj->id, pObj);
  pObj->pDnode = sdbAcquire(pSdb, SDB_DNODE, &pObj->id);
  if (pObj->pDnode == NULL) {
    terrno = TSDB_CODE_MND_DNODE_NOT_EXIST;
    mError("snode:%d, failed to perform insert action since %s", pObj->id, terrstr());
    return -1;
  }

  return 0;
}

static int32_t mndSnodeActionDelete(SSdb *pSdb, SSnodeObj *pObj) {
  mTrace("snode:%d, perform delete action, row:%p", pObj->id, pObj);
  if (pObj->pDnode != NULL) {
    sdbRelease(pSdb, pObj->pDnode);
    pObj->pDnode = NULL;
  }

  return 0;
}

static int32_t mndSnodeActionUpdate(SSdb *pSdb, SSnodeObj *pOld, SSnodeObj *pNew) {
  mTrace("snode:%d, perform update action, old row:%p new row:%p", pOld->id, pOld, pNew);
  pOld->updateTime = pNew->updateTime;
  return 0;
}

static int32_t mndSetCreateSnodeRedoLogs(STrans *pTrans, SSnodeObj *pObj) {
  SSdbRaw *pRedoRaw = mndSnodeActionEncode(pObj);
  if (pRedoRaw == NULL) return -1;
  if (mndTransAppendRedolog(pTrans, pRedoRaw) != 0) return -1;
  if (sdbSetRawStatus(pRedoRaw, SDB_STATUS_CREATING) != 0) return -1;
  return 0;
}

static int32_t mndSetCreateSnodeUndoLogs(STrans *pTrans, SSnodeObj *pObj) {
  SSdbRaw *pUndoRaw = mndSnodeActionEncode(pObj);
  if (pUndoRaw == NULL) return -1;
  if (mndTransAppendUndolog(pTrans, pUndoRaw) != 0) return -1;
  if (sdbSetRawStatus(pUndoRaw, SDB_STATUS_DROPPED) != 0) return -1;
  return 0;
}

static int32_t mndSetCreateSnodeCommitLogs(STrans *pTrans, SSnodeObj *pObj) {
  SSdbRaw *pCommitRaw = mndSnodeActionEncode(pObj);
  if (pCommitRaw == NULL) return -1;
  if (mndTransAppendCommitlog(pTrans, pCommitRaw) != 0) return -1;
  if (sdbSetRawStatus(pCommitRaw, SDB_STATUS_READY) != 0) return -1;
  return 0;
}

static int32_t mndSetCreateSnodeRedoActions(STrans *pTrans, SDnodeObj *pDnode, SSnodeObj *pObj) {
  SDCreateSnodeReq *pReq = malloc(sizeof(SDCreateSnodeReq));
  if (pReq == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  pReq->dnodeId = htonl(pDnode->id);

  STransAction action = {0};
  action.epSet = mndGetDnodeEpset(pDnode);
  action.pCont = pReq;
  action.contLen = sizeof(SDCreateSnodeReq);
  action.msgType = TDMT_DND_CREATE_SNODE;
  action.acceptableCode = TSDB_CODE_DND_SNODE_ALREADY_DEPLOYED;

  if (mndTransAppendRedoAction(pTrans, &action) != 0) {
    free(pReq);
    return -1;
  }

  return 0;
}

static int32_t mndSetCreateSnodeUndoActions(STrans *pTrans, SDnodeObj *pDnode, SSnodeObj *pObj) {
  SDDropSnodeReq *pReq = malloc(sizeof(SDDropSnodeReq));
  if (pReq == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  pReq->dnodeId = htonl(pDnode->id);

  STransAction action = {0};
  action.epSet = mndGetDnodeEpset(pDnode);
  action.pCont = pReq;
  action.contLen = sizeof(SDDropSnodeReq);
  action.msgType = TDMT_DND_DROP_SNODE;
  action.acceptableCode = TSDB_CODE_DND_SNODE_NOT_DEPLOYED;

  if (mndTransAppendUndoAction(pTrans, &action) != 0) {
    free(pReq);
    return -1;
  }

  return 0;
}

static int32_t mndCreateSnode(SMnode *pMnode, SMnodeMsg *pReq, SDnodeObj *pDnode, SMCreateSnodeReq *pCreate) {
  int32_t code = -1;

  SSnodeObj snodeObj = {0};
  snodeObj.id = pDnode->id;
  snodeObj.createdTime = taosGetTimestampMs();
  snodeObj.updateTime = snodeObj.createdTime;

  STrans *pTrans = mndTransCreate(pMnode, TRN_POLICY_ROLLBACK, &pReq->rpcMsg);
  if (pTrans == NULL) goto CREATE_SNODE_OVER;

  mDebug("trans:%d, used to create snode:%d", pTrans->id, pCreate->dnodeId);

  if (mndSetCreateSnodeRedoLogs(pTrans, &snodeObj) != 0) goto CREATE_SNODE_OVER;
  if (mndSetCreateSnodeUndoLogs(pTrans, &snodeObj) != 0) goto CREATE_SNODE_OVER;
  if (mndSetCreateSnodeCommitLogs(pTrans, &snodeObj) != 0) goto CREATE_SNODE_OVER;
  if (mndSetCreateSnodeRedoActions(pTrans, pDnode, &snodeObj) != 0) goto CREATE_SNODE_OVER;
  if (mndSetCreateSnodeUndoActions(pTrans, pDnode, &snodeObj) != 0) goto CREATE_SNODE_OVER;
  if (mndTransPrepare(pMnode, pTrans) != 0) goto CREATE_SNODE_OVER;

  code = 0;

CREATE_SNODE_OVER:
  mndTransDrop(pTrans);
  return code;
}

static int32_t mndProcessCreateSnodeReq(SMnodeMsg *pReq) {
  SMnode           *pMnode = pReq->pMnode;
  SMCreateSnodeReq *pCreate = pReq->rpcMsg.pCont;

  pCreate->dnodeId = htonl(pCreate->dnodeId);

  mDebug("snode:%d, start to create", pCreate->dnodeId);

  SSnodeObj *pObj = mndAcquireSnode(pMnode, pCreate->dnodeId);
  if (pObj != NULL) {
    mError("snode:%d, snode already exist", pObj->id);
    terrno = TSDB_CODE_MND_SNODE_ALREADY_EXIST;
    mndReleaseSnode(pMnode, pObj);
    return -1;
  } else if (terrno != TSDB_CODE_MND_SNODE_NOT_EXIST) {
    mError("snode:%d, failed to create snode since %s", pCreate->dnodeId, terrstr());
    return -1;
  }

  SDnodeObj *pDnode = mndAcquireDnode(pMnode, pCreate->dnodeId);
  if (pDnode == NULL) {
    mError("snode:%d, dnode not exist", pCreate->dnodeId);
    terrno = TSDB_CODE_MND_DNODE_NOT_EXIST;
    return -1;
  }

  int32_t code = mndCreateSnode(pMnode, pReq, pDnode, pCreate);
  mndReleaseDnode(pMnode, pDnode);

  if (code != 0) {
    mError("snode:%d, failed to create since %s", pCreate->dnodeId, terrstr());
    return -1;
  }

  return TSDB_CODE_MND_ACTION_IN_PROGRESS;
}

static int32_t mndSetDropSnodeRedoLogs(STrans *pTrans, SSnodeObj *pObj) {
  SSdbRaw *pRedoRaw = mndSnodeActionEncode(pObj);
  if (pRedoRaw == NULL) return -1;
  if (mndTransAppendRedolog(pTrans, pRedoRaw) != 0) return -1;
  if (sdbSetRawStatus(pRedoRaw, SDB_STATUS_DROPPING) != 0) return -1;
  return 0;
}

static int32_t mndSetDropSnodeCommitLogs(STrans *pTrans, SSnodeObj *pObj) {
  SSdbRaw *pCommitRaw = mndSnodeActionEncode(pObj);
  if (pCommitRaw == NULL) return -1;
  if (mndTransAppendCommitlog(pTrans, pCommitRaw) != 0) return -1;
  if (sdbSetRawStatus(pCommitRaw, SDB_STATUS_DROPPED) != 0) return -1;
  return 0;
}

static int32_t mndSetDropSnodeRedoActions(STrans *pTrans, SDnodeObj *pDnode, SSnodeObj *pObj) {
  SDDropSnodeReq *pReq = malloc(sizeof(SDDropSnodeReq));
  if (pReq == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  pReq->dnodeId = htonl(pDnode->id);

  STransAction action = {0};
  action.epSet = mndGetDnodeEpset(pDnode);
  action.pCont = pReq;
  action.contLen = sizeof(SDDropSnodeReq);
  action.msgType = TDMT_DND_DROP_SNODE;
  action.acceptableCode = TSDB_CODE_DND_SNODE_NOT_DEPLOYED;

  if (mndTransAppendRedoAction(pTrans, &action) != 0) {
    free(pReq);
    return -1;
  }

  return 0;
}

static int32_t mndDropSnode(SMnode *pMnode, SMnodeMsg *pReq, SSnodeObj *pObj) {
  int32_t code = -1;

  STrans *pTrans = mndTransCreate(pMnode, TRN_POLICY_RETRY, &pReq->rpcMsg);
  if (pTrans == NULL) goto DROP_SNODE_OVER;

  mDebug("trans:%d, used to drop snode:%d", pTrans->id, pObj->id);

  if (mndSetDropSnodeRedoLogs(pTrans, pObj) != 0) goto DROP_SNODE_OVER;
  if (mndSetDropSnodeCommitLogs(pTrans, pObj) != 0) goto DROP_SNODE_OVER;
  if (mndSetDropSnodeRedoActions(pTrans, pObj->pDnode, pObj) != 0) goto DROP_SNODE_OVER;
  if (mndTransPrepare(pMnode, pTrans) != 0) goto DROP_SNODE_OVER;

  code = 0;

DROP_SNODE_OVER:
  mndTransDrop(pTrans);
  return code;
}

static int32_t mndProcessDropSnodeReq(SMnodeMsg *pReq) {
  SMnode         *pMnode = pReq->pMnode;
  SMDropSnodeReq *pDrop = pReq->rpcMsg.pCont;
  pDrop->dnodeId = htonl(pDrop->dnodeId);

  mDebug("snode:%d, start to drop", pDrop->dnodeId);

  if (pDrop->dnodeId <= 0) {
    terrno = TSDB_CODE_SDB_APP_ERROR;
    mError("snode:%d, failed to drop since %s", pDrop->dnodeId, terrstr());
    return -1;
  }

  SSnodeObj *pObj = mndAcquireSnode(pMnode, pDrop->dnodeId);
  if (pObj == NULL) {
    mError("snode:%d, failed to drop since %s", pDrop->dnodeId, terrstr());
    return -1;
  }

  int32_t code = mndDropSnode(pMnode, pReq, pObj);
  if (code != 0) {
    sdbRelease(pMnode->pSdb, pObj);
    mError("snode:%d, failed to drop since %s", pMnode->dnodeId, terrstr());
    return -1;
  }

  sdbRelease(pMnode->pSdb, pObj);
  return TSDB_CODE_MND_ACTION_IN_PROGRESS;
}

static int32_t mndProcessCreateSnodeRsp(SMnodeMsg *pRsp) {
  mndTransProcessRsp(pRsp);
  return 0;
}

static int32_t mndProcessDropSnodeRsp(SMnodeMsg *pRsp) {
  mndTransProcessRsp(pRsp);
  return 0;
}

static int32_t mndGetSnodeMeta(SMnodeMsg *pReq, SShowObj *pShow, STableMetaRsp *pMeta) {
  SMnode *pMnode = pReq->pMnode;
  SSdb   *pSdb = pMnode->pSdb;

  int32_t  cols = 0;
  SSchema *pSchema = pMeta->pSchema;

  pShow->bytes[cols] = 2;
  pSchema[cols].type = TSDB_DATA_TYPE_SMALLINT;
  strcpy(pSchema[cols].name, "id");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = TSDB_EP_LEN + VARSTR_HEADER_SIZE;
  pSchema[cols].type = TSDB_DATA_TYPE_BINARY;
  strcpy(pSchema[cols].name, "endpoint");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 8;
  pSchema[cols].type = TSDB_DATA_TYPE_TIMESTAMP;
  strcpy(pSchema[cols].name, "create_time");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pMeta->numOfColumns = htonl(cols);
  pShow->numOfColumns = cols;

  pShow->offset[0] = 0;
  for (int32_t i = 1; i < cols; ++i) {
    pShow->offset[i] = pShow->offset[i - 1] + pShow->bytes[i - 1];
  }

  pShow->numOfRows = sdbGetSize(pSdb, SDB_SNODE);
  pShow->rowSize = pShow->offset[cols - 1] + pShow->bytes[cols - 1];
  strcpy(pMeta->tbFname, mndShowStr(pShow->type));

  return 0;
}

static int32_t mndRetrieveSnodes(SMnodeMsg *pReq, SShowObj *pShow, char *data, int32_t rows) {
  SMnode    *pMnode = pReq->pMnode;
  SSdb      *pSdb = pMnode->pSdb;
  int32_t    numOfRows = 0;
  int32_t    cols = 0;
  SSnodeObj *pObj = NULL;
  char      *pWrite;

  while (numOfRows < rows) {
    pShow->pIter = sdbFetch(pSdb, SDB_SNODE, pShow->pIter, (void **)&pObj);
    if (pShow->pIter == NULL) break;

    cols = 0;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    *(int16_t *)pWrite = pObj->id;
    cols++;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    STR_WITH_MAXSIZE_TO_VARSTR(pWrite, pObj->pDnode->ep, pShow->bytes[cols]);

    cols++;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    *(int64_t *)pWrite = pObj->createdTime;
    cols++;

    numOfRows++;
    sdbRelease(pSdb, pObj);
  }

  mndVacuumResult(data, pShow->numOfColumns, numOfRows, rows, pShow);
  pShow->numOfReads += numOfRows;

  return numOfRows;
}

static void mndCancelGetNextSnode(SMnode *pMnode, void *pIter) {
  SSdb *pSdb = pMnode->pSdb;
  sdbCancelFetch(pSdb, pIter);
}

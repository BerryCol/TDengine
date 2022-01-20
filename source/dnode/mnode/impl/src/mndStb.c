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
#include "mndStb.h"
#include "mndDb.h"
#include "mndDnode.h"
#include "mndMnode.h"
#include "mndShow.h"
#include "mndTrans.h"
#include "mndUser.h"
#include "mndVgroup.h"
#include "tname.h"

#define TSDB_STB_VER_NUMBER 1
#define TSDB_STB_RESERVE_SIZE 64

static SSdbRaw *mndStbActionEncode(SStbObj *pStb);
static SSdbRow *mndStbActionDecode(SSdbRaw *pRaw);
static int32_t  mndStbActionInsert(SSdb *pSdb, SStbObj *pStb);
static int32_t  mndStbActionDelete(SSdb *pSdb, SStbObj *pStb);
static int32_t  mndStbActionUpdate(SSdb *pSdb, SStbObj *pOldStb, SStbObj *pNewStb);
static int32_t  mndProcesSMCreateStbReq(SMnodeMsg *pMsg);
static int32_t  mndProcesSMAlterStbReq(SMnodeMsg *pMsg);
static int32_t  mndProcesSMDropStbReq(SMnodeMsg *pMsg);
static int32_t  mndProcessCreateStbInRsp(SMnodeMsg *pMsg);
static int32_t  mndProcessAlterStbInRsp(SMnodeMsg *pMsg);
static int32_t  mndProcessDropStbInRsp(SMnodeMsg *pMsg);
static int32_t  mndProcessStbMetaMsg(SMnodeMsg *pMsg);
static int32_t  mndGetStbMeta(SMnodeMsg *pMsg, SShowObj *pShow, STableMetaRsp *pMeta);
static int32_t  mndRetrieveStb(SMnodeMsg *pMsg, SShowObj *pShow, char *data, int32_t rows);
static void     mndCancelGetNextStb(SMnode *pMnode, void *pIter);

int32_t mndInitStb(SMnode *pMnode) {
  SSdbTable table = {.sdbType = SDB_STB,
                     .keyType = SDB_KEY_BINARY,
                     .encodeFp = (SdbEncodeFp)mndStbActionEncode,
                     .decodeFp = (SdbDecodeFp)mndStbActionDecode,
                     .insertFp = (SdbInsertFp)mndStbActionInsert,
                     .updateFp = (SdbUpdateFp)mndStbActionUpdate,
                     .deleteFp = (SdbDeleteFp)mndStbActionDelete};

  mndSetMsgHandle(pMnode, TDMT_MND_CREATE_STB, mndProcesSMCreateStbReq);
  mndSetMsgHandle(pMnode, TDMT_MND_ALTER_STB, mndProcesSMAlterStbReq);
  mndSetMsgHandle(pMnode, TDMT_MND_DROP_STB, mndProcesSMDropStbReq);
  mndSetMsgHandle(pMnode, TDMT_VND_CREATE_STB_RSP, mndProcessCreateStbInRsp);
  mndSetMsgHandle(pMnode, TDMT_VND_ALTER_STB_RSP, mndProcessAlterStbInRsp);
  mndSetMsgHandle(pMnode, TDMT_VND_DROP_STB_RSP, mndProcessDropStbInRsp);
  mndSetMsgHandle(pMnode, TDMT_MND_STB_META, mndProcessStbMetaMsg);

  mndAddShowMetaHandle(pMnode, TSDB_MGMT_TABLE_STB, mndGetStbMeta);
  mndAddShowRetrieveHandle(pMnode, TSDB_MGMT_TABLE_STB, mndRetrieveStb);
  mndAddShowFreeIterHandle(pMnode, TSDB_MGMT_TABLE_STB, mndCancelGetNextStb);

  return sdbSetTable(pMnode->pSdb, table);
}

void mndCleanupStb(SMnode *pMnode) {}

static SSdbRaw *mndStbActionEncode(SStbObj *pStb) {
  terrno = TSDB_CODE_OUT_OF_MEMORY;

  int32_t  size = sizeof(SStbObj) + (pStb->numOfColumns + pStb->numOfTags) * sizeof(SSchema) + TSDB_STB_RESERVE_SIZE;
  SSdbRaw *pRaw = sdbAllocRaw(SDB_STB, TSDB_STB_VER_NUMBER, size);
  if (pRaw == NULL) goto STB_ENCODE_OVER;

  int32_t dataPos = 0;
  SDB_SET_BINARY(pRaw, dataPos, pStb->name, TSDB_TABLE_FNAME_LEN, STB_ENCODE_OVER)
  SDB_SET_BINARY(pRaw, dataPos, pStb->db, TSDB_DB_FNAME_LEN, STB_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pStb->createdTime, STB_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pStb->updateTime, STB_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pStb->uid, STB_ENCODE_OVER)
  SDB_SET_INT64(pRaw, dataPos, pStb->dbUid, STB_ENCODE_OVER)
  SDB_SET_INT32(pRaw, dataPos, pStb->version, STB_ENCODE_OVER)
  SDB_SET_INT32(pRaw, dataPos, pStb->numOfColumns, STB_ENCODE_OVER)
  SDB_SET_INT32(pRaw, dataPos, pStb->numOfTags, STB_ENCODE_OVER)

  int32_t totalCols = pStb->numOfColumns + pStb->numOfTags;
  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pStb->pSchema[i];
    SDB_SET_INT8(pRaw, dataPos, pSchema->type, STB_ENCODE_OVER)
    SDB_SET_INT32(pRaw, dataPos, pSchema->colId, STB_ENCODE_OVER)
    SDB_SET_INT32(pRaw, dataPos, pSchema->bytes, STB_ENCODE_OVER)
    SDB_SET_BINARY(pRaw, dataPos, pSchema->name, TSDB_COL_NAME_LEN, STB_ENCODE_OVER)
  }

  SDB_SET_RESERVE(pRaw, dataPos, TSDB_STB_RESERVE_SIZE, STB_ENCODE_OVER)
  SDB_SET_DATALEN(pRaw, dataPos, STB_ENCODE_OVER)

  terrno = 0;

STB_ENCODE_OVER:
  if (terrno != 0) {
    mError("stb:%s, failed to encode to raw:%p since %s", pStb->name, pRaw, terrstr());
    sdbFreeRaw(pRaw);
    return NULL;
  }

  mTrace("stb:%s, encode to raw:%p, row:%p", pStb->name, pRaw, pStb);
  return pRaw;
}

static SSdbRow *mndStbActionDecode(SSdbRaw *pRaw) {
  terrno = TSDB_CODE_OUT_OF_MEMORY;

  int8_t sver = 0;
  if (sdbGetRawSoftVer(pRaw, &sver) != 0) goto STB_DECODE_OVER;

  if (sver != TSDB_STB_VER_NUMBER) {
    terrno = TSDB_CODE_SDB_INVALID_DATA_VER;
    goto STB_DECODE_OVER;
  }

  int32_t  size = sizeof(SStbObj) + TSDB_MAX_COLUMNS * sizeof(SSchema);
  SSdbRow *pRow = sdbAllocRow(size);
  if (pRow == NULL) goto STB_DECODE_OVER;

  SStbObj *pStb = sdbGetRowObj(pRow);
  if (pStb == NULL) goto STB_DECODE_OVER;

  int32_t dataPos = 0;
  SDB_GET_BINARY(pRaw, dataPos, pStb->name, TSDB_TABLE_FNAME_LEN, STB_DECODE_OVER)
  SDB_GET_BINARY(pRaw, dataPos, pStb->db, TSDB_DB_FNAME_LEN, STB_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pStb->createdTime, STB_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pStb->updateTime, STB_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pStb->uid, STB_DECODE_OVER)
  SDB_GET_INT64(pRaw, dataPos, &pStb->dbUid, STB_DECODE_OVER)
  SDB_GET_INT32(pRaw, dataPos, &pStb->version, STB_DECODE_OVER)
  SDB_GET_INT32(pRaw, dataPos, &pStb->numOfColumns, STB_DECODE_OVER)
  SDB_GET_INT32(pRaw, dataPos, &pStb->numOfTags, STB_DECODE_OVER)

  int32_t totalCols = pStb->numOfColumns + pStb->numOfTags;
  pStb->pSchema = calloc(totalCols, sizeof(SSchema));

  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pStb->pSchema[i];
    SDB_GET_INT8(pRaw, dataPos, &pSchema->type, STB_DECODE_OVER)
    SDB_GET_INT32(pRaw, dataPos, &pSchema->colId, STB_DECODE_OVER)
    SDB_GET_INT32(pRaw, dataPos, &pSchema->bytes, STB_DECODE_OVER)
    SDB_GET_BINARY(pRaw, dataPos, pSchema->name, TSDB_COL_NAME_LEN, STB_DECODE_OVER)
  }

  SDB_GET_RESERVE(pRaw, dataPos, TSDB_STB_RESERVE_SIZE, STB_DECODE_OVER)

  terrno = 0;

STB_DECODE_OVER:
  if (terrno != 0) {
    mError("stb:%s, failed to decode from raw:%p since %s", pStb->name, pRaw, terrstr());
    tfree(pRow);
    return NULL;
  }

  mTrace("stb:%s, decode from raw:%p, row:%p", pStb->name, pRaw, pStb);
  return pRow;
}

static int32_t mndStbActionInsert(SSdb *pSdb, SStbObj *pStb) {
  mTrace("stb:%s, perform insert action, row:%p", pStb->name, pStb);
  return 0;
}

static int32_t mndStbActionDelete(SSdb *pSdb, SStbObj *pStb) {
  mTrace("stb:%s, perform delete action, row:%p", pStb->name, pStb);
  return 0;
}

static int32_t mndStbActionUpdate(SSdb *pSdb, SStbObj *pOldStb, SStbObj *pNewStb) {
  mTrace("stb:%s, perform update action, old row:%p new row:%p", pOldStb->name, pOldStb, pNewStb);
  atomic_exchange_32(&pOldStb->updateTime, pNewStb->updateTime);
  atomic_exchange_32(&pOldStb->version, pNewStb->version);

  taosWLockLatch(&pOldStb->lock);
  pOldStb->numOfColumns = pNewStb->numOfColumns;
  pOldStb->numOfTags = pNewStb->numOfTags;
  int32_t totalCols = pNewStb->numOfTags + pNewStb->numOfColumns;
  int32_t totalSize = totalCols * sizeof(SSchema);

  if (pOldStb->numOfTags + pOldStb->numOfColumns < totalCols) {
    void *pSchema = malloc(totalSize);
    if (pSchema != NULL) {
      free(pOldStb->pSchema);
      pOldStb->pSchema = pSchema;
    }
  }

  memcpy(pOldStb->pSchema, pNewStb->pSchema, totalSize);
  taosWUnLockLatch(&pOldStb->lock);
  return 0;
}

SStbObj *mndAcquireStb(SMnode *pMnode, char *stbName) {
  SSdb    *pSdb = pMnode->pSdb;
  SStbObj *pStb = sdbAcquire(pSdb, SDB_STB, stbName);
  if (pStb == NULL && terrno == TSDB_CODE_SDB_OBJ_NOT_THERE) {
    terrno = TSDB_CODE_MND_STB_NOT_EXIST;
  }
  return pStb;
}

void mndReleaseStb(SMnode *pMnode, SStbObj *pStb) {
  SSdb *pSdb = pMnode->pSdb;
  sdbRelease(pSdb, pStb);
}

static SDbObj *mndAcquireDbByStb(SMnode *pMnode, char *stbName) {
  SName name = {0};
  tNameFromString(&name, stbName, T_NAME_ACCT | T_NAME_DB | T_NAME_TABLE);

  char db[TSDB_TABLE_FNAME_LEN] = {0};
  tNameGetFullDbName(&name, db);

  return mndAcquireDb(pMnode, db);
}

static void *mndBuildCreateStbMsg(SMnode *pMnode, SVgObj *pVgroup, SStbObj *pStb, int *pContLen) {
  SVCreateTbReq req;
  void         *buf;
  int           bsize;
  SMsgHead     *pMsgHead;

  req.ver = 0;
  SName name = {0};
  tNameFromString(&name, pStb->name, T_NAME_ACCT|T_NAME_DB|T_NAME_TABLE);

  req.name = (char*) tNameGetTableName(&name);
  req.ttl = 0;
  req.keep = 0;
  req.type = TD_SUPER_TABLE;
  req.stbCfg.suid = pStb->uid;
  req.stbCfg.nCols = pStb->numOfColumns;
  req.stbCfg.pSchema = pStb->pSchema;
  req.stbCfg.nTagCols = pStb->numOfTags;
  req.stbCfg.pTagSchema = pStb->pSchema + pStb->numOfColumns;

  bsize = tSerializeSVCreateTbReq(NULL, &req);
  buf = malloc(sizeof(SMsgHead) + bsize);
  if (buf == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return NULL;
  }

  pMsgHead = (SMsgHead *)buf;

  pMsgHead->contLen = htonl(sizeof(SMsgHead) + bsize);
  pMsgHead->vgId = htonl(pVgroup->vgId);

  void *pBuf = POINTER_SHIFT(buf, sizeof(SMsgHead));
  tSerializeSVCreateTbReq(&pBuf, &req);

  *pContLen = sizeof(SMsgHead) + bsize;
  return buf;
}

static SVDropTbReq *mndBuildDropStbMsg(SMnode *pMnode, SVgObj *pVgroup, SStbObj *pStb) {
  int32_t contLen = sizeof(SVDropTbReq);

  SVDropTbReq *pDrop = calloc(1, contLen);
  if (pDrop == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return NULL;
  }

  pDrop->head.contLen = htonl(contLen);
  pDrop->head.vgId = htonl(pVgroup->vgId);
  memcpy(pDrop->name, pStb->name, TSDB_TABLE_FNAME_LEN);
  // pDrop->suid = htobe64(pStb->uid);

  return pDrop;
}

static int32_t mndCheckCreateStbMsg(SMCreateStbReq *pCreate) {
  pCreate->numOfColumns = htonl(pCreate->numOfColumns);
  pCreate->numOfTags = htonl(pCreate->numOfTags);
  int32_t totalCols = pCreate->numOfColumns + pCreate->numOfTags;
  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pCreate->pSchema[i];
    pSchema->bytes = htonl(pSchema->bytes);
  }

  if (pCreate->igExists < 0 || pCreate->igExists > 1) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }

  if (pCreate->numOfColumns < TSDB_MIN_COLUMNS || pCreate->numOfColumns > TSDB_MAX_COLUMNS) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }

  if (pCreate->numOfTags <= 0 || pCreate->numOfTags > TSDB_MAX_TAGS) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }

  int32_t maxColId = (TSDB_MAX_COLUMNS + TSDB_MAX_TAGS);
  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pCreate->pSchema[i];
    if (pSchema->type < 0) {
      terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
      return -1;
    }
    if (pSchema->bytes <= 0) {
      terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
      return -1;
    }
    if (pSchema->name[0] == 0) {
      terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
      return -1;
    }
  }

  return 0;
}

static int32_t mndSetCreateStbRedoLogs(SMnode *pMnode, STrans *pTrans, SDbObj *pDb, SStbObj *pStb) {
  SSdbRaw *pRedoRaw = mndStbActionEncode(pStb);
  if (pRedoRaw == NULL) return -1;
  if (mndTransAppendRedolog(pTrans, pRedoRaw) != 0) return -1;
  if (sdbSetRawStatus(pRedoRaw, SDB_STATUS_CREATING) != 0) return -1;

  return 0;
}

static int32_t mndSetCreateStbUndoLogs(SMnode *pMnode, STrans *pTrans, SDbObj *pDb, SStbObj *pStb) {
  SSdbRaw *pUndoRaw = mndStbActionEncode(pStb);
  if (pUndoRaw == NULL) return -1;
  if (mndTransAppendUndolog(pTrans, pUndoRaw) != 0) return -1;
  if (sdbSetRawStatus(pUndoRaw, SDB_STATUS_DROPPED) != 0) return -1;

  return 0;
}

static int32_t mndSetCreateStbCommitLogs(SMnode *pMnode, STrans *pTrans, SDbObj *pDb, SStbObj *pStb) {
  SSdbRaw *pCommitRaw = mndStbActionEncode(pStb);
  if (pCommitRaw == NULL) return -1;
  if (mndTransAppendCommitlog(pTrans, pCommitRaw) != 0) return -1;
  if (sdbSetRawStatus(pCommitRaw, SDB_STATUS_READY) != 0) return -1;

  return 0;
}

static int32_t mndSetCreateStbRedoActions(SMnode *pMnode, STrans *pTrans, SDbObj *pDb, SStbObj *pStb) {
  SSdb   *pSdb = pMnode->pSdb;
  SVgObj *pVgroup = NULL;
  void   *pIter = NULL;
  int     contLen;

  while (1) {
    pIter = sdbFetch(pSdb, SDB_VGROUP, pIter, (void **)&pVgroup);
    if (pIter == NULL) break;
    if (pVgroup->dbUid != pDb->uid) continue;

    void *pMsg = mndBuildCreateStbMsg(pMnode, pVgroup, pStb, &contLen);
    if (pMsg == NULL) {
      sdbCancelFetch(pSdb, pIter);
      sdbRelease(pSdb, pVgroup);
      terrno = TSDB_CODE_OUT_OF_MEMORY;
      return -1;
    }

    STransAction action = {0};
    action.epSet = mndGetVgroupEpset(pMnode, pVgroup);
    action.pCont = pMsg;
    action.contLen = contLen;
    action.msgType = TDMT_VND_CREATE_STB;
    if (mndTransAppendRedoAction(pTrans, &action) != 0) {
      free(pMsg);
      sdbCancelFetch(pSdb, pIter);
      sdbRelease(pSdb, pVgroup);
      return -1;
    }
    sdbRelease(pSdb, pVgroup);
  }

  return 0;
}

static int32_t mndSetCreateStbUndoActions(SMnode *pMnode, STrans *pTrans, SDbObj *pDb, SStbObj *pStb) {
  SSdb   *pSdb = pMnode->pSdb;
  SVgObj *pVgroup = NULL;
  void   *pIter = NULL;

  while (1) {
    pIter = sdbFetch(pSdb, SDB_VGROUP, pIter, (void **)&pVgroup);
    if (pIter == NULL) break;
    if (pVgroup->dbUid != pDb->uid) continue;

    SVDropTbReq *pMsg = mndBuildDropStbMsg(pMnode, pVgroup, pStb);
    if (pMsg == NULL) {
      sdbCancelFetch(pSdb, pIter);
      sdbRelease(pSdb, pVgroup);
      terrno = TSDB_CODE_OUT_OF_MEMORY;
      return -1;
    }

    STransAction action = {0};
    action.epSet = mndGetVgroupEpset(pMnode, pVgroup);
    action.pCont = pMsg;
    action.contLen = sizeof(SVDropTbReq);
    action.msgType = TDMT_VND_DROP_STB;
    if (mndTransAppendUndoAction(pTrans, &action) != 0) {
      free(pMsg);
      sdbCancelFetch(pSdb, pIter);
      sdbRelease(pSdb, pVgroup);
      return -1;
    }
    sdbRelease(pSdb, pVgroup);
  }

  return 0;
}

static int32_t mndCreateStb(SMnode *pMnode, SMnodeMsg *pMsg, SMCreateStbReq *pCreate, SDbObj *pDb) {
  SStbObj stbObj = {0};
  tstrncpy(stbObj.name, pCreate->name, TSDB_TABLE_FNAME_LEN);
  tstrncpy(stbObj.db, pDb->name, TSDB_DB_FNAME_LEN);
  stbObj.createdTime = taosGetTimestampMs();
  stbObj.updateTime = stbObj.createdTime;
  stbObj.uid = mndGenerateUid(pCreate->name, TSDB_TABLE_FNAME_LEN);
  stbObj.dbUid = pDb->uid;
  stbObj.version = 1;
  stbObj.numOfColumns = pCreate->numOfColumns;
  stbObj.numOfTags = pCreate->numOfTags;

  int32_t totalCols = stbObj.numOfColumns + stbObj.numOfTags;
  int32_t totalSize = totalCols * sizeof(SSchema);
  stbObj.pSchema = malloc(totalSize);
  if (stbObj.pSchema == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  memcpy(stbObj.pSchema, pCreate->pSchema, totalSize);

  for (int32_t i = 0; i < totalCols; ++i) {
    stbObj.pSchema[i].colId = i + 1;
  }

  int32_t code = 0;
  STrans *pTrans = mndTransCreate(pMnode, TRN_POLICY_ROLLBACK, &pMsg->rpcMsg);
  if (pTrans == NULL) {
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
    return -1;
  }
  mDebug("trans:%d, used to create stb:%s", pTrans->id, pCreate->name);

  if (mndSetCreateStbRedoLogs(pMnode, pTrans, pDb, &stbObj) != 0) {
    mError("trans:%d, failed to set redo log since %s", pTrans->id, terrstr());
    goto CREATE_STB_OVER;
  }

  if (mndSetCreateStbUndoLogs(pMnode, pTrans, pDb, &stbObj) != 0) {
    mError("trans:%d, failed to set undo log since %s", pTrans->id, terrstr());
    goto CREATE_STB_OVER;
  }

  if (mndSetCreateStbCommitLogs(pMnode, pTrans, pDb, &stbObj) != 0) {
    mError("trans:%d, failed to set commit log since %s", pTrans->id, terrstr());
    goto CREATE_STB_OVER;
  }

  if (mndSetCreateStbRedoActions(pMnode, pTrans, pDb, &stbObj) != 0) {
    mError("trans:%d, failed to set redo actions since %s", pTrans->id, terrstr());
    goto CREATE_STB_OVER;
  }

  if (mndSetCreateStbUndoActions(pMnode, pTrans, pDb, &stbObj) != 0) {
    mError("trans:%d, failed to set redo actions since %s", pTrans->id, terrstr());
    goto CREATE_STB_OVER;
  }

  if (mndTransPrepare(pMnode, pTrans) != 0) {
    mError("trans:%d, failed to prepare since %s", pTrans->id, terrstr());
    mndTransDrop(pTrans);
    return -1;
  }

  code = 0;

CREATE_STB_OVER:
  mndTransDrop(pTrans);
  return code;
}

static int32_t mndProcesSMCreateStbReq(SMnodeMsg *pMsg) {
  SMnode        *pMnode = pMsg->pMnode;
  SMCreateStbReq *pCreate = pMsg->rpcMsg.pCont;

  mDebug("stb:%s, start to create", pCreate->name);

  if (mndCheckCreateStbMsg(pCreate) != 0) {
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
    return -1;
  }

  SStbObj *pStb = mndAcquireStb(pMnode, pCreate->name);
  if (pStb != NULL) {
    sdbRelease(pMnode->pSdb, pStb);
    if (pCreate->igExists) {
      mDebug("stb:%s, already exist, ignore exist is set", pCreate->name);
      return 0;
    } else {
      terrno = TSDB_CODE_MND_STB_ALREADY_EXIST;
      mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
      return -1;
    }
  } else if (terrno != TSDB_CODE_MND_STB_NOT_EXIST) {
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
  }

  // topic should have different name with stb
  SStbObj *pTopic = mndAcquireStb(pMnode, pCreate->name);
  if (pTopic != NULL) {
    sdbRelease(pMnode->pSdb, pTopic);
    terrno = TSDB_CODE_MND_NAME_CONFLICT_WITH_TOPIC;
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
    return -1;
  }

  SDbObj *pDb = mndAcquireDbByStb(pMnode, pCreate->name);
  if (pDb == NULL) {
    terrno = TSDB_CODE_MND_DB_NOT_SELECTED;
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
    return -1;
  }

  int32_t code = mndCreateStb(pMnode, pMsg, pCreate, pDb);
  mndReleaseDb(pMnode, pDb);

  if (code != 0) {
    terrno = code;
    mError("stb:%s, failed to create since %s", pCreate->name, terrstr());
    return -1;
  }

  return TSDB_CODE_MND_ACTION_IN_PROGRESS;
}

static int32_t mndProcessCreateStbInRsp(SMnodeMsg *pMsg) {
  mndTransProcessRsp(pMsg);
  return 0;
}

static int32_t mndCheckAlterStbMsg(SMAlterStbReq *pAlter) {
  SSchema *pSchema = &pAlter->schema;
  pSchema->colId = htonl(pSchema->colId);
  pSchema->bytes = htonl(pSchema->bytes);

  if (pSchema->type <= 0) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }
  if (pSchema->colId < 0 || pSchema->colId >= (TSDB_MAX_COLUMNS + TSDB_MAX_TAGS)) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }
  if (pSchema->bytes <= 0) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }
  if (pSchema->name[0] == 0) {
    terrno = TSDB_CODE_MND_INVALID_STB_OPTION;
    return -1;
  }

  return 0;
}

static int32_t mndUpdateStb(SMnode *pMnode, SMnodeMsg *pMsg, SStbObj *pOldStb, SStbObj *pNewStb) { return 0; }

static int32_t mndProcesSMAlterStbReq(SMnodeMsg *pMsg) {
  SMnode       *pMnode = pMsg->pMnode;
  SMAlterStbReq *pAlter = pMsg->rpcMsg.pCont;

  mDebug("stb:%s, start to alter", pAlter->name);

  if (mndCheckAlterStbMsg(pAlter) != 0) {
    mError("stb:%s, failed to alter since %s", pAlter->name, terrstr());
    return -1;
  }

  SStbObj *pStb = mndAcquireStb(pMnode, pAlter->name);
  if (pStb == NULL) {
    terrno = TSDB_CODE_MND_STB_NOT_EXIST;
    mError("stb:%s, failed to alter since %s", pAlter->name, terrstr());
    return -1;
  }

  SStbObj stbObj = {0};
  memcpy(&stbObj, pStb, sizeof(SStbObj));

  int32_t code = mndUpdateStb(pMnode, pMsg, pStb, &stbObj);
  mndReleaseStb(pMnode, pStb);

  if (code != 0) {
    mError("stb:%s, failed to alter since %s", pAlter->name, tstrerror(code));
    return code;
  }

  return TSDB_CODE_MND_ACTION_IN_PROGRESS;
}

static int32_t mndProcessAlterStbInRsp(SMnodeMsg *pMsg) {
  mndTransProcessRsp(pMsg);
  return 0;
}

static int32_t mndSetDropStbRedoLogs(SMnode *pMnode, STrans *pTrans, SStbObj *pStb) {
  SSdbRaw *pRedoRaw = mndStbActionEncode(pStb);
  if (pRedoRaw == NULL) return -1;
  if (mndTransAppendRedolog(pTrans, pRedoRaw) != 0) return -1;
  if (sdbSetRawStatus(pRedoRaw, SDB_STATUS_DROPPING) != 0) return -1;

  return 0;
}

static int32_t mndSetDropStbUndoLogs(SMnode *pMnode, STrans *pTrans, SStbObj *pStb) {
  SSdbRaw *pUndoRaw = mndStbActionEncode(pStb);
  if (pUndoRaw == NULL) return -1;
  if (mndTransAppendUndolog(pTrans, pUndoRaw) != 0) return -1;
  if (sdbSetRawStatus(pUndoRaw, SDB_STATUS_READY) != 0) return -1;

  return 0;
}

static int32_t mndSetDropStbCommitLogs(SMnode *pMnode, STrans *pTrans, SStbObj *pStb) {
  SSdbRaw *pCommitRaw = mndStbActionEncode(pStb);
  if (pCommitRaw == NULL) return -1;
  if (mndTransAppendCommitlog(pTrans, pCommitRaw) != 0) return -1;
  if (sdbSetRawStatus(pCommitRaw, SDB_STATUS_DROPPED) != 0) return -1;

  return 0;
}

static int32_t mndSetDropStbRedoActions(SMnode *pMnode, STrans *pTrans, SStbObj *pStb) { return 0; }

static int32_t mndSetDropStbUndoActions(SMnode *pMnode, STrans *pTrans, SStbObj *pStb) { return 0; }

static int32_t mndDropStb(SMnode *pMnode, SMnodeMsg *pMsg, SStbObj *pStb) {
  int32_t code = -1;
  STrans *pTrans = mndTransCreate(pMnode, TRN_POLICY_ROLLBACK, &pMsg->rpcMsg);
  if (pTrans == NULL) {
    mError("stb:%s, failed to drop since %s", pStb->name, terrstr());
    return -1;
  }
  mDebug("trans:%d, used to drop stb:%s", pTrans->id, pStb->name);

  if (mndSetDropStbRedoLogs(pMnode, pTrans, pStb) != 0) {
    mError("trans:%d, failed to set redo log since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  if (mndSetDropStbUndoLogs(pMnode, pTrans, pStb) != 0) {
    mError("trans:%d, failed to set undo log since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  if (mndSetDropStbCommitLogs(pMnode, pTrans, pStb) != 0) {
    mError("trans:%d, failed to set commit log since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  if (mndSetDropStbRedoActions(pMnode, pTrans, pStb) != 0) {
    mError("trans:%d, failed to set redo actions since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  if (mndSetDropStbUndoActions(pMnode, pTrans, pStb) != 0) {
    mError("trans:%d, failed to set redo actions since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  if (mndTransPrepare(pMnode, pTrans) != 0) {
    mError("trans:%d, failed to prepare since %s", pTrans->id, terrstr());
    goto DROP_STB_OVER;
  }

  code = 0;

DROP_STB_OVER:
  mndTransDrop(pTrans);
  return 0;
}

static int32_t mndProcesSMDropStbReq(SMnodeMsg *pMsg) {
  SMnode      *pMnode = pMsg->pMnode;
  SMDropStbReq *pDrop = pMsg->rpcMsg.pCont;

  mDebug("stb:%s, start to drop", pDrop->name);

  SStbObj *pStb = mndAcquireStb(pMnode, pDrop->name);
  if (pStb == NULL) {
    if (pDrop->igNotExists) {
      mDebug("stb:%s, not exist, ignore not exist is set", pDrop->name);
      return 0;
    } else {
      terrno = TSDB_CODE_MND_STB_NOT_EXIST;
      mError("stb:%s, failed to drop since %s", pDrop->name, terrstr());
      return -1;
    }
  }

  int32_t code = mndDropStb(pMnode, pMsg, pStb);
  mndReleaseStb(pMnode, pStb);

  if (code != 0) {
    terrno = code;
    mError("stb:%s, failed to drop since %s", pDrop->name, terrstr());
    return -1;
  }

  return TSDB_CODE_MND_ACTION_IN_PROGRESS;
}

static int32_t mndProcessDropStbInRsp(SMnodeMsg *pMsg) {
  mndTransProcessRsp(pMsg);
  return 0;
}

static int32_t mndProcessStbMetaMsg(SMnodeMsg *pMsg) {
  SMnode        *pMnode = pMsg->pMnode;
  STableInfoReq *pInfo = pMsg->rpcMsg.pCont;

  mDebug("stb:%s, start to retrieve meta", pInfo->tableFname);

  SDbObj *pDb = mndAcquireDbByStb(pMnode, pInfo->tableFname);
  if (pDb == NULL) {
    terrno = TSDB_CODE_MND_DB_NOT_SELECTED;
    mError("stb:%s, failed to retrieve meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  SStbObj *pStb = mndAcquireStb(pMnode, pInfo->tableFname);
  if (pStb == NULL) {
    mndReleaseDb(pMnode, pDb);
    terrno = TSDB_CODE_MND_INVALID_STB;
    mError("stb:%s, failed to get meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  taosRLockLatch(&pStb->lock);
  int32_t totalCols = pStb->numOfColumns + pStb->numOfTags;
  int32_t contLen = sizeof(STableMetaRsp) + totalCols * sizeof(SSchema);

  STableMetaRsp *pMeta = rpcMallocCont(contLen);
  if (pMeta == NULL) {
    taosRUnLockLatch(&pStb->lock);
    mndReleaseDb(pMnode, pDb);
    mndReleaseStb(pMnode, pStb);
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    mError("stb:%s, failed to get meta since %s", pInfo->tableFname, terrstr());
    return -1;
  }

  memcpy(pMeta->tbFname, pStb->name, TSDB_TABLE_FNAME_LEN);
  pMeta->numOfTags = htonl(pStb->numOfTags);
  pMeta->numOfColumns = htonl(pStb->numOfColumns);
  pMeta->precision = pDb->cfg.precision;
  pMeta->tableType = TSDB_SUPER_TABLE;
  pMeta->update = pDb->cfg.update;
  pMeta->sversion = htonl(pStb->version);
  pMeta->suid = htobe64(pStb->uid);
  pMeta->tuid = htobe64(pStb->uid);

  for (int32_t i = 0; i < totalCols; ++i) {
    SSchema *pSchema = &pMeta->pSchema[i];
    SSchema *pSrcSchema = &pStb->pSchema[i];
    memcpy(pSchema->name, pSrcSchema->name, TSDB_COL_NAME_LEN);
    pSchema->type = pSrcSchema->type;
    pSchema->colId = htonl(pSrcSchema->colId);
    pSchema->bytes = htonl(pSrcSchema->bytes);
  }
  taosRUnLockLatch(&pStb->lock);
  mndReleaseDb(pMnode, pDb);
  mndReleaseStb(pMnode, pStb);

  pMsg->pCont = pMeta;
  pMsg->contLen = contLen;

  mDebug("stb:%s, meta is retrieved, cols:%d tags:%d", pInfo->tableFname, pStb->numOfColumns, pStb->numOfTags);
  return 0;
}

static int32_t mndGetNumOfStbs(SMnode *pMnode, char *dbName, int32_t *pNumOfStbs) {
  SSdb *pSdb = pMnode->pSdb;

  SDbObj *pDb = mndAcquireDb(pMnode, dbName);
  if (pDb == NULL) {
    terrno = TSDB_CODE_MND_DB_NOT_SELECTED;
    return -1;
  }

  int32_t numOfStbs = 0;
  void   *pIter = NULL;
  while (1) {
    SStbObj *pStb = NULL;
    pIter = sdbFetch(pSdb, SDB_STB, pIter, (void **)&pStb);
    if (pIter == NULL) break;

    if (strcmp(pStb->db, dbName) == 0) {
      numOfStbs++;
    }

    sdbRelease(pSdb, pStb);
  }

  *pNumOfStbs = numOfStbs;
  return 0;
}

static int32_t mndGetStbMeta(SMnodeMsg *pMsg, SShowObj *pShow, STableMetaRsp *pMeta) {
  SMnode *pMnode = pMsg->pMnode;
  SSdb   *pSdb = pMnode->pSdb;

  if (mndGetNumOfStbs(pMnode, pShow->db, &pShow->numOfRows) != 0) {
    return -1;
  }

  int32_t  cols = 0;
  SSchema *pSchema = pMeta->pSchema;

  pShow->bytes[cols] = TSDB_TABLE_NAME_LEN + VARSTR_HEADER_SIZE;
  pSchema[cols].type = TSDB_DATA_TYPE_BINARY;
  strcpy(pSchema[cols].name, "name");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 8;
  pSchema[cols].type = TSDB_DATA_TYPE_TIMESTAMP;
  strcpy(pSchema[cols].name, "create_time");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 4;
  pSchema[cols].type = TSDB_DATA_TYPE_INT;
  strcpy(pSchema[cols].name, "columns");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pShow->bytes[cols] = 4;
  pSchema[cols].type = TSDB_DATA_TYPE_INT;
  strcpy(pSchema[cols].name, "tags");
  pSchema[cols].bytes = htonl(pShow->bytes[cols]);
  cols++;

  pMeta->numOfColumns = htonl(cols);
  pShow->numOfColumns = cols;

  pShow->offset[0] = 0;
  for (int32_t i = 1; i < cols; ++i) {
    pShow->offset[i] = pShow->offset[i - 1] + pShow->bytes[i - 1];
  }

  pShow->numOfRows = sdbGetSize(pSdb, SDB_STB);
  pShow->rowSize = pShow->offset[cols - 1] + pShow->bytes[cols - 1];
  strcpy(pMeta->tbFname, mndShowStr(pShow->type));

  return 0;
}

static void mndExtractTableName(char *tableId, char *name) {
  int32_t pos = -1;
  int32_t num = 0;
  for (pos = 0; tableId[pos] != 0; ++pos) {
    if (tableId[pos] == '.') num++;
    if (num == 2) break;
  }

  if (num == 2) {
    strcpy(name, tableId + pos + 1);
  }
}

static int32_t mndRetrieveStb(SMnodeMsg *pMsg, SShowObj *pShow, char *data, int32_t rows) {
  SMnode  *pMnode = pMsg->pMnode;
  SSdb    *pSdb = pMnode->pSdb;
  int32_t  numOfRows = 0;
  SStbObj *pStb = NULL;
  int32_t  cols = 0;
  char    *pWrite;
  char     prefix[64] = {0};

  SDbObj *pDb = mndAcquireDb(pMnode, pShow->db);
  if (pDb == NULL) {
    return TSDB_CODE_MND_INVALID_DB;
  }

  tstrncpy(prefix, pShow->db, 64);
  strcat(prefix, TS_PATH_DELIMITER);
  int32_t prefixLen = (int32_t)strlen(prefix);

  while (numOfRows < rows) {
    pShow->pIter = sdbFetch(pSdb, SDB_STB, pShow->pIter, (void **)&pStb);
    if (pShow->pIter == NULL) break;

    if (pStb->dbUid != pDb->uid) {
      sdbRelease(pSdb, pStb);
      continue;
    }

    cols = 0;

    char stbName[TSDB_TABLE_NAME_LEN] = {0};
    tstrncpy(stbName, pStb->name + prefixLen, TSDB_TABLE_NAME_LEN);
    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    STR_TO_VARSTR(pWrite, stbName);
    cols++;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    *(int64_t *)pWrite = pStb->createdTime;
    cols++;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    *(int32_t *)pWrite = pStb->numOfColumns;
    cols++;

    pWrite = data + pShow->offset[cols] * rows + pShow->bytes[cols] * numOfRows;
    *(int32_t *)pWrite = pStb->numOfTags;
    cols++;

    numOfRows++;
    sdbRelease(pSdb, pStb);
  }

  mndReleaseDb(pMnode, pDb);
  pShow->numOfReads += numOfRows;
  mndVacuumResult(data, pShow->numOfColumns, numOfRows, rows, pShow);
  return numOfRows;
}

static void mndCancelGetNextStb(SMnode *pMnode, void *pIter) {
  SSdb *pSdb = pMnode->pSdb;
  sdbCancelFetch(pSdb, pIter);
}

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

#include "vnd.h"

void smaHandleRes(void *pVnode, int64_t smaId, const SArray *data) {
  // TODO

  blockDebugShowData(data);
  tsdbInsertTSmaData(((SVnode *)pVnode)->pTsdb, smaId, (const char *)data);
}

void vnodeProcessWMsgs(SVnode *pVnode, SArray *pMsgs) {
  SNodeMsg *pMsg;
  SRpcMsg  *pRpc;

  for (int i = 0; i < taosArrayGetSize(pMsgs); i++) {
    pMsg = *(SNodeMsg **)taosArrayGet(pMsgs, i);
    pRpc = &pMsg->rpcMsg;

    // set request version
    void   *pBuf = POINTER_SHIFT(pRpc->pCont, sizeof(SMsgHead));
    int64_t ver = pVnode->state.processed++;
    taosEncodeFixedI64(&pBuf, ver);

    if (walWrite(pVnode->pWal, ver, pRpc->msgType, pRpc->pCont, pRpc->contLen) < 0) {
      // TODO: handle error
      /*ASSERT(false);*/
      vError("vnode:%d  write wal error since %s", pVnode->vgId, terrstr());
    }
  }

  walFsync(pVnode->pWal, false);

  // TODO: Integrate RAFT module here

  // No results are returned because error handling is difficult
  // return 0;
}

int vnodeApplyWMsg(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp) {
  void *ptr = NULL;

  if (pVnode->config.streamMode == 0) {
    ptr = vnodeMalloc(pVnode, pMsg->contLen);
    if (ptr == NULL) {
      // TODO: handle error
    }

    // TODO: copy here need to be extended
    memcpy(ptr, pMsg->pCont, pMsg->contLen);
  }

  // todo: change the interface here
  int64_t ver;
  taosDecodeFixedI64(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &ver);
  if (tqPushMsg(pVnode->pTq, pMsg->pCont, pMsg->contLen, pMsg->msgType, ver) < 0) {
    // TODO: handle error
  }

  switch (pMsg->msgType) {
    case TDMT_VND_CREATE_STB: {
      SVCreateTbReq vCreateTbReq = {0};
      tDeserializeSVCreateTbReq(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &vCreateTbReq);
      if (metaCreateTable(pVnode->pMeta, &(vCreateTbReq)) < 0) {
        // TODO: handle error
      }

      // TODO: maybe need to clear the request struct
      taosMemoryFree(vCreateTbReq.stbCfg.pSchema);
      taosMemoryFree(vCreateTbReq.stbCfg.pTagSchema);
      taosMemoryFree(vCreateTbReq.stbCfg.pBSmaCols);
      taosMemoryFree(vCreateTbReq.stbCfg.pRSmaParam);
      taosMemoryFree(vCreateTbReq.dbFName);
      taosMemoryFree(vCreateTbReq.name);
      break;
    }
    case TDMT_VND_CREATE_TABLE: {
      SVCreateTbBatchReq vCreateTbBatchReq = {0};
      SVCreateTbBatchRsp vCreateTbBatchRsp = {0};
      tDeserializeSVCreateTbBatchReq(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &vCreateTbBatchReq);
      int reqNum = taosArrayGetSize(vCreateTbBatchReq.pArray);
      for (int i = 0; i < reqNum; i++) {
        SVCreateTbReq *pCreateTbReq = taosArrayGet(vCreateTbBatchReq.pArray, i);

        char      tableFName[TSDB_TABLE_FNAME_LEN];
        SMsgHead *pHead = (SMsgHead *)pMsg->pCont;
        sprintf(tableFName, "%s.%s", pCreateTbReq->dbFName, pCreateTbReq->name);

        int32_t code = vnodeValidateTableHash(&pVnode->config, tableFName);
        if (code) {
          SVCreateTbRsp rsp;
          rsp.code = code;

          taosArrayPush(vCreateTbBatchRsp.rspList, &rsp);
        }

        if (metaCreateTable(pVnode->pMeta, pCreateTbReq) < 0) {
          // TODO: handle error
          vError("vgId:%d, failed to create table: %s", pVnode->vgId, pCreateTbReq->name);
        }
        taosMemoryFree(pCreateTbReq->name);
        taosMemoryFree(pCreateTbReq->dbFName);
        if (pCreateTbReq->type == TD_SUPER_TABLE) {
          taosMemoryFree(pCreateTbReq->stbCfg.pSchema);
          taosMemoryFree(pCreateTbReq->stbCfg.pTagSchema);
          taosMemoryFree(pCreateTbReq->stbCfg.pBSmaCols);
          taosMemoryFree(pCreateTbReq->stbCfg.pRSmaParam);
        } else if (pCreateTbReq->type == TD_CHILD_TABLE) {
          taosMemoryFree(pCreateTbReq->ctbCfg.pTag);
        } else {
          taosMemoryFree(pCreateTbReq->ntbCfg.pSchema);
          taosMemoryFree(pCreateTbReq->ntbCfg.pBSmaCols);
          taosMemoryFree(pCreateTbReq->ntbCfg.pRSmaParam);
        }
      }

      vTrace("vgId:%d process create %" PRIzu " tables", pVnode->vgId, taosArrayGetSize(vCreateTbBatchReq.pArray));
      taosArrayDestroy(vCreateTbBatchReq.pArray);
      if (vCreateTbBatchRsp.rspList) {
        int32_t contLen = tSerializeSVCreateTbBatchRsp(NULL, 0, &vCreateTbBatchRsp);
        void   *msg = rpcMallocCont(contLen);
        tSerializeSVCreateTbBatchRsp(msg, contLen, &vCreateTbBatchRsp);
        taosArrayDestroy(vCreateTbBatchRsp.rspList);

        *pRsp = taosMemoryCalloc(1, sizeof(SRpcMsg));
        (*pRsp)->msgType = TDMT_VND_CREATE_TABLE_RSP;
        (*pRsp)->pCont = msg;
        (*pRsp)->contLen = contLen;
        (*pRsp)->handle = pMsg->handle;
        (*pRsp)->ahandle = pMsg->ahandle;
      }
      break;
    }
    case TDMT_VND_ALTER_STB: {
      SVCreateTbReq vAlterTbReq = {0};
      vTrace("vgId:%d, process alter stb req", pVnode->vgId);
      tDeserializeSVCreateTbReq(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &vAlterTbReq);
      taosMemoryFree(vAlterTbReq.stbCfg.pSchema);
      taosMemoryFree(vAlterTbReq.stbCfg.pTagSchema);
      taosMemoryFree(vAlterTbReq.stbCfg.pBSmaCols);
      taosMemoryFree(vAlterTbReq.stbCfg.pRSmaParam);
      taosMemoryFree(vAlterTbReq.dbFName);
      taosMemoryFree(vAlterTbReq.name);
      break;
    }
    case TDMT_VND_DROP_STB:
      vTrace("vgId:%d, process drop stb req", pVnode->vgId);
      break;
    case TDMT_VND_DROP_TABLE:
      // if (metaDropTable(pVnode->pMeta, vReq.dtReq.uid) < 0) {
      //   // TODO: handle error
      // }
      break;
    case TDMT_VND_SUBMIT:
      if (pVnode->config.streamMode == 0) {
        if (tsdbInsertData(pVnode->pTsdb, (SSubmitReq *)ptr, NULL) < 0) {
          // TODO: handle error
        }
      }
      break;
    case TDMT_VND_MQ_SET_CONN: {
      if (tqProcessSetConnReq(pVnode->pTq, POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead))) < 0) {
        // TODO: handle error
      }
    } break;
    case TDMT_VND_MQ_REB: {
      if (tqProcessRebReq(pVnode->pTq, POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead))) < 0) {
      }
    } break;
    case TDMT_VND_TASK_DEPLOY: {
      if (tqProcessTaskDeploy(pVnode->pTq, POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)),
                              pMsg->contLen - sizeof(SMsgHead)) < 0) {
      }
    } break;
    case TDMT_VND_TASK_WRITE_EXEC: {
      if (tqProcessTaskExec(pVnode->pTq, POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), pMsg->contLen - sizeof(SMsgHead),
                            0) < 0) {
      }
    } break;
    case TDMT_VND_CREATE_SMA: {  // timeRangeSMA
#if 1

      SSmaCfg vCreateSmaReq = {0};
      if (tDeserializeSVCreateTSmaReq(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &vCreateSmaReq) == NULL) {
        terrno = TSDB_CODE_OUT_OF_MEMORY;
        vWarn("vgId%d: TDMT_VND_CREATE_SMA received but deserialize failed since %s", pVnode->config.vgId,
              terrstr(terrno));
        return -1;
      }
      vWarn("vgId%d: TDMT_VND_CREATE_SMA received for %s:%" PRIi64, pVnode->config.vgId, vCreateSmaReq.tSma.indexName,
            vCreateSmaReq.tSma.indexUid);

      // record current timezone of server side
      vCreateSmaReq.tSma.timezoneInt = tsTimezone;

      if (metaCreateTSma(pVnode->pMeta, &vCreateSmaReq) < 0) {
        // TODO: handle error
        tdDestroyTSma(&vCreateSmaReq.tSma);
        return -1;
      }
      // TODO: send msg to stream computing to create tSma
      // if ((send msg to stream computing) < 0) {
      //   tdDestroyTSma(&vCreateSmaReq);
      //   return -1;
      // }
      tdDestroyTSma(&vCreateSmaReq.tSma);
      // TODO: return directly or go on follow steps?
#endif
    } break;
    case TDMT_VND_CANCEL_SMA: {  // timeRangeSMA
    } break;
    case TDMT_VND_DROP_SMA: {  // timeRangeSMA
#if 0    
      SVDropTSmaReq vDropSmaReq = {0};
      if (tDeserializeSVDropTSmaReq(POINTER_SHIFT(pMsg->pCont, sizeof(SMsgHead)), &vDropSmaReq) == NULL) {
        terrno = TSDB_CODE_OUT_OF_MEMORY;
        return -1;
      }

      // TODO: send msg to stream computing to drop tSma
      // if ((send msg to stream computing) < 0) {
      //   tdDestroyTSma(&vCreateSmaReq);
      //   return -1;
      // }
      // 

      if (metaDropTSma(pVnode->pMeta, vDropSmaReq.indexUid) < 0) {
        // TODO: handle error
        return -1;
      }

      if(tsdbDropTSmaData(pVnode->pTsdb, vDropSmaReq.indexUid) < 0) {
        // TODO: handle error
        return -1;
      }

      // TODO: return directly or go on follow steps?
#endif
    } break;
    default:
      ASSERT(0);
      break;
  }

  pVnode->state.applied = ver;

  // Check if it needs to commit
  if (vnodeShouldCommit(pVnode)) {
    // tsem_wait(&(pVnode->canCommit));
    if (vnodeAsyncCommit(pVnode) < 0) {
      // TODO: handle error
    }
  }

  return 0;
}

/* ------------------------ STATIC METHODS ------------------------ */

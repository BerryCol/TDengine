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

#ifndef _TD_VNODE_H_
#define _TD_VNODE_H_

#include "os.h"
#include "trpc.h"

#include "meta.h"
#include "tarray.h"
#include "tq.h"
#include "tsdb.h"
#include "wal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------ TYPES EXPOSED ------------------------ */
typedef struct SVnode SVnode;
typedef struct SVnodeCfg {
  /** vnode buffer pool options */
  struct {
    /** write buffer size */
    uint64_t wsize;
    uint64_t ssize;
    uint64_t lsize;
    /** use heap allocator or arena allocator */
    bool isHeapAllocator;
  };

  /** time to live of tables in this vnode */
  uint32_t ttl;

  /** data to keep in this vnode */
  uint32_t keep;

  /** if TS data is eventually consistency */
  bool isWeak;

  /** TSDB config */
  STsdbCfg tsdbCfg;

  /** META config */
  SMetaCfg metaCfg;

  /** TQ config */
  STqCfg tqCfg;

  /** WAL config */
  SWalCfg walCfg;
} SVnodeCfg;

/* ------------------------ SVnode ------------------------ */
/**
 * @brief Initialize the vnode module
 *
 * @param nthreads number of commit threads. 0 for no threads and
 *        a schedule queue should be given (TODO)
 * @return int 0 for success and -1 for failure
 */
int vnodeInit(uint16_t nthreads);

/**
 * @brief clear a vnode
 *
 */
void vnodeClear();

/**
 * @brief Open a VNODE.
 *
 * @param path path of the vnode
 * @param pVnodeCfg options of the vnode
 * @return SVnode* The vnode object
 */
SVnode *vnodeOpen(const char *path, const SVnodeCfg *pVnodeCfg);

/**
 * @brief Close a VNODE
 *
 * @param pVnode The vnode object to close
 */
void vnodeClose(SVnode *pVnode);

/**
 * @brief Destroy a VNODE.
 *
 * @param path Path of the VNODE.
 */
void vnodeDestroy(const char *path);

/**
 * @brief Process an array of write messages.
 *
 * @param pVnode The vnode object.
 * @param pMsgs The array of SRpcMsg
 * @return int 0 for success, -1 for failure
 */
int vnodeProcessWMsgs(SVnode *pVnode, SArray *pMsgs);

/**
 * @brief Apply a write request message.
 *
 * @param pVnode The vnode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int 0 for success, -1 for failure
 */
int vnodeApplyWMsg(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/**
 * @brief Process a consume message.
 *
 * @param pVnode The vnode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int 0 for success, -1 for failure
 */
int vnodeProcessCMsg(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/**
 * @brief Process the sync request
 *
 * @param pVnode
 * @param pMsg
 * @param pRsp
 * @return int
 */
int vnodeProcessSyncReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/**
 * @brief Process a query message.
 *
 * @param pVnode The vnode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int 0 for success, -1 for failure
 */
int vnodeProcessQueryReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/**
 * @brief Process a fetch message.
 *
 * @param pVnode The vnode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int 0 for success, -1 for failure
 */
int vnodeProcessFetchReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/**
 * @brief Process a consume message.
 *
 * @param pVnode The vnode object.
 * @param pMsg The request message
 * @param pRsp The response message
 * @return int 0 for success, -1 for failure
 */
int vnodeProcessConsumeReq(SVnode *pVnode, SRpcMsg *pMsg, SRpcMsg **pRsp);

/* ------------------------ SVnodeCfg ------------------------ */
/**
 * @brief Initialize VNODE options.
 *
 * @param pOptions The options object to be initialized. It should not be NULL.
 */
void vnodeOptionsInit(SVnodeCfg *pOptions);

/**
 * @brief Clear VNODE options.
 *
 * @param pOptions Options to clear.
 */
void vnodeOptionsClear(SVnodeCfg *pOptions);

/* ------------------------ REQUESTS ------------------------ */
typedef STbCfg SVCreateTableReq;
typedef struct {
  tb_uid_t uid;
} SVDropTableReq;

typedef struct {
  // TODO
} SVSubmitReq;

typedef struct {
  uint64_t ver;
  union {
    SVCreateTableReq ctReq;
    SVDropTableReq   dtReq;
  };
} SVnodeReq;

typedef struct {
  int  err;
  char info[];
} SVnodeRsp;

static FORCE_INLINE void vnodeSetCreateStbReq(SVnodeReq *pReq, char *name, uint32_t ttl, uint32_t keep, tb_uid_t suid,
                                              STSchema *pSchema, STSchema *pTagSchema) {
  pReq->ver = 0;

  pReq->ctReq.name = name;
  pReq->ctReq.ttl = ttl;
  pReq->ctReq.keep = keep;
  pReq->ctReq.type = META_SUPER_TABLE;
  pReq->ctReq.stbCfg.suid = suid;
  pReq->ctReq.stbCfg.pSchema = pSchema;
  pReq->ctReq.stbCfg.pTagSchema = pTagSchema;
}

static FORCE_INLINE void vnodeSetCreateCtbReq(SVnodeReq *pReq, char *name, uint32_t ttl, uint32_t keep, tb_uid_t suid,
                                              SKVRow pTag) {
  pReq->ver = 0;

  pReq->ctReq.name = name;
  pReq->ctReq.ttl = ttl;
  pReq->ctReq.keep = keep;
  pReq->ctReq.type = META_CHILD_TABLE;
  pReq->ctReq.ctbCfg.suid = suid;
  pReq->ctReq.ctbCfg.pTag = pTag;
}

static FORCE_INLINE void vnodeSetCreateNtbReq(SVnodeReq *pReq, char *name, uint32_t ttl, uint32_t keep,
                                              STSchema *pSchema) {
  pReq->ver = 0;

  pReq->ctReq.name = name;
  pReq->ctReq.ttl = ttl;
  pReq->ctReq.keep = keep;
  pReq->ctReq.type = META_NORMAL_TABLE;
  pReq->ctReq.ntbCfg.pSchema = pSchema;
}

int   vnodeBuildReq(void **buf, const SVnodeReq *pReq, tmsg_t type);
void *vnodeParseReq(void *buf, SVnodeReq *pReq, tmsg_t type);

/* ------------------------ FOR COMPILE ------------------------ */

int32_t vnodeAlter(SVnode *pVnode, const SVnodeCfg *pCfg);
int32_t vnodeCompact(SVnode *pVnode);
int32_t vnodeSync(SVnode *pVnode);
int32_t vnodeGetLoad(SVnode *pVnode, SVnodeLoad *pLoad);

#ifdef __cplusplus
}
#endif

#endif /*_TD_VNODE_H_*/

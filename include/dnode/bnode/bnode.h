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

#ifndef _TD_BNODE_H_
#define _TD_BNODE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------ TYPES EXPOSED ------------------------ */
typedef struct SDnode SDnode;
typedef struct SBnode SBnode;
typedef void (*SendMsgToDnodeFp)(SDnode *pDnode, struct SEpSet *epSet, struct SRpcMsg *rpcMsg);
typedef void (*SendMsgToMnodeFp)(SDnode *pDnode, struct SRpcMsg *rpcMsg);
typedef void (*SendRedirectMsgFp)(SDnode *pDnode, struct SRpcMsg *rpcMsg);

typedef struct {
  int64_t numOfErrors;
} SBnodeLoad;

typedef struct {
  int32_t sver;
} SBnodeCfg;

typedef struct {
  int32_t           dnodeId;
  int64_t           clusterId;
  SBnodeCfg         cfg;
  SDnode           *pDnode;
  SendMsgToDnodeFp  sendMsgToDnodeFp;
  SendMsgToMnodeFp  sendMsgToMnodeFp;
  SendRedirectMsgFp sendRedirectMsgFp;
} SBnodeOpt;

/* ------------------------ SBnode ------------------------ */
/**
 * @brief Start one Bnode in Dnode.
 *
 * @param path Path of the bnode.
 * @param pOption Option of the bnode.
 * @return SBnode* The bnode object.
 */
SBnode *bndOpen(const char *path, const SBnodeOpt *pOption);

/**
 * @brief Stop Bnode in Dnode.
 *
 * @param pBnode The bnode object to close.
 */
void bndClose(SBnode *pBnode);

/**
 * @brief Get the statistical information of Bnode
 *
 * @param pBnode The bnode object.
 * @param pLoad Statistics of the bnode.
 * @return int32_t 0 for success, -1 for failure.
 */
int32_t bndGetLoad(SBnode *pBnode, SBnodeLoad *pLoad);

/**
 * @brief Process a query message.
 *
 * @param pBnode The bnode object.
 * @param pMsgs The array of SRpcMsg
 * @return int32_t 0 for success, -1 for failure
 */
int32_t bndProcessWMsgs(SBnode *pBnode, SArray *pMsgs);

/**
 * @brief Drop a bnode.
 *
 * @param path Path of the bnode.
 */
void bndDestroy(const char *path);

#ifdef __cplusplus
}
#endif

#endif /*_TD_BNODE_H_*/
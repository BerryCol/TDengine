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

/* this file is mainly responsible for the communication between DNODEs. Each
 * dnode works as both server and client. Dnode may send status, grant, config
 * messages to mnode, mnode may send create/alter/drop table/vnode messages
 * to dnode. All theses messages are handled from here
 */

#define _DEFAULT_SOURCE
#include "dndTransport.h"
#include "dndDnode.h"
#include "dndMnode.h"
#include "dndVnodes.h"

#define INTERNAL_USER "_internal"
#define INTERNAL_CKEY "_key"
#define INTERNAL_SECRET "_secret"

static void dndInitMsgFp(STransMgmt *pMgmt) {
  // Requests handled by DNODE
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CREATE_MNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CREATE_MNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_ALTER_MNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_ALTER_MNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_DROP_MNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_DROP_MNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CREATE_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CREATE_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_ALTER_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_ALTER_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_DROP_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_DROP_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_SYNC_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_SYNC_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_AUTH_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_AUTH_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_COMPACT_VNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_COMPACT_VNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CONFIG_DNODE)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_CONFIG_DNODE_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_DND_NETWORK_TEST)] = dndProcessMgmtMsg;

  // Requests handled by MNODE
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CONNECT)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_ACCT)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_ALTER_ACCT)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_ACCT)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_USER)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_ALTER_USER)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_USER)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_DNODE)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CONFIG_DNODE)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_DNODE)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_MNODE)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_MNODE)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_USE_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_ALTER_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_SYNC_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_COMPACT_DB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_FUNCTION)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_RETRIEVE_FUNCTION)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_FUNCTION)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_STB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_ALTER_STB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_STB)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_STB_META)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_VGROUP_LIST)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_KILL_QUERY)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_KILL_CONN)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_HEARTBEAT)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_SHOW)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_SHOW_RETRIEVE)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_STATUS)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_STATUS_RSP)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_TRANS)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_TRANS_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_GRANT)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_GRANT_RSP)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_AUTH)] = dndProcessMnodeReadMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_AUTH_RSP)] = dndProcessMgmtMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_CREATE_TOPIC)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_ALTER_TOPIC)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_MND_DROP_TOPIC)] = dndProcessMnodeWriteMsg;

  // Requests handled by VNODE
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_SUBMIT)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_QUERY)] = dndProcessVnodeQueryMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_FETCH)] = dndProcessVnodeFetchMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_ALTER_TABLE)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_UPDATE_TAG_VAL)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_TABLE_META)] = dndProcessVnodeQueryMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_TABLES_META)] = dndProcessVnodeQueryMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_MQ_CONSUME)] = dndProcessVnodeQueryMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_MQ_QUERY)] = dndProcessVnodeQueryMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_MQ_CONNECT)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_MQ_DISCONNECT)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_MQ_SET_CUR)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_RES_READY)] = dndProcessVnodeFetchMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_TASKS_STATUS)] = dndProcessVnodeFetchMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_CANCEL_TASK)] = dndProcessVnodeFetchMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_DROP_TASK)] = dndProcessVnodeFetchMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_CREATE_STB)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_CREATE_STB_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_ALTER_STB)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_ALTER_STB_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_DROP_STB)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_DROP_STB_RSP)] = dndProcessMnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_CREATE_TABLE)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_ALTER_TABLE)] = dndProcessVnodeWriteMsg;
  pMgmt->msgFp[TMSG_INDEX(TDMT_VND_DROP_TABLE)] = dndProcessVnodeWriteMsg;
}

static void dndProcessResponse(void *parent, SRpcMsg *pMsg, SEpSet *pEpSet) {
  SDnode     *pDnode = parent;
  STransMgmt *pMgmt = &pDnode->tmgmt;

  tmsg_t msgType = pMsg->msgType;

  if (dndGetStat(pDnode) == DND_STAT_STOPPED) {
    if (pMsg == NULL || pMsg->pCont == NULL) return;
    dTrace("RPC %p, rsp:%s is ignored since dnode is stopping", pMsg->handle, TMSG_INFO(msgType));
    rpcFreeCont(pMsg->pCont);
    return;
  }

  DndMsgFp fp = pMgmt->msgFp[TMSG_INDEX(msgType)];
  if (fp != NULL) {
    (*fp)(pDnode, pMsg, pEpSet);
    dTrace("RPC %p, rsp:%s is processed, code:0x%x", pMsg->handle, TMSG_INFO(msgType), pMsg->code & 0XFFFF);
  } else {
    dError("RPC %p, rsp:%s not processed", pMsg->handle, TMSG_INFO(msgType));
    rpcFreeCont(pMsg->pCont);
  }
}

static int32_t dndInitClient(SDnode *pDnode) {
  STransMgmt *pMgmt = &pDnode->tmgmt;

  SRpcInit rpcInit;
  memset(&rpcInit, 0, sizeof(rpcInit));
  rpcInit.label = "DND-C";
  rpcInit.numOfThreads = 1;
  rpcInit.cfp = dndProcessResponse;
  rpcInit.sessions = 1024;
  rpcInit.connType = TAOS_CONN_CLIENT;
  rpcInit.idleTime = pDnode->opt.shellActivityTimer * 1000;
  rpcInit.user = INTERNAL_USER;
  rpcInit.ckey = INTERNAL_CKEY;
  rpcInit.secret = INTERNAL_SECRET;
  rpcInit.parent = pDnode;

  pMgmt->clientRpc = rpcOpen(&rpcInit);
  if (pMgmt->clientRpc == NULL) {
    dError("failed to init rpc client");
    return -1;
  }

  dDebug("dnode rpc client is initialized");
  return 0;
}

static void dndCleanupClient(SDnode *pDnode) {
  STransMgmt *pMgmt = &pDnode->tmgmt;
  if (pMgmt->clientRpc) {
    rpcClose(pMgmt->clientRpc);
    pMgmt->clientRpc = NULL;
    dDebug("dnode rpc client is closed");
  }
}

static void dndProcessRequest(void *param, SRpcMsg *pMsg, SEpSet *pEpSet) {
  SDnode     *pDnode = param;
  STransMgmt *pMgmt = &pDnode->tmgmt;

  tmsg_t msgType = pMsg->msgType;
  if (msgType == TDMT_DND_NETWORK_TEST) {
    dTrace("RPC %p, network test req, app:%p will be processed, code:0x%x", pMsg->handle, pMsg->ahandle, pMsg->code);
    dndProcessStartupReq(pDnode, pMsg);
    return;
  }

  if (dndGetStat(pDnode) == DND_STAT_STOPPED) {
    dError("RPC %p, req:%s app:%p is ignored since dnode exiting", pMsg->handle, TMSG_INFO(msgType), pMsg->ahandle);
    SRpcMsg rspMsg = {.handle = pMsg->handle, .code = TSDB_CODE_DND_EXITING};
    rpcSendResponse(&rspMsg);
    rpcFreeCont(pMsg->pCont);
    return;
  } else if (dndGetStat(pDnode) != DND_STAT_RUNNING) {
    dError("RPC %p, req:%s app:%p is ignored since dnode not running", pMsg->handle, TMSG_INFO(msgType), pMsg->ahandle);
    SRpcMsg rspMsg = {.handle = pMsg->handle, .code = TSDB_CODE_APP_NOT_READY};
    rpcSendResponse(&rspMsg);
    rpcFreeCont(pMsg->pCont);
    return;
  }

  if (pMsg->pCont == NULL) {
    dTrace("RPC %p, req:%s app:%p not processed since content is null", pMsg->handle, TMSG_INFO(msgType),
           pMsg->ahandle);
    SRpcMsg rspMsg = {.handle = pMsg->handle, .code = TSDB_CODE_DND_INVALID_MSG_LEN};
    rpcSendResponse(&rspMsg);
    return;
  }

  DndMsgFp fp = pMgmt->msgFp[TMSG_INDEX(msgType)];
  if (fp != NULL) {
    dTrace("RPC %p, req:%s app:%p will be processed", pMsg->handle, TMSG_INFO(msgType), pMsg->ahandle);
    (*fp)(pDnode, pMsg, pEpSet);
  } else {
    dError("RPC %p, req:%s app:%p is not processed since no handle", pMsg->handle, TMSG_INFO(msgType), pMsg->ahandle);
    SRpcMsg rspMsg = {.handle = pMsg->handle, .code = TSDB_CODE_MSG_NOT_PROCESSED};
    rpcSendResponse(&rspMsg);
    rpcFreeCont(pMsg->pCont);
  }
}

static void dndSendMsgToMnodeRecv(SDnode *pDnode, SRpcMsg *pRpcMsg, SRpcMsg *pRpcRsp) {
  STransMgmt *pMgmt = &pDnode->tmgmt;

  SEpSet epSet = {0};
  dndGetMnodeEpSet(pDnode, &epSet);
  rpcSendRecv(pMgmt->clientRpc, &epSet, pRpcMsg, pRpcRsp);
}

static int32_t dndAuthInternalMsg(SDnode *pDnode, char *user, char *spi, char *encrypt, char *secret, char *ckey) {
  if (strcmp(user, INTERNAL_USER) == 0) {
    // A simple temporary implementation
    char pass[TSDB_PASSWORD_LEN] = {0};
    taosEncryptPass((uint8_t *)(INTERNAL_SECRET), strlen(INTERNAL_SECRET), pass);
    memcpy(secret, pass, TSDB_PASSWORD_LEN);
    *spi = 0;
    *encrypt = 0;
    *ckey = 0;
    return 0;
  } else if (strcmp(user, TSDB_NETTEST_USER) == 0) {
    // A simple temporary implementation
    char pass[TSDB_PASSWORD_LEN] = {0};
    taosEncryptPass((uint8_t *)(TSDB_NETTEST_USER), strlen(TSDB_NETTEST_USER), pass);
    memcpy(secret, pass, TSDB_PASSWORD_LEN);
    *spi = 0;
    *encrypt = 0;
    *ckey = 0;
    return 0;
  } else {
    return -1;
  }
}

static int32_t dndRetrieveUserAuthInfo(void *parent, char *user, char *spi, char *encrypt, char *secret, char *ckey) {
  SDnode *pDnode = parent;

  if (dndAuthInternalMsg(parent, user, spi, encrypt, secret, ckey) == 0) {
    // dTrace("get internal auth success");
    return 0;
  }

  if (dndGetUserAuthFromMnode(pDnode, user, spi, encrypt, secret, ckey) == 0) {
    // dTrace("get auth from internal mnode");
    return 0;
  }

  if (terrno != TSDB_CODE_APP_NOT_READY) {
    dTrace("failed to get user auth from internal mnode since %s", terrstr());
    return -1;
  }

  // dDebug("user:%s, send auth msg to other mnodes", user);

  SAuthMsg *pMsg = rpcMallocCont(sizeof(SAuthMsg));
  tstrncpy(pMsg->user, user, TSDB_USER_LEN);

  SRpcMsg rpcMsg = {.pCont = pMsg, .contLen = sizeof(SAuthMsg), .msgType = TDMT_MND_AUTH};
  SRpcMsg rpcRsp = {0};
  dndSendMsgToMnodeRecv(pDnode, &rpcMsg, &rpcRsp);

  if (rpcRsp.code != 0) {
    terrno = rpcRsp.code;
    dError("user:%s, failed to get user auth from other mnodes since %s", user, terrstr());
  } else {
    SAuthRsp *pRsp = rpcRsp.pCont;
    memcpy(secret, pRsp->secret, TSDB_PASSWORD_LEN);
    memcpy(ckey, pRsp->ckey, TSDB_PASSWORD_LEN);
    *spi = pRsp->spi;
    *encrypt = pRsp->encrypt;
    dDebug("user:%s, success to get user auth from other mnodes", user);
  }

  rpcFreeCont(rpcRsp.pCont);
  return rpcRsp.code;
}

static int32_t dndInitServer(SDnode *pDnode) {
  STransMgmt *pMgmt = &pDnode->tmgmt;
  dndInitMsgFp(pMgmt);

  int32_t numOfThreads = (int32_t)((pDnode->opt.numOfCores * pDnode->opt.numOfThreadsPerCore) / 2.0);
  if (numOfThreads < 1) {
    numOfThreads = 1;
  }

  SRpcInit rpcInit;
  memset(&rpcInit, 0, sizeof(rpcInit));
  rpcInit.localPort = pDnode->opt.serverPort;
  rpcInit.label = "DND-S";
  rpcInit.numOfThreads = numOfThreads;
  rpcInit.cfp = dndProcessRequest;
  rpcInit.sessions = pDnode->opt.maxShellConns;
  rpcInit.connType = TAOS_CONN_SERVER;
  rpcInit.idleTime = pDnode->opt.shellActivityTimer * 1000;
  rpcInit.afp = dndRetrieveUserAuthInfo;
  rpcInit.parent = pDnode;

  pMgmt->serverRpc = rpcOpen(&rpcInit);
  if (pMgmt->serverRpc == NULL) {
    dError("failed to init rpc server");
    return -1;
  }

  dDebug("dnode rpc server is initialized");
  return 0;
}

static void dndCleanupServer(SDnode *pDnode) {
  STransMgmt *pMgmt = &pDnode->tmgmt;
  if (pMgmt->serverRpc) {
    rpcClose(pMgmt->serverRpc);
    pMgmt->serverRpc = NULL;
    dDebug("dnode rpc server is closed");
  }
}

int32_t dndInitTrans(SDnode *pDnode) {
  if (dndInitClient(pDnode) != 0) {
    return -1;
  }

  if (dndInitServer(pDnode) != 0) {
    return -1;
  }

  dInfo("dnode-transport is initialized");
  return 0;
}

void dndCleanupTrans(SDnode *pDnode) {
  dInfo("dnode-transport start to clean up");
  dndCleanupServer(pDnode);
  dndCleanupClient(pDnode);
  dInfo("dnode-transport is cleaned up");
}

void dndSendMsgToDnode(SDnode *pDnode, SEpSet *pEpSet, SRpcMsg *pMsg) {
  STransMgmt *pMgmt = &pDnode->tmgmt;
  rpcSendRequest(pMgmt->clientRpc, pEpSet, pMsg, NULL);
}

void dndSendMsgToMnode(SDnode *pDnode, SRpcMsg *pMsg) {
  SEpSet epSet = {0};
  dndGetMnodeEpSet(pDnode, &epSet);
  dndSendMsgToDnode(pDnode, &epSet, pMsg);
}

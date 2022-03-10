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

#include "syncMessage.h"
#include "syncUtil.h"
#include "tcoding.h"

// ---------------------------------------------
cJSON* syncRpcMsg2Json(SRpcMsg* pRpcMsg) {
  cJSON* pRoot;

  // in compiler optimization, switch case = if else constants
  if (pRpcMsg->msgType == SYNC_TIMEOUT) {
    SyncTimeout* pSyncMsg = (SyncTimeout*)pRpcMsg->pCont;
    pRoot = syncTimeout2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_PING) {
    SyncPing* pSyncMsg = (SyncPing*)pRpcMsg->pCont;
    pRoot = syncPing2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_PING_REPLY) {
    SyncPingReply* pSyncMsg = (SyncPingReply*)pRpcMsg->pCont;
    pRoot = syncPingReply2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_CLIENT_REQUEST) {
    SyncClientRequest* pSyncMsg = (SyncClientRequest*)pRpcMsg->pCont;
    pRoot = syncClientRequest2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_CLIENT_REQUEST_REPLY) {
    pRoot = syncRpcUnknownMsg2Json();

  } else if (pRpcMsg->msgType == SYNC_REQUEST_VOTE) {
    SyncRequestVote* pSyncMsg = (SyncRequestVote*)pRpcMsg->pCont;
    pRoot = syncRequestVote2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_REQUEST_VOTE_REPLY) {
    SyncRequestVoteReply* pSyncMsg = (SyncRequestVoteReply*)pRpcMsg->pCont;
    pRoot = syncRequestVoteReply2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_APPEND_ENTRIES) {
    SyncAppendEntries* pSyncMsg = (SyncAppendEntries*)pRpcMsg->pCont;
    pRoot = syncAppendEntries2Json(pSyncMsg);

  } else if (pRpcMsg->msgType == SYNC_APPEND_ENTRIES_REPLY) {
    SyncAppendEntriesReply* pSyncMsg = (SyncAppendEntriesReply*)pRpcMsg->pCont;
    pRoot = syncAppendEntriesReply2Json(pSyncMsg);

  } else {
    pRoot = syncRpcUnknownMsg2Json();
  }

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "RpcMsg", pRoot);
  return pJson;
}

cJSON* syncRpcUnknownMsg2Json() {
  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "msgType", SYNC_UNKNOWN);
  cJSON_AddStringToObject(pRoot, "data", "known message");

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncPing", pRoot);
  return pJson;
}

char* syncRpcMsg2Str(SRpcMsg* pRpcMsg) {
  cJSON* pJson = syncRpcMsg2Json(pRpcMsg);
  char*  serialized = cJSON_Print(pJson);
  cJSON_Delete(pJson);
  return serialized;
}

// ---- message process SyncTimeout----
SyncTimeout* syncTimeoutBuild() {
  uint32_t     bytes = sizeof(SyncTimeout);
  SyncTimeout* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_TIMEOUT;
  return pMsg;
}

void syncTimeoutDestroy(SyncTimeout* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncTimeoutSerialize(const SyncTimeout* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncTimeoutDeserialize(const char* buf, uint32_t len, SyncTimeout* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
}

void syncTimeout2RpcMsg(const SyncTimeout* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncTimeoutSerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncTimeoutFromRpcMsg(const SRpcMsg* pRpcMsg, SyncTimeout* pMsg) {
  syncTimeoutDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncTimeout2Json(const SyncTimeout* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);
  cJSON_AddNumberToObject(pRoot, "timeoutType", pMsg->timeoutType);
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->logicClock);
  cJSON_AddStringToObject(pRoot, "logicClock", u64buf);
  cJSON_AddNumberToObject(pRoot, "timerMS", pMsg->timerMS);
  snprintf(u64buf, sizeof(u64buf), "%p", pMsg->data);
  cJSON_AddStringToObject(pRoot, "data", u64buf);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncTimeout", pRoot);
  return pJson;
}

SyncTimeout* syncTimeoutBuild2(ESyncTimeoutType timeoutType, uint64_t logicClock, int32_t timerMS, void* data) {
  SyncTimeout* pMsg = syncTimeoutBuild();
  pMsg->timeoutType = timeoutType;
  pMsg->logicClock = logicClock;
  pMsg->timerMS = timerMS;
  pMsg->data = data;
  return pMsg;
}

// ---- message process SyncPing----
SyncPing* syncPingBuild(uint32_t dataLen) {
  uint32_t  bytes = SYNC_PING_FIX_LEN + dataLen;
  SyncPing* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_PING;
  pMsg->dataLen = dataLen;
  return pMsg;
}

void syncPingDestroy(SyncPing* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncPingSerialize(const SyncPing* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncPingDeserialize(const char* buf, uint32_t len, SyncPing* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
  assert(pMsg->bytes == SYNC_PING_FIX_LEN + pMsg->dataLen);
}

void syncPing2RpcMsg(const SyncPing* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncPingSerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncPingFromRpcMsg(const SRpcMsg* pRpcMsg, SyncPing* pMsg) {
  syncPingDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncPing2Json(const SyncPing* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->destId.addr);
  cJSON_AddStringToObject(pDestId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  cJSON_AddNumberToObject(pRoot, "dataLen", pMsg->dataLen);
  cJSON_AddStringToObject(pRoot, "data", pMsg->data);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncPing", pRoot);
  return pJson;
}

SyncPing* syncPingBuild2(const SRaftId* srcId, const SRaftId* destId, const char* str) {
  uint32_t  dataLen = strlen(str) + 1;
  SyncPing* pMsg = syncPingBuild(dataLen);
  pMsg->srcId = *srcId;
  pMsg->destId = *destId;
  snprintf(pMsg->data, pMsg->dataLen, "%s", str);
  return pMsg;
}

SyncPing* syncPingBuild3(const SRaftId* srcId, const SRaftId* destId) {
  SyncPing* pMsg = syncPingBuild2(srcId, destId, "ping");
  return pMsg;
}

// ---- message process SyncPingReply----
SyncPingReply* syncPingReplyBuild(uint32_t dataLen) {
  uint32_t       bytes = SYNC_PING_REPLY_FIX_LEN + dataLen;
  SyncPingReply* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_PING_REPLY;
  pMsg->dataLen = dataLen;
  return pMsg;
}

void syncPingReplyDestroy(SyncPingReply* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncPingReplySerialize(const SyncPingReply* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncPingReplyDeserialize(const char* buf, uint32_t len, SyncPingReply* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
  assert(pMsg->bytes == SYNC_PING_FIX_LEN + pMsg->dataLen);
}

void syncPingReply2RpcMsg(const SyncPingReply* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncPingReplySerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncPingReplyFromRpcMsg(const SRpcMsg* pRpcMsg, SyncPingReply* pMsg) {
  syncPingReplyDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncPingReply2Json(const SyncPingReply* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->destId.addr);
  cJSON_AddStringToObject(pDestId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  cJSON_AddNumberToObject(pRoot, "dataLen", pMsg->dataLen);
  cJSON_AddStringToObject(pRoot, "data", pMsg->data);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncPingReply", pRoot);
  return pJson;
}

SyncPingReply* syncPingReplyBuild2(const SRaftId* srcId, const SRaftId* destId, const char* str) {
  uint32_t       dataLen = strlen(str) + 1;
  SyncPingReply* pMsg = syncPingReplyBuild(dataLen);
  pMsg->srcId = *srcId;
  pMsg->destId = *destId;
  snprintf(pMsg->data, pMsg->dataLen, "%s", str);
  return pMsg;
}

SyncPingReply* syncPingReplyBuild3(const SRaftId* srcId, const SRaftId* destId) {
  SyncPingReply* pMsg = syncPingReplyBuild2(srcId, destId, "pang");
  return pMsg;
}

// ---- message process SyncClientRequest----
SyncClientRequest* syncClientRequestBuild(uint32_t dataLen) {
  uint32_t           bytes = sizeof(SyncClientRequest) + dataLen;
  SyncClientRequest* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_CLIENT_REQUEST;
  pMsg->seqNum = 0;
  pMsg->isWeak = false;
  pMsg->dataLen = dataLen;
  return pMsg;
}

void syncClientRequestDestroy(SyncClientRequest* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncClientRequestSerialize(const SyncClientRequest* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncClientRequestDeserialize(const char* buf, uint32_t len, SyncClientRequest* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
}

void syncClientRequest2RpcMsg(const SyncClientRequest* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncClientRequestSerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncClientRequestFromRpcMsg(const SRpcMsg* pRpcMsg, SyncClientRequest* pMsg) {
  syncClientRequestDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncClientRequest2Json(const SyncClientRequest* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);
  cJSON_AddNumberToObject(pRoot, "originalRpcType", pMsg->originalRpcType);
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->seqNum);
  cJSON_AddStringToObject(pRoot, "seqNum", u64buf);
  cJSON_AddNumberToObject(pRoot, "isWeak", pMsg->isWeak);
  cJSON_AddNumberToObject(pRoot, "dataLen", pMsg->dataLen);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncClientRequest", pRoot);
  return pJson;
}

SyncClientRequest* syncClientRequestBuild2(const SRpcMsg* pOriginalRpcMsg, uint64_t seqNum, bool isWeak) {
  SyncClientRequest* pMsg = syncClientRequestBuild(pOriginalRpcMsg->contLen);
  pMsg->originalRpcType = pOriginalRpcMsg->msgType;
  pMsg->seqNum = seqNum;
  pMsg->isWeak = isWeak;
  memcpy(pMsg->data, pOriginalRpcMsg->pCont, pOriginalRpcMsg->contLen);
  return pMsg;
}

// ---- message process SyncRequestVote----
SyncRequestVote* syncRequestVoteBuild() {
  uint32_t         bytes = sizeof(SyncRequestVote);
  SyncRequestVote* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_REQUEST_VOTE;
  return pMsg;
}

void syncRequestVoteDestroy(SyncRequestVote* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncRequestVoteSerialize(const SyncRequestVote* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncRequestVoteDeserialize(const char* buf, uint32_t len, SyncRequestVote* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
}

void syncRequestVote2RpcMsg(const SyncRequestVote* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncRequestVoteSerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncRequestVoteFromRpcMsg(const SRpcMsg* pRpcMsg, SyncRequestVote* pMsg) {
  syncRequestVoteDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncRequestVote2Json(const SyncRequestVote* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  cJSON_AddNumberToObject(pDestId, "addr", pMsg->destId.addr);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->currentTerm);
  cJSON_AddStringToObject(pRoot, "currentTerm", u64buf);
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->lastLogIndex);
  cJSON_AddStringToObject(pRoot, "lastLogIndex", u64buf);
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->lastLogTerm);
  cJSON_AddStringToObject(pRoot, "lastLogTerm", u64buf);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncRequestVote", pRoot);
  return pJson;
}

// ---- message process SyncRequestVoteReply----
SyncRequestVoteReply* SyncRequestVoteReplyBuild() {
  uint32_t              bytes = sizeof(SyncRequestVoteReply);
  SyncRequestVoteReply* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_REQUEST_VOTE_REPLY;
  return pMsg;
}

void syncRequestVoteReplyDestroy(SyncRequestVoteReply* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncRequestVoteReplySerialize(const SyncRequestVoteReply* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncRequestVoteReplyDeserialize(const char* buf, uint32_t len, SyncRequestVoteReply* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
}

void syncRequestVoteReply2RpcMsg(const SyncRequestVoteReply* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncRequestVoteReplySerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncRequestVoteReplyFromRpcMsg(const SRpcMsg* pRpcMsg, SyncRequestVoteReply* pMsg) {
  syncRequestVoteReplyDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncRequestVoteReply2Json(const SyncRequestVoteReply* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  cJSON_AddNumberToObject(pDestId, "addr", pMsg->destId.addr);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->term);
  cJSON_AddStringToObject(pRoot, "term", u64buf);
  cJSON_AddNumberToObject(pRoot, "vote_granted", pMsg->voteGranted);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncRequestVoteReply", pRoot);
  return pJson;
}

// ---- message process SyncAppendEntries----
SyncAppendEntries* syncAppendEntriesBuild(uint32_t dataLen) {
  uint32_t           bytes = SYNC_APPEND_ENTRIES_FIX_LEN + dataLen;
  SyncAppendEntries* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_APPEND_ENTRIES;
  pMsg->dataLen = dataLen;
  return pMsg;
}

void syncAppendEntriesDestroy(SyncAppendEntries* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncAppendEntriesSerialize(const SyncAppendEntries* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncAppendEntriesDeserialize(const char* buf, uint32_t len, SyncAppendEntries* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
  assert(pMsg->bytes == SYNC_APPEND_ENTRIES_FIX_LEN + pMsg->dataLen);
}

void syncAppendEntries2RpcMsg(const SyncAppendEntries* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncAppendEntriesSerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncAppendEntriesFromRpcMsg(const SRpcMsg* pRpcMsg, SyncAppendEntries* pMsg) {
  syncAppendEntriesDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncAppendEntries2Json(const SyncAppendEntries* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->destId.addr);
  cJSON_AddStringToObject(pDestId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->prevLogIndex);
  cJSON_AddStringToObject(pRoot, "pre_log_index", u64buf);

  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->prevLogTerm);
  cJSON_AddStringToObject(pRoot, "pre_log_term", u64buf);

  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->commitIndex);
  cJSON_AddStringToObject(pRoot, "commit_index", u64buf);

  cJSON_AddNumberToObject(pRoot, "dataLen", pMsg->dataLen);
  cJSON_AddStringToObject(pRoot, "data", pMsg->data);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncAppendEntries", pRoot);
  return pJson;
}

// ---- message process SyncAppendEntriesReply----
SyncAppendEntriesReply* syncAppendEntriesReplyBuild() {
  uint32_t                bytes = sizeof(SyncAppendEntriesReply);
  SyncAppendEntriesReply* pMsg = malloc(bytes);
  memset(pMsg, 0, bytes);
  pMsg->bytes = bytes;
  pMsg->msgType = SYNC_APPEND_ENTRIES_REPLY;
  return pMsg;
}

void syncAppendEntriesReplyDestroy(SyncAppendEntriesReply* pMsg) {
  if (pMsg != NULL) {
    free(pMsg);
  }
}

void syncAppendEntriesReplySerialize(const SyncAppendEntriesReply* pMsg, char* buf, uint32_t bufLen) {
  assert(pMsg->bytes <= bufLen);
  memcpy(buf, pMsg, pMsg->bytes);
}

void syncAppendEntriesReplyDeserialize(const char* buf, uint32_t len, SyncAppendEntriesReply* pMsg) {
  memcpy(pMsg, buf, len);
  assert(len == pMsg->bytes);
}

void syncAppendEntriesReply2RpcMsg(const SyncAppendEntriesReply* pMsg, SRpcMsg* pRpcMsg) {
  memset(pRpcMsg, 0, sizeof(*pRpcMsg));
  pRpcMsg->msgType = pMsg->msgType;
  pRpcMsg->contLen = pMsg->bytes;
  pRpcMsg->pCont = rpcMallocCont(pRpcMsg->contLen);
  syncAppendEntriesReplySerialize(pMsg, pRpcMsg->pCont, pRpcMsg->contLen);
}

void syncAppendEntriesReplyFromRpcMsg(const SRpcMsg* pRpcMsg, SyncAppendEntriesReply* pMsg) {
  syncAppendEntriesReplyDeserialize(pRpcMsg->pCont, pRpcMsg->contLen, pMsg);
}

cJSON* syncAppendEntriesReply2Json(const SyncAppendEntriesReply* pMsg) {
  char u64buf[128];

  cJSON* pRoot = cJSON_CreateObject();
  cJSON_AddNumberToObject(pRoot, "bytes", pMsg->bytes);
  cJSON_AddNumberToObject(pRoot, "msgType", pMsg->msgType);

  cJSON* pSrcId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->srcId.addr);
  cJSON_AddStringToObject(pSrcId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->srcId.addr;
    cJSON*   pTmp = pSrcId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pSrcId, "vgId", pMsg->srcId.vgId);
  cJSON_AddItemToObject(pRoot, "srcId", pSrcId);

  cJSON* pDestId = cJSON_CreateObject();
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->destId.addr);
  cJSON_AddStringToObject(pDestId, "addr", u64buf);
  {
    uint64_t u64 = pMsg->destId.addr;
    cJSON*   pTmp = pDestId;
    char     host[128];
    uint16_t port;
    syncUtilU642Addr(u64, host, sizeof(host), &port);
    cJSON_AddStringToObject(pTmp, "addr_host", host);
    cJSON_AddNumberToObject(pTmp, "addr_port", port);
  }
  cJSON_AddNumberToObject(pDestId, "vgId", pMsg->destId.vgId);
  cJSON_AddItemToObject(pRoot, "destId", pDestId);

  cJSON_AddNumberToObject(pRoot, "success", pMsg->success);
  snprintf(u64buf, sizeof(u64buf), "%lu", pMsg->matchIndex);
  cJSON_AddStringToObject(pRoot, "match_index", u64buf);

  cJSON* pJson = cJSON_CreateObject();
  cJSON_AddItemToObject(pJson, "SyncAppendEntriesReply", pRoot);
  return pJson;
}
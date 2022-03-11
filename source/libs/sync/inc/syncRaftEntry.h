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

#ifndef _TD_LIBS_SYNC_RAFT_ENTRY_H
#define _TD_LIBS_SYNC_RAFT_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "syncInt.h"
#include "syncMessage.h"
#include "taosdef.h"

typedef struct SSyncRaftEntry {
  uint32_t  bytes;
  uint32_t  msgType;
  uint32_t  originalRpcType;
  uint64_t  seqNum;
  bool      isWeak;
  SyncTerm  term;
  SyncIndex index;
  uint32_t  dataLen;
  char      data[];
} SSyncRaftEntry;

SSyncRaftEntry* syncEntryBuild(uint32_t dataLen);
SSyncRaftEntry* syncEntryBuild2(SyncClientRequest* pMsg, SyncTerm term, SyncIndex index);
void            syncEntryDestory(SSyncRaftEntry* pEntry);
char*           syncEntrySerialize(const SSyncRaftEntry* pEntry, uint32_t* len);
SSyncRaftEntry* syncEntryDeserialize(const char* buf, uint32_t len);
cJSON*          syncEntry2Json(const SSyncRaftEntry* pEntry);
char*           syncEntry2Str(const SSyncRaftEntry* pEntry);
void            syncEntryPrint(const SSyncRaftEntry* pEntry);
void            syncEntryPrint2(char *s, const SSyncRaftEntry* pEntry);

#ifdef __cplusplus
}
#endif

#endif /*_TD_LIBS_SYNC_RAFT_ENTRY_H*/
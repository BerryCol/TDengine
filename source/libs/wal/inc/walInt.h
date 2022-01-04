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

#ifndef _TD_WAL_INT_H_
#define _TD_WAL_INT_H_

#include "compare.h"
#include "tchecksum.h"
#include "wal.h"

#ifdef __cplusplus
extern "C" {
#endif

// meta section begin
typedef struct WalFileInfo {
  int64_t firstVer;
  int64_t lastVer;
  int64_t createTs;
  int64_t closeTs;
  int64_t fileSize;
} SWalFileInfo;

typedef struct WalIdxEntry {
  int64_t ver;
  int64_t offset;
} SWalIdxEntry;

static inline int32_t compareWalFileInfo(const void* pLeft, const void* pRight) {
  SWalFileInfo* pInfoLeft = (SWalFileInfo*)pLeft;
  SWalFileInfo* pInfoRight = (SWalFileInfo*)pRight;
  return compareInt64Val(&pInfoLeft->firstVer, &pInfoRight->firstVer);
}

static inline int64_t walGetLastFileSize(SWal* pWal) {
  SWalFileInfo* pInfo = (SWalFileInfo*)taosArrayGetLast(pWal->fileInfoSet);
  return pInfo->fileSize;
}

static inline int64_t walGetLastFileFirstVer(SWal* pWal) {
  SWalFileInfo* pInfo = (SWalFileInfo*)taosArrayGetLast(pWal->fileInfoSet);
  return pInfo->firstVer;
}

static inline int64_t walGetCurFileFirstVer(SWal* pWal) {
  SWalFileInfo* pInfo = (SWalFileInfo*)taosArrayGet(pWal->fileInfoSet, pWal->writeCur);
  return pInfo->firstVer;
}

static inline int64_t walGetCurFileLastVer(SWal* pWal) {
  SWalFileInfo* pInfo = (SWalFileInfo*)taosArrayGet(pWal->fileInfoSet, pWal->writeCur);
  return pInfo->firstVer;
}

static inline int64_t walGetCurFileOffset(SWal* pWal) {
  SWalFileInfo* pInfo = (SWalFileInfo*)taosArrayGet(pWal->fileInfoSet, pWal->writeCur);
  return pInfo->fileSize;
}

static inline bool walCurFileClosed(SWal* pWal) { return taosArrayGetSize(pWal->fileInfoSet) != pWal->writeCur; }

static inline SWalFileInfo* walGetCurFileInfo(SWal* pWal) {
  return (SWalFileInfo*)taosArrayGet(pWal->fileInfoSet, pWal->writeCur);
}

static inline int walBuildLogName(SWal* pWal, int64_t fileFirstVer, char* buf) {
  return sprintf(buf, "%s/%020" PRId64 "." WAL_LOG_SUFFIX, pWal->path, fileFirstVer);
}

static inline int walBuildIdxName(SWal* pWal, int64_t fileFirstVer, char* buf) {
  return sprintf(buf, "%s/%020" PRId64 "." WAL_INDEX_SUFFIX, pWal->path, fileFirstVer);
}

static inline int walValidHeadCksum(SWalHead* pHead) {
  return taosCheckChecksum((uint8_t*)&pHead->head, sizeof(SWalReadHead), pHead->cksumHead);
}

static inline int walValidBodyCksum(SWalHead* pHead) {
  return taosCheckChecksum((uint8_t*)pHead->head.body, pHead->head.len, pHead->cksumBody);
}

static inline int walValidCksum(SWalHead* pHead, void* body, int64_t bodyLen) {
  return walValidHeadCksum(pHead) && walValidBodyCksum(pHead);
}

static inline uint32_t walCalcHeadCksum(SWalHead* pHead) {
  return taosCalcChecksum(0, (uint8_t*)&pHead->head, sizeof(SWalReadHead));
}

static inline uint32_t walCalcBodyCksum(const void* body, uint32_t len) {
  return taosCalcChecksum(0, (uint8_t*)body, len);
}

static inline int64_t walGetVerIdxOffset(SWal* pWal, int64_t ver) {
  return (ver - walGetCurFileFirstVer(pWal)) * sizeof(SWalIdxEntry);
}

static inline void walResetVer(SWalVer* pVer) {
  pVer->firstVer = -1;
  pVer->verInSnapshotting = -1;
  pVer->snapshotVer = -1;
  pVer->commitVer = -1;
  pVer->lastVer = -1;
}

int walLoadMeta(SWal* pWal);
int walSaveMeta(SWal* pWal);
int walRollFileInfo(SWal* pWal);

int walCheckAndRepairMeta(SWal* pWal);

int walCheckAndRepairIdx(SWal* pWal);

char* walMetaSerialize(SWal* pWal);
int   walMetaDeserialize(SWal* pWal, const char* bytes);
// meta section end

// seek section
int walChangeFile(SWal* pWal, int64_t ver);
// seek section end

int64_t walGetSeq();
int     walSeekVer(SWal* pWal, int64_t ver);
int     walRoll(SWal* pWal);

#ifdef __cplusplus
}
#endif

#endif /*_TD_WAL_INT_H_*/

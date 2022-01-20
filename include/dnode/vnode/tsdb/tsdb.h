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

#ifndef _TD_TSDB_H_
#define _TD_TSDB_H_

#include "mallocator.h"
#include "meta.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDataStatis {
  int16_t colId;
  int64_t sum;
  int64_t max;
  int64_t min;
  int16_t maxIndex;
  int16_t minIndex;
  int16_t numOfNull;
} SDataStatis;

typedef struct STable {
  uint64_t  tid;
  uint64_t  uid;
  STSchema *pSchema;
} STable;

#define TABLE_TID(t) (t)->tid
#define TABLE_UID(t) (t)->uid

// TYPES EXPOSED
typedef struct STsdb STsdb;

typedef struct STsdbCfg {
  int8_t   precision;
  uint64_t lruCacheSize;
  int32_t  daysPerFile;
  int32_t  minRowsPerFileBlock;
  int32_t  maxRowsPerFileBlock;
  int32_t  keep;
  int32_t  keep1;
  int32_t  keep2;
  int8_t   update;
  int8_t   compression;
} STsdbCfg;

// STsdb
STsdb *tsdbOpen(const char *path, int32_t vgId, const STsdbCfg *pTsdbCfg, SMemAllocatorFactory *pMAF, SMeta *pMeta);
void   tsdbClose(STsdb *);
void   tsdbRemove(const char *path);
int    tsdbInsertData(STsdb *pTsdb, SSubmitMsg *pMsg, SSubmitRsp *pRsp);
int    tsdbPrepareCommit(STsdb *pTsdb);
int    tsdbCommit(STsdb *pTsdb);

// STsdbCfg
int  tsdbOptionsInit(STsdbCfg *);
void tsdbOptionsClear(STsdbCfg *);

#ifdef __cplusplus
}
#endif

#endif /*_TD_TSDB_H_*/

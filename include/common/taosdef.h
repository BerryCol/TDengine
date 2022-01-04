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

#ifndef _TD_COMMON_TAOS_DEF_H_
#define _TD_COMMON_TAOS_DEF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "taos.h"
#include "tdef.h"

typedef uint64_t tb_uid_t;

#define TSWINDOW_INITIALIZER ((STimeWindow){INT64_MIN, INT64_MAX})
#define TSWINDOW_DESC_INITIALIZER ((STimeWindow){INT64_MAX, INT64_MIN})
#define IS_TSWINDOW_SPECIFIED(win) (((win).skey != INT64_MIN) || ((win).ekey != INT64_MAX))

typedef enum {
  TAOS_QTYPE_RPC = 1,
  TAOS_QTYPE_FWD = 2,
  TAOS_QTYPE_WAL = 3,
  TAOS_QTYPE_CQ = 4,
  TAOS_QTYPE_QUERY = 5
} EQType;

typedef enum {
  TSDB_SUPER_TABLE = 1,   // super table
  TSDB_CHILD_TABLE = 2,   // table created from super table
  TSDB_NORMAL_TABLE = 3,  // ordinary table
  TSDB_STREAM_TABLE = 4,  // table created from stream computing
  TSDB_TEMP_TABLE = 5,    // temp table created by nest query
  TSDB_TABLE_MAX = 6
} ETableType;

typedef enum {
  TSDB_MOD_MNODE = 1,
  TSDB_MOD_HTTP = 2,
  TSDB_MOD_MONITOR = 3,
  TSDB_MOD_MQTT = 4,
  TSDB_MOD_MAX = 5
} EModuleType;

typedef enum {
  TSDB_CHECK_ITEM_NETWORK,
  TSDB_CHECK_ITEM_MEM,
  TSDB_CHECK_ITEM_CPU,
  TSDB_CHECK_ITEM_DISK,
  TSDB_CHECK_ITEM_OS,
  TSDB_CHECK_ITEM_ACCESS,
  TSDB_CHECK_ITEM_VERSION,
  TSDB_CHECK_ITEM_DATAFILE,
  TSDB_CHECK_ITEM_MAX
} ECheckItemType;

typedef enum { TD_ROW_DISCARD_UPDATE = 0, TD_ROW_OVERWRITE_UPDATE = 1, TD_ROW_PARTIAL_UPDATE = 2 } TDUpdateConfig;

extern char *qtypeStr[];

#ifdef __cplusplus
}
#endif

#endif /*_TD_COMMON_TAOS_DEF_H_*/

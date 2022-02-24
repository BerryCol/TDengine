
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

#ifndef _TD_CONFIG_H_
#define _TD_CONFIG_H_

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_NAME_MAX_LEN 128

typedef enum {
  CFG_STYPE_DEFAULT,
  CFG_STYPE_CFG_FILE,
  CFG_STYPE_ENV_FILE,
  CFG_STYPE_ENV_VAR,
  CFG_STYPE_APOLLO_URL,
  CFG_STYPE_ARG_LIST,
  CFG_STYPE_API_OPTION
} ECfgSrcType;

typedef enum {
  CFG_DTYPE_NONE,
  CFG_DTYPE_BOOL,
  CFG_DTYPE_INT32,
  CFG_DTYPE_INT64,
  CFG_DTYPE_FLOAT,
  CFG_DTYPE_STRING,
  CFG_DTYPE_IPSTR,
  CFG_DTYPE_DIR,
  CFG_DTYPE_LOCALE,
  CFG_DTYPE_CHARSET,
  CFG_DTYPE_TIMEZONE
} ECfgDataType;

typedef struct SConfigItem {
  ECfgSrcType  stype;
  ECfgDataType dtype;
  char        *name;
  union {
    bool     bval;
    float    fval;
    int32_t  i32;
    int64_t  i64;
    char    *str;
  };
  union {
    int64_t imin;
    double  fmin;
  };
  union {
    int64_t imax;
    double  fmax;
  };
} SConfigItem;

typedef struct SConfig SConfig;

SConfig *cfgInit();
int32_t  cfgLoad(SConfig *pCfg, ECfgSrcType cfgType, const char *sourceStr);
void     cfgCleanup(SConfig *pCfg);

int32_t      cfgGetSize(SConfig *pCfg);
SConfigItem *cfgIterate(SConfig *pCfg, SConfigItem *pIter);
void         cfgCancelIterate(SConfig *pCfg, SConfigItem *pIter);
SConfigItem *cfgGetItem(SConfig *pCfg, const char *name);

int32_t cfgAddBool(SConfig *pCfg, const char *name, bool defaultVal);
int32_t cfgAddInt32(SConfig *pCfg, const char *name, int32_t defaultVal, int64_t minval, int64_t maxval);
int32_t cfgAddInt64(SConfig *pCfg, const char *name, int64_t defaultVal, int64_t minval, int64_t maxval);
int32_t cfgAddFloat(SConfig *pCfg, const char *name, float defaultVal, double minval, double maxval);
int32_t cfgAddString(SConfig *pCfg, const char *name, const char *defaultVal);
int32_t cfgAddIpStr(SConfig *pCfg, const char *name, const char *defaultVa);
int32_t cfgAddDir(SConfig *pCfg, const char *name, const char *defaultVal);
int32_t cfgAddLocale(SConfig *pCfg, const char *name, const char *defaultVal);
int32_t cfgAddCharset(SConfig *pCfg, const char *name, const char *defaultVal);
int32_t cfgAddTimezone(SConfig *pCfg, const char *name, const char *defaultVal);

const char *cfgStypeStr(ECfgSrcType type);
const char *cfgDtypeStr(ECfgDataType type);

void cfgDumpCfg(SConfig *pCfg);

#ifdef __cplusplus
}
#endif

#endif /*_TD_CONFIG_H_*/

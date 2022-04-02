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

#ifndef _TD_DND_H_
#define _TD_DND_H_

#include "os.h"

#include "cJSON.h"
#include "tcache.h"
#include "tcrc32c.h"
#include "tdatablock.h"
#include "tglobal.h"
#include "thash.h"
#include "tlockfree.h"
#include "tlog.h"
#include "tmsg.h"
#include "tmsgcb.h"
#include "tprocess.h"
#include "tqueue.h"
#include "trpc.h"
#include "tthread.h"
#include "ttime.h"
#include "tworker.h"

#include "dnode.h"
#include "monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define dFatal(...)                                              \
  {                                                              \
    if (dDebugFlag & DEBUG_FATAL) {                              \
      taosPrintLog("DND FATAL ", DEBUG_FATAL, 255, __VA_ARGS__); \
    }                                                            \
  }
#define dError(...)                                              \
  {                                                              \
    if (dDebugFlag & DEBUG_ERROR) {                              \
      taosPrintLog("DND ERROR ", DEBUG_ERROR, 255, __VA_ARGS__); \
    }                                                            \
  }
#define dWarn(...)                                             \
  {                                                            \
    if (dDebugFlag & DEBUG_WARN) {                             \
      taosPrintLog("DND WARN ", DEBUG_WARN, 255, __VA_ARGS__); \
    }                                                          \
  }
#define dInfo(...)                                        \
  {                                                       \
    if (dDebugFlag & DEBUG_INFO) {                        \
      taosPrintLog("DND ", DEBUG_INFO, 255, __VA_ARGS__); \
    }                                                     \
  }
#define dDebug(...)                                               \
  {                                                               \
    if (dDebugFlag & DEBUG_DEBUG) {                               \
      taosPrintLog("DND ", DEBUG_DEBUG, dDebugFlag, __VA_ARGS__); \
    }                                                             \
  }
#define dTrace(...)                                               \
  {                                                               \
    if (dDebugFlag & DEBUG_TRACE) {                               \
      taosPrintLog("DND ", DEBUG_TRACE, dDebugFlag, __VA_ARGS__); \
    }                                                             \
  }

typedef enum { DNODE, VNODES, QNODE, SNODE, MNODE, BNODE, NODE_MAX } ENodeType;
typedef enum { DND_STAT_INIT, DND_STAT_RUNNING, DND_STAT_STOPPED } EDndStatus;
typedef enum { DND_ENV_INIT, DND_ENV_READY, DND_ENV_CLEANUP } EEnvStatus;
typedef enum { PROC_SINGLE, PROC_CHILD, PROC_PARENT } EProcType;

typedef struct SMgmtFp      SMgmtFp;
typedef struct SMgmtWrapper SMgmtWrapper;
typedef struct SMsgHandle   SMsgHandle;
typedef struct SDnodeMgmt   SDnodeMgmt;
typedef struct SVnodesMgmt  SVnodesMgmt;
typedef struct SMnodeMgmt   SMnodeMgmt;
typedef struct SQnodeMgmt   SQnodeMgmt;
typedef struct SSnodeMgmt   SSnodeMgmt;
typedef struct SBnodeMgmt   SBnodeMgmt;

typedef int32_t (*NodeMsgFp)(SMgmtWrapper *pWrapper, SNodeMsg *pMsg);
typedef int32_t (*OpenNodeFp)(SMgmtWrapper *pWrapper);
typedef void (*CloseNodeFp)(SMgmtWrapper *pWrapper);
typedef int32_t (*StartNodeFp)(SMgmtWrapper *pWrapper);
typedef int32_t (*CreateNodeFp)(SMgmtWrapper *pWrapper, SNodeMsg *pMsg);
typedef int32_t (*DropNodeFp)(SMgmtWrapper *pWrapper, SNodeMsg *pMsg);
typedef int32_t (*RequireNodeFp)(SMgmtWrapper *pWrapper, bool *required);

typedef struct SMsgHandle {
  SMgmtWrapper *pQndWrapper;
  SMgmtWrapper *pMndWrapper;
  SMgmtWrapper *pWrapper;
} SMsgHandle;

typedef struct SMgmtFp {
  OpenNodeFp    openFp;
  CloseNodeFp   closeFp;
  StartNodeFp   startFp;
  CreateNodeFp  createMsgFp;
  DropNodeFp    dropMsgFp;
  RequireNodeFp requiredFp;
} SMgmtFp;

typedef struct SMgmtWrapper {
  const char *name;
  char       *path;
  int32_t     refCount;
  SRWLatch    latch;
  ENodeType   ntype;
  bool        deployed;
  bool        required;
  EProcType   procType;
  int32_t     procId;
  SProcObj   *pProc;
  SShm        shm;
  void       *pMgmt;
  SDnode     *pDnode;
  SMgmtFp     fp;
  int8_t      msgVgIds[TDMT_MAX];  // Handle the case where the same message type is distributed to qnode or vnode
  NodeMsgFp   msgFps[TDMT_MAX];
} SMgmtWrapper;

typedef struct {
  void      *serverRpc;
  void      *clientRpc;
  SMsgHandle msgHandles[TDMT_MAX];
} STransMgmt;

typedef struct SDnode {
  int64_t      clusterId;
  int32_t      dnodeId;
  int32_t      numOfSupportVnodes;
  int64_t      rebootTime;
  char        *localEp;
  char        *localFqdn;
  char        *firstEp;
  char        *secondEp;
  char        *dataDir;
  SDiskCfg    *disks;
  int32_t      numOfDisks;
  uint16_t     serverPort;
  bool         dropped;
  ENodeType    ntype;
  EDndStatus   status;
  EDndEvent    event;
  SStartupReq  startup;
  TdFilePtr    lockfile;
  STransMgmt   trans;
  SMgmtWrapper wrappers[NODE_MAX];
} SDnode;

// dndFile.h
int32_t dndReadFile(SMgmtWrapper *pWrapper, bool *pDeployed);
int32_t dndWriteFile(SMgmtWrapper *pWrapper, bool deployed);

// dndInt.h
EDndStatus    dndGetStatus(SDnode *pDnode);
void          dndSetStatus(SDnode *pDnode, EDndStatus stat);
void          dndSetMsgHandle(SMgmtWrapper *pWrapper, tmsg_t msgType, NodeMsgFp nodeMsgFp, int8_t vgId);
SMgmtWrapper *dndAcquireWrapper(SDnode *pDnode, ENodeType nodeType);
int32_t       dndMarkWrapper(SMgmtWrapper *pWrapper);
void          dndReleaseWrapper(SMgmtWrapper *pWrapper);
void          dndReportStartup(SDnode *pDnode, const char *pName, const char *pDesc);

// dndStr.h
const char *dndStatStr(EDndStatus stat);
const char *dndNodeLogStr(ENodeType ntype);
const char *dndNodeProcStr(ENodeType ntype);
const char *dndEventStr(EDndEvent ev);

// dndTransport.h
int32_t  dndInitTrans(SDnode *pDnode);
void     dndCleanupTrans(SDnode *pDnode);
SMsgCb   dndCreateMsgcb(SMgmtWrapper *pWrapper);
SProcCfg dndGenProcCfg(SMgmtWrapper *pWrapper);
int32_t  dndInitMsgHandle(SDnode *pDnode);

// mgmt
void dmSetMgmtFp(SMgmtWrapper *pWrapper);
void bmSetMgmtFp(SMgmtWrapper *pWrapper);
void qmSetMgmtFp(SMgmtWrapper *pMgmt);
void smSetMgmtFp(SMgmtWrapper *pWrapper);
void vmSetMgmtFp(SMgmtWrapper *pWrapper);
void mmSetMgmtFp(SMgmtWrapper *pMgmt);

void dmGetMnodeEpSet(SDnodeMgmt *pMgmt, SEpSet *pEpSet);
void dmUpdateMnodeEpSet(SDnodeMgmt *pMgmt, SEpSet *pEpSet);
void dmSendRedirectRsp(SDnodeMgmt *pMgmt, const SRpcMsg *pMsg);

typedef struct {
  int32_t openVnodes;
  int32_t totalVnodes;
  int32_t masterNum;
  int64_t numOfSelectReqs;
  int64_t numOfInsertReqs;
  int64_t numOfInsertSuccessReqs;
  int64_t numOfBatchInsertReqs;
  int64_t numOfBatchInsertSuccessReqs;
} SVnodesStat;

void    vmMonitorVnodeLoads(SMgmtWrapper *pWrapper, SArray *pLoads);
int32_t vmMonitorTfsInfo(SMgmtWrapper *pWrapper, SMonDiskInfo *pInfo);
void    vmMonitorVnodeReqs(SMgmtWrapper *pWrapper, SMonDnodeInfo *pInfo);
int32_t mmMonitorMnodeInfo(SMgmtWrapper *pWrapper, SMonClusterInfo *pClusterInfo, SMonVgroupInfo *pVgroupInfo,
                           SMonGrantInfo *pGrantInfo);

#ifdef __cplusplus
}
#endif

#endif /*_TD_DND_H_*/
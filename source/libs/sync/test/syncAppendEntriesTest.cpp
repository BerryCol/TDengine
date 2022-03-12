#include <gtest/gtest.h>
#include <stdio.h>
#include "syncIO.h"
#include "syncInt.h"
#include "syncMessage.h"
#include "syncUtil.h"

void logTest() {
  sTrace("--- sync log test: trace");
  sDebug("--- sync log test: debug");
  sInfo("--- sync log test: info");
  sWarn("--- sync log test: warn");
  sError("--- sync log test: error");
  sFatal("--- sync log test: fatal");
}

SyncAppendEntries *createMsg() {
  SyncAppendEntries *pMsg = syncAppendEntriesBuild(20);
  pMsg->srcId.addr = syncUtilAddr2U64("127.0.0.1", 1234);
  pMsg->srcId.vgId = 100;
  pMsg->destId.addr = syncUtilAddr2U64("127.0.0.1", 5678);
  pMsg->destId.vgId = 100;
  pMsg->prevLogIndex = 11;
  pMsg->prevLogTerm = 22;
  pMsg->commitIndex = 33;
  strcpy(pMsg->data, "hello world");
  return pMsg;
}

void test1() {
  SyncAppendEntries *pMsg = createMsg();
  syncAppendEntriesPrint2((char *)"test1:", pMsg);
  syncAppendEntriesDestroy(pMsg);
}

void test2() {
  SyncAppendEntries *pMsg = createMsg();
  uint32_t           len = pMsg->bytes;
  char *             serialized = (char *)malloc(len);
  syncAppendEntriesSerialize(pMsg, serialized, len);
  SyncAppendEntries *pMsg2 = syncAppendEntriesBuild(pMsg->dataLen);
  syncAppendEntriesDeserialize(serialized, len, pMsg2);
  syncAppendEntriesPrint2((char *)"test2: syncAppendEntriesSerialize -> syncAppendEntriesDeserialize ", pMsg2);

  free(serialized);
  syncAppendEntriesDestroy(pMsg);
  syncAppendEntriesDestroy(pMsg2);
}

void test3() {
  SyncAppendEntries *pMsg = createMsg();
  uint32_t           len;
  char *             serialized = syncAppendEntriesSerialize2(pMsg, &len);
  SyncAppendEntries *pMsg2 = syncAppendEntriesDeserialize2(serialized, len);
  syncAppendEntriesPrint2((char *)"test3: syncAppendEntriesSerialize3 -> syncAppendEntriesDeserialize2 ", pMsg2);

  free(serialized);
  syncAppendEntriesDestroy(pMsg);
  syncAppendEntriesDestroy(pMsg2);
}

void test4() {
  SyncAppendEntries *pMsg = createMsg();
  SRpcMsg            rpcMsg;
  syncAppendEntries2RpcMsg(pMsg, &rpcMsg);
  SyncAppendEntries *pMsg2 = (SyncAppendEntries *)malloc(rpcMsg.contLen);
  syncAppendEntriesFromRpcMsg(&rpcMsg, pMsg2);
  syncAppendEntriesPrint2((char *)"test4: syncAppendEntries2RpcMsg -> syncAppendEntriesFromRpcMsg ", pMsg2);

  syncAppendEntriesDestroy(pMsg);
  syncAppendEntriesDestroy(pMsg2);
}

void test5() {
  SyncAppendEntries *pMsg = createMsg();
  SRpcMsg            rpcMsg;
  syncAppendEntries2RpcMsg(pMsg, &rpcMsg);
  SyncAppendEntries *pMsg2 = syncAppendEntriesFromRpcMsg2(&rpcMsg);
  syncAppendEntriesPrint2((char *)"test5: syncAppendEntries2RpcMsg -> syncAppendEntriesFromRpcMsg2 ", pMsg2);

  syncAppendEntriesDestroy(pMsg);
  syncAppendEntriesDestroy(pMsg2);
}

int main() {
  // taosInitLog((char *)"syncTest.log", 100000, 10);
  tsAsyncLog = 0;
  sDebugFlag = 143 + 64;
  logTest();

  test1();
  test2();
  test3();
  test4();
  test5();

  return 0;
}

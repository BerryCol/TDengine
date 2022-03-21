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

int gg = 0;

SyncTimeout *createMsg() {
  SyncTimeout *pMsg = syncTimeoutBuild2(SYNC_TIMEOUT_PING, 999, 333, &gg);
  return pMsg;
}

void test1() {
  SyncTimeout *pMsg = createMsg();
  syncTimeoutPrint2((char *)"test1:", pMsg);
  syncTimeoutDestroy(pMsg);
}

void test2() {
  SyncTimeout *pMsg = createMsg();
  uint32_t     len = pMsg->bytes;
  char *       serialized = (char *)malloc(len);
  syncTimeoutSerialize(pMsg, serialized, len);
  SyncTimeout *pMsg2 = syncTimeoutBuild();
  syncTimeoutDeserialize(serialized, len, pMsg2);
  syncTimeoutPrint2((char *)"test2: syncTimeoutSerialize -> syncTimeoutDeserialize ", pMsg2);

  free(serialized);
  syncTimeoutDestroy(pMsg);
  syncTimeoutDestroy(pMsg2);
}

void test3() {
  SyncTimeout *pMsg = createMsg();
  uint32_t     len;
  char *       serialized = syncTimeoutSerialize2(pMsg, &len);
  SyncTimeout *pMsg2 = syncTimeoutDeserialize2(serialized, len);
  syncTimeoutPrint2((char *)"test3: syncTimeoutSerialize3 -> syncTimeoutDeserialize2 ", pMsg2);

  free(serialized);
  syncTimeoutDestroy(pMsg);
  syncTimeoutDestroy(pMsg2);
}

void test4() {
  SyncTimeout *pMsg = createMsg();
  SRpcMsg      rpcMsg;
  syncTimeout2RpcMsg(pMsg, &rpcMsg);
  SyncTimeout *pMsg2 = (SyncTimeout *)malloc(rpcMsg.contLen);
  syncTimeoutFromRpcMsg(&rpcMsg, pMsg2);
  syncTimeoutPrint2((char *)"test4: syncTimeout2RpcMsg -> syncTimeoutFromRpcMsg ", pMsg2);

  syncTimeoutDestroy(pMsg);
  syncTimeoutDestroy(pMsg2);
}

void test5() {
  SyncTimeout *pMsg = createMsg();
  SRpcMsg      rpcMsg;
  syncTimeout2RpcMsg(pMsg, &rpcMsg);
  SyncTimeout *pMsg2 = syncTimeoutFromRpcMsg2(&rpcMsg);
  syncTimeoutPrint2((char *)"test5: syncTimeout2RpcMsg -> syncTimeoutFromRpcMsg2 ", pMsg2);

  syncTimeoutDestroy(pMsg);
  syncTimeoutDestroy(pMsg2);
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

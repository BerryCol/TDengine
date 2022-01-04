/**
 * @file profile.cpp
 * @author slguan (slguan@taosdata.com)
 * @brief DNODE module profile-msg tests
 * @version 0.1
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "base.h"

class DndTestProfile : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { test.Init("/tmp/dnode_test_profile", 9080); }
  static void TearDownTestSuite() { test.Cleanup(); }

  static Testbase test;

 public:
  void SetUp() override {}
  void TearDown() override {}

  int32_t connId;
};

Testbase DndTestProfile::test;

TEST_F(DndTestProfile, 01_ConnectMsg) {
  int32_t contLen = sizeof(SConnectMsg);

  SConnectMsg* pReq = (SConnectMsg*)rpcMallocCont(contLen);
  pReq->pid = htonl(1234);
  strcpy(pReq->app, "dnode_test_profile");
  strcpy(pReq->db, "");

  SRpcMsg* pMsg = test.SendMsg(TDMT_MND_CONNECT, pReq, contLen);
  ASSERT_NE(pMsg, nullptr);
  ASSERT_EQ(pMsg->code, 0);

  SConnectRsp* pRsp = (SConnectRsp*)pMsg->pCont;
  ASSERT_NE(pRsp, nullptr);
  pRsp->acctId = htonl(pRsp->acctId);
  pRsp->clusterId = htobe64(pRsp->clusterId);
  pRsp->connId = htonl(pRsp->connId);
  pRsp->epSet.port[0] = htons(pRsp->epSet.port[0]);

  EXPECT_EQ(pRsp->acctId, 1);
  EXPECT_GT(pRsp->clusterId, 0);
  EXPECT_EQ(pRsp->connId, 1);
  EXPECT_EQ(pRsp->superUser, 1);

  EXPECT_EQ(pRsp->epSet.inUse, 0);
  EXPECT_EQ(pRsp->epSet.numOfEps, 1);
  EXPECT_EQ(pRsp->epSet.port[0], 9080);
  EXPECT_STREQ(pRsp->epSet.fqdn[0], "localhost");

  connId = pRsp->connId;
}

TEST_F(DndTestProfile, 02_ConnectMsg_InvalidDB) {
  int32_t contLen = sizeof(SConnectMsg);

  SConnectMsg* pReq = (SConnectMsg*)rpcMallocCont(contLen);
  pReq->pid = htonl(1234);
  strcpy(pReq->app, "dnode_test_profile");
  strcpy(pReq->db, "invalid_db");

  SRpcMsg* pMsg = test.SendMsg(TDMT_MND_CONNECT, pReq, contLen);
  ASSERT_NE(pMsg, nullptr);
  ASSERT_EQ(pMsg->code, TSDB_CODE_MND_INVALID_DB);
  ASSERT_EQ(pMsg->contLen, 0);
}

TEST_F(DndTestProfile, 03_ConnectMsg_Show) {
  test.SendShowMetaMsg(TSDB_MGMT_TABLE_CONNS, "");
  CHECK_META("show connections", 7);
  CHECK_SCHEMA(0, TSDB_DATA_TYPE_INT, 4, "connId");
  CHECK_SCHEMA(1, TSDB_DATA_TYPE_BINARY, TSDB_USER_LEN + VARSTR_HEADER_SIZE, "user");
  CHECK_SCHEMA(2, TSDB_DATA_TYPE_BINARY, TSDB_APP_NAME_LEN + VARSTR_HEADER_SIZE, "program");
  CHECK_SCHEMA(3, TSDB_DATA_TYPE_INT, 4, "pid");
  CHECK_SCHEMA(4, TSDB_DATA_TYPE_BINARY, TSDB_IPv4ADDR_LEN + 6 + VARSTR_HEADER_SIZE, "ip:port");
  CHECK_SCHEMA(5, TSDB_DATA_TYPE_TIMESTAMP, 8, "login_time");
  CHECK_SCHEMA(6, TSDB_DATA_TYPE_TIMESTAMP, 8, "last_access");

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 1);
  CheckInt32(1);
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("dnode_test_profile", TSDB_APP_NAME_LEN);
  CheckInt32(1234);
  IgnoreBinary(TSDB_IPv4ADDR_LEN + 6);
  CheckTimestamp();
  CheckTimestamp();
}

TEST_F(DndTestProfile, 04_HeartBeatMsg) {
  int32_t contLen = sizeof(SHeartBeatMsg);

  SHeartBeatMsg* pReq = (SHeartBeatMsg*)rpcMallocCont(contLen);
  pReq->connId = htonl(connId);
  pReq->pid = htonl(1234);
  pReq->numOfQueries = htonl(0);
  pReq->numOfStreams = htonl(0);
  strcpy(pReq->app, "dnode_test_profile");

  SRpcMsg* pMsg = test.SendMsg(TDMT_MND_HEARTBEAT, pReq, contLen);
  ASSERT_NE(pMsg, nullptr);
  ASSERT_EQ(pMsg->code, 0);

  SHeartBeatRsp* pRsp = (SHeartBeatRsp*)pMsg->pCont;
  ASSERT_NE(pRsp, nullptr);
  pRsp->connId = htonl(pRsp->connId);
  pRsp->queryId = htonl(pRsp->queryId);
  pRsp->streamId = htonl(pRsp->streamId);
  pRsp->totalDnodes = htonl(pRsp->totalDnodes);
  pRsp->onlineDnodes = htonl(pRsp->onlineDnodes);
  pRsp->epSet.port[0] = htons(pRsp->epSet.port[0]);

  EXPECT_EQ(pRsp->connId, connId);
  EXPECT_EQ(pRsp->queryId, 0);
  EXPECT_EQ(pRsp->streamId, 0);
  EXPECT_EQ(pRsp->totalDnodes, 1);
  EXPECT_EQ(pRsp->onlineDnodes, 1);
  EXPECT_EQ(pRsp->killConnection, 0);

  EXPECT_EQ(pRsp->epSet.inUse, 0);
  EXPECT_EQ(pRsp->epSet.numOfEps, 1);
  EXPECT_EQ(pRsp->epSet.port[0], 9080);
  EXPECT_STREQ(pRsp->epSet.fqdn[0], "localhost");
}

TEST_F(DndTestProfile, 05_KillConnMsg) {
  {
    int32_t contLen = sizeof(SKillConnMsg);

    SKillConnMsg* pReq = (SKillConnMsg*)rpcMallocCont(contLen);
    pReq->connId = htonl(connId);

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_KILL_CONN, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
  }

  {
    int32_t contLen = sizeof(SHeartBeatMsg);

    SHeartBeatMsg* pReq = (SHeartBeatMsg*)rpcMallocCont(contLen);
    pReq->connId = htonl(connId);
    pReq->pid = htonl(1234);
    pReq->numOfQueries = htonl(0);
    pReq->numOfStreams = htonl(0);
    strcpy(pReq->app, "dnode_test_profile");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_HEARTBEAT, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, TSDB_CODE_MND_INVALID_CONNECTION);
    ASSERT_EQ(pMsg->contLen, 0);
  }

  {
    int32_t contLen = sizeof(SConnectMsg);

    SConnectMsg* pReq = (SConnectMsg*)rpcMallocCont(contLen);
    pReq->pid = htonl(1234);
    strcpy(pReq->app, "dnode_test_profile");
    strcpy(pReq->db, "");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_CONNECT, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);

    SConnectRsp* pRsp = (SConnectRsp*)pMsg->pCont;
    ASSERT_NE(pRsp, nullptr);
    pRsp->acctId = htonl(pRsp->acctId);
    pRsp->clusterId = htobe64(pRsp->clusterId);
    pRsp->connId = htonl(pRsp->connId);
    pRsp->epSet.port[0] = htons(pRsp->epSet.port[0]);

    EXPECT_EQ(pRsp->acctId, 1);
    EXPECT_GT(pRsp->clusterId, 0);
    EXPECT_GT(pRsp->connId, connId);
    EXPECT_EQ(pRsp->superUser, 1);

    EXPECT_EQ(pRsp->epSet.inUse, 0);
    EXPECT_EQ(pRsp->epSet.numOfEps, 1);
    EXPECT_EQ(pRsp->epSet.port[0], 9080);
    EXPECT_STREQ(pRsp->epSet.fqdn[0], "localhost");

    connId = pRsp->connId;
  }
}

TEST_F(DndTestProfile, 06_KillConnMsg_InvalidConn) {
  int32_t contLen = sizeof(SKillConnMsg);

  SKillConnMsg* pReq = (SKillConnMsg*)rpcMallocCont(contLen);
  pReq->connId = htonl(2345);

  SRpcMsg* pMsg = test.SendMsg(TDMT_MND_KILL_CONN, pReq, contLen);
  ASSERT_NE(pMsg, nullptr);
  ASSERT_EQ(pMsg->code, TSDB_CODE_MND_INVALID_CONN_ID);
}

TEST_F(DndTestProfile, 07_KillQueryMsg) {
  {
    int32_t contLen = sizeof(SKillQueryMsg);

    SKillQueryMsg* pReq = (SKillQueryMsg*)rpcMallocCont(contLen);
    pReq->connId = htonl(connId);
    pReq->queryId = htonl(1234);

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_KILL_QUERY, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
    ASSERT_EQ(pMsg->contLen, 0);
  }

  {
    int32_t contLen = sizeof(SHeartBeatMsg);

    SHeartBeatMsg* pReq = (SHeartBeatMsg*)rpcMallocCont(contLen);
    pReq->connId = htonl(connId);
    pReq->pid = htonl(1234);
    pReq->numOfQueries = htonl(0);
    pReq->numOfStreams = htonl(0);
    strcpy(pReq->app, "dnode_test_profile");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_HEARTBEAT, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);

    SHeartBeatRsp* pRsp = (SHeartBeatRsp*)pMsg->pCont;
    ASSERT_NE(pRsp, nullptr);
    pRsp->connId = htonl(pRsp->connId);
    pRsp->queryId = htonl(pRsp->queryId);
    pRsp->streamId = htonl(pRsp->streamId);
    pRsp->totalDnodes = htonl(pRsp->totalDnodes);
    pRsp->onlineDnodes = htonl(pRsp->onlineDnodes);
    pRsp->epSet.port[0] = htons(pRsp->epSet.port[0]);

    EXPECT_EQ(pRsp->connId, connId);
    EXPECT_EQ(pRsp->queryId, 1234);
    EXPECT_EQ(pRsp->streamId, 0);
    EXPECT_EQ(pRsp->totalDnodes, 1);
    EXPECT_EQ(pRsp->onlineDnodes, 1);
    EXPECT_EQ(pRsp->killConnection, 0);

    EXPECT_EQ(pRsp->epSet.inUse, 0);
    EXPECT_EQ(pRsp->epSet.numOfEps, 1);
    EXPECT_EQ(pRsp->epSet.port[0], 9080);
    EXPECT_STREQ(pRsp->epSet.fqdn[0], "localhost");
  }
}

TEST_F(DndTestProfile, 08_KillQueryMsg_InvalidConn) {
  int32_t contLen = sizeof(SKillQueryMsg);

  SKillQueryMsg* pReq = (SKillQueryMsg*)rpcMallocCont(contLen);
  pReq->connId = htonl(2345);
  pReq->queryId = htonl(1234);

  SRpcMsg* pMsg = test.SendMsg(TDMT_MND_KILL_QUERY, pReq, contLen);
  ASSERT_NE(pMsg, nullptr);
  ASSERT_EQ(pMsg->code, TSDB_CODE_MND_INVALID_CONN_ID);
}

TEST_F(DndTestProfile, 09_KillQueryMsg) {
  test.SendShowMetaMsg(TSDB_MGMT_TABLE_QUERIES, "");
  CHECK_META("show queries", 14);

  CHECK_SCHEMA(0, TSDB_DATA_TYPE_INT, 4, "queryId");
  CHECK_SCHEMA(1, TSDB_DATA_TYPE_INT, 4, "connId");
  CHECK_SCHEMA(2, TSDB_DATA_TYPE_BINARY, TSDB_USER_LEN + VARSTR_HEADER_SIZE, "user");
  CHECK_SCHEMA(3, TSDB_DATA_TYPE_BINARY, TSDB_IPv4ADDR_LEN + 6 + VARSTR_HEADER_SIZE, "ip:port");
  CHECK_SCHEMA(4, TSDB_DATA_TYPE_BINARY, 22 + VARSTR_HEADER_SIZE, "qid");
  CHECK_SCHEMA(5, TSDB_DATA_TYPE_TIMESTAMP, 8, "created_time");
  CHECK_SCHEMA(6, TSDB_DATA_TYPE_BIGINT, 8, "time");
  CHECK_SCHEMA(7, TSDB_DATA_TYPE_BINARY, 18 + VARSTR_HEADER_SIZE, "sql_obj_id");
  CHECK_SCHEMA(8, TSDB_DATA_TYPE_INT, 4, "pid");
  CHECK_SCHEMA(9, TSDB_DATA_TYPE_BINARY, TSDB_EP_LEN + VARSTR_HEADER_SIZE, "ep");
  CHECK_SCHEMA(10, TSDB_DATA_TYPE_BOOL, 1, "stable_query");
  CHECK_SCHEMA(11, TSDB_DATA_TYPE_INT, 4, "sub_queries");
  CHECK_SCHEMA(12, TSDB_DATA_TYPE_BINARY, TSDB_SHOW_SUBQUERY_LEN + VARSTR_HEADER_SIZE, "sub_query_info");
  CHECK_SCHEMA(13, TSDB_DATA_TYPE_BINARY, TSDB_SHOW_SQL_LEN + VARSTR_HEADER_SIZE, "sql");

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 0);
}

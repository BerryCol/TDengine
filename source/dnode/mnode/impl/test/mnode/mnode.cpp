/**
 * @file mnode.cpp
 * @author slguan (slguan@taosdata.com)
 * @brief MNODE module mnode tests
 * @version 1.0
 * @date 2022-01-07
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "sut.h"

class MndTestMnode : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}

 public:
  static void SetUpTestSuite() {
    test.Init("/tmp/mnode_test_mnode1", 9028);
    const char* fqdn = "localhost";
    const char* firstEp = "localhost:9028";

    server2.Start("/tmp/mnode_test_mnode2", fqdn, 9029, firstEp);
    taosMsleep(300);
  }

  static void TearDownTestSuite() {
    server2.Stop();
    test.Cleanup();
  }

  static Testbase   test;
  static TestServer server2;
};

Testbase   MndTestMnode::test;
TestServer MndTestMnode::server2;

TEST_F(MndTestMnode, 01_ShowDnode) {
  test.SendShowMetaReq(TSDB_MGMT_TABLE_MNODE, "");
  CHECK_META("show mnodes", 5);

  CHECK_SCHEMA(0, TSDB_DATA_TYPE_SMALLINT, 2, "id");
  CHECK_SCHEMA(1, TSDB_DATA_TYPE_BINARY, TSDB_EP_LEN + VARSTR_HEADER_SIZE, "endpoint");
  CHECK_SCHEMA(2, TSDB_DATA_TYPE_BINARY, 12 + VARSTR_HEADER_SIZE, "role");
  CHECK_SCHEMA(3, TSDB_DATA_TYPE_TIMESTAMP, 8, "create_time");
  CHECK_SCHEMA(4, TSDB_DATA_TYPE_TIMESTAMP, 8, "role_time");

  test.SendShowRetrieveReq();
  EXPECT_EQ(test.GetShowRows(), 1);

  CheckInt16(1);
  CheckBinary("localhost:9028", TSDB_EP_LEN);
  CheckBinary("master", 12);
  CheckTimestamp();
  IgnoreTimestamp();
}

TEST_F(MndTestMnode, 02_Create_Mnode_Invalid_Id) {
  {
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(1);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_MNODE_ALREADY_EXIST);
  }
}

TEST_F(MndTestMnode, 03_Create_Mnode_Invalid_Id) {
  {
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_DNODE_NOT_EXIST);
  }
}

TEST_F(MndTestMnode, 04_Create_Mnode) {
  {
    // create dnode
    int32_t contLen = sizeof(SCreateDnodeReq);

    SCreateDnodeReq* pReq = (SCreateDnodeReq*)rpcMallocCont(contLen);
    strcpy(pReq->fqdn, "localhost");
    pReq->port = htonl(9029);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_DNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    taosMsleep(1300);
    test.SendShowMetaReq(TSDB_MGMT_TABLE_DNODE, "");
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 2);
  }

  {
    // create mnode
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_MNODE, "");
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 2);

    CheckInt16(1);
    CheckInt16(2);
    CheckBinary("localhost:9028", TSDB_EP_LEN);
    CheckBinary("localhost:9029", TSDB_EP_LEN);
    CheckBinary("master", 12);
    CheckBinary("slave", 12);
    CheckTimestamp();
    CheckTimestamp();
    IgnoreTimestamp();
    IgnoreTimestamp();
  }

  {
    // drop mnode
    int32_t contLen = sizeof(SMDropMnodeReq);

    SMDropMnodeReq* pReq = (SMDropMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, 0);

    test.SendShowMetaReq(TSDB_MGMT_TABLE_MNODE, "");
    test.SendShowRetrieveReq();
    EXPECT_EQ(test.GetShowRows(), 1);

    CheckInt16(1);
    CheckBinary("localhost:9028", TSDB_EP_LEN);
    CheckBinary("master", 12);
    CheckTimestamp();
    IgnoreTimestamp();
  }

  {
    // drop mnode
    int32_t contLen = sizeof(SMDropMnodeReq);

    SMDropMnodeReq* pReq = (SMDropMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_MND_MNODE_NOT_EXIST);
  }
}

TEST_F(MndTestMnode, 03_Create_Mnode_Rollback) {
  {
    // send message first, then dnode2 crash, result is returned, and rollback is started
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    server2.Stop();
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_RPC_NETWORK_UNAVAIL);
  }

  {
    // continue send message, mnode is creating
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_SDB_OBJ_CREATING);
  }

  {
    // continue send message, mnode is creating
    int32_t contLen = sizeof(SMDropMnodeReq);

    SMDropMnodeReq* pReq = (SMDropMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_SDB_OBJ_CREATING);
  }

  {
    // server start, wait until the rollback finished
    server2.DoStart();
    taosMsleep(1000);

    int32_t retry = 0;
    int32_t retryMax = 20;

    for (retry = 0; retry < retryMax; retry++) {
      int32_t contLen = sizeof(SMCreateMnodeReq);

      SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
      pReq->dnodeId = htonl(2);

      SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
      ASSERT_NE(pRsp, nullptr);
      if (pRsp->code == TSDB_CODE_MND_MNODE_ALREADY_EXIST) break;
      taosMsleep(1000);
    }

    ASSERT_NE(retry, retryMax);
  }
}

TEST_F(MndTestMnode, 04_Drop_Mnode_Rollback) {
  {
    // send message first, then dnode2 crash, result is returned, and rollback is started
    int32_t contLen = sizeof(SMDropMnodeReq);

    SMDropMnodeReq* pReq = (SMDropMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    server2.Stop();
    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_RPC_NETWORK_UNAVAIL);
  }

  {
    // continue send message, mnode is dropping
    int32_t contLen = sizeof(SMCreateMnodeReq);

    SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_SDB_OBJ_DROPPING);
  }

  {
    // continue send message, mnode is dropping
    int32_t contLen = sizeof(SMDropMnodeReq);

    SMDropMnodeReq* pReq = (SMDropMnodeReq*)rpcMallocCont(contLen);
    pReq->dnodeId = htonl(2);

    SRpcMsg* pRsp = test.SendReq(TDMT_MND_DROP_MNODE, pReq, contLen);
    ASSERT_NE(pRsp, nullptr);
    ASSERT_EQ(pRsp->code, TSDB_CODE_SDB_OBJ_DROPPING);
  }

  {
    // server start, wait until the rollback finished
    server2.DoStart();
    taosMsleep(1000);

    int32_t retry = 0;
    int32_t retryMax = 20;

    for (retry = 0; retry < retryMax; retry++) {
      int32_t contLen = sizeof(SMCreateMnodeReq);

      SMCreateMnodeReq* pReq = (SMCreateMnodeReq*)rpcMallocCont(contLen);
      pReq->dnodeId = htonl(2);

      SRpcMsg* pRsp = test.SendReq(TDMT_MND_CREATE_MNODE, pReq, contLen);
      ASSERT_NE(pRsp, nullptr);
      if (pRsp->code == 0) break;
      taosMsleep(1000);
    }

    ASSERT_NE(retry, retryMax);
  }
}
/**
 * @file user.cpp
 * @author slguan (slguan@taosdata.com)
 * @brief DNODE module user-msg tests
 * @version 0.1
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "base.h"

class DndTestUser : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { test.Init("/tmp/dnode_test_user", 9140); }
  static void TearDownTestSuite() { test.Cleanup(); }

  static Testbase test;

 public:
  void SetUp() override {}
  void TearDown() override {}
};

Testbase DndTestUser::test;

TEST_F(DndTestUser, 01_ShowUser) {
  test.SendShowMetaMsg(TSDB_MGMT_TABLE_USER, "");
  CHECK_META("show users", 4);

  CHECK_SCHEMA(0, TSDB_DATA_TYPE_BINARY, TSDB_USER_LEN + VARSTR_HEADER_SIZE, "name");
  CHECK_SCHEMA(1, TSDB_DATA_TYPE_BINARY, 10 + VARSTR_HEADER_SIZE, "privilege");
  CHECK_SCHEMA(2, TSDB_DATA_TYPE_TIMESTAMP, 8, "create_time");
  CHECK_SCHEMA(3, TSDB_DATA_TYPE_BINARY, TSDB_USER_LEN + VARSTR_HEADER_SIZE, "account");

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 1);

  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("super", 10);
  CheckTimestamp();
  CheckBinary("root", TSDB_USER_LEN);
}

TEST_F(DndTestUser, 02_Create_Drop_Alter_User) {
  {
    int32_t contLen = sizeof(SCreateUserMsg);

    SCreateUserMsg* pReq = (SCreateUserMsg*)rpcMallocCont(contLen);
    strcpy(pReq->user, "u1");
    strcpy(pReq->pass, "p1");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_CREATE_USER, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
  }

  {
    int32_t contLen = sizeof(SCreateUserMsg);

    SCreateUserMsg* pReq = (SCreateUserMsg*)rpcMallocCont(contLen);
    strcpy(pReq->user, "u2");
    strcpy(pReq->pass, "p2");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_CREATE_USER, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
  }

  test.SendShowMetaMsg(TSDB_MGMT_TABLE_USER, "");
  CHECK_META("show users", 4);

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 3);

  CheckBinary("u1", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("u2", TSDB_USER_LEN);
  CheckBinary("normal", 10);
  CheckBinary("super", 10);
  CheckBinary("normal", 10);
  CheckTimestamp();
  CheckTimestamp();
  CheckTimestamp();
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);

  {
    int32_t contLen = sizeof(SAlterUserMsg);

    SAlterUserMsg* pReq = (SAlterUserMsg*)rpcMallocCont(contLen);
    strcpy(pReq->user, "u1");
    strcpy(pReq->pass, "p2");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_ALTER_USER, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
  }

  test.SendShowMetaMsg(TSDB_MGMT_TABLE_USER, "");
  CHECK_META("show users", 4);

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 3);

  CheckBinary("u1", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("u2", TSDB_USER_LEN);
  CheckBinary("normal", 10);
  CheckBinary("super", 10);
  CheckBinary("normal", 10);
  CheckTimestamp();
  CheckTimestamp();
  CheckTimestamp();
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);

  {
    int32_t contLen = sizeof(SDropUserMsg);

    SDropUserMsg* pReq = (SDropUserMsg*)rpcMallocCont(contLen);
    strcpy(pReq->user, "u1");

    SRpcMsg* pMsg = test.SendMsg(TDMT_MND_DROP_USER, pReq, contLen);
    ASSERT_NE(pMsg, nullptr);
    ASSERT_EQ(pMsg->code, 0);
  }

  test.SendShowMetaMsg(TSDB_MGMT_TABLE_USER, "");
  CHECK_META("show users", 4);

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 2);

  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("u2", TSDB_USER_LEN);
  CheckBinary("super", 10);
  CheckBinary("normal", 10);
  CheckTimestamp();
  CheckTimestamp();
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);

  // restart
  test.Restart();

  test.SendShowMetaMsg(TSDB_MGMT_TABLE_USER, "");
  CHECK_META("show users", 4);

  test.SendShowRetrieveMsg();
  EXPECT_EQ(test.GetShowRows(), 2);

  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("u2", TSDB_USER_LEN);
  CheckBinary("super", 10);
  CheckBinary("normal", 10);
  CheckTimestamp();
  CheckTimestamp();
  CheckBinary("root", TSDB_USER_LEN);
  CheckBinary("root", TSDB_USER_LEN);
}

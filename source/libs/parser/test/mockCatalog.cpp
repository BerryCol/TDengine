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

#include <iostream>
#include "stub.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"

#include <addr_any.h>

#pragma GCC diagnostic pop

#include "mockCatalog.h"
namespace {

void generateTestT1(MockCatalogService* mcs) {
  ITableBuilder& builder = mcs->createTableBuilder("test", "t1", TSDB_NORMAL_TABLE, 6)
      .setPrecision(TSDB_TIME_PRECISION_MILLI).setVgid(1).addColumn("ts", TSDB_DATA_TYPE_TIMESTAMP)
      .addColumn("c1", TSDB_DATA_TYPE_INT).addColumn("c2", TSDB_DATA_TYPE_BINARY, 20).addColumn("c3", TSDB_DATA_TYPE_BIGINT)
      .addColumn("c4", TSDB_DATA_TYPE_DOUBLE).addColumn("c5", TSDB_DATA_TYPE_DOUBLE);
  builder.done();
}

void generateTestST1(MockCatalogService* mcs) {
  ITableBuilder& builder = mcs->createTableBuilder("test", "st1", TSDB_SUPER_TABLE, 3, 2)
      .setPrecision(TSDB_TIME_PRECISION_MILLI).addColumn("ts", TSDB_DATA_TYPE_TIMESTAMP)
      .addColumn("c1", TSDB_DATA_TYPE_INT).addColumn("c2", TSDB_DATA_TYPE_BINARY, 20)
      .addTag("tag1", TSDB_DATA_TYPE_INT).addTag("tag2", TSDB_DATA_TYPE_BINARY, 20);
  builder.done();
  mcs->createSubTable("test", "st1", "st1s1", 1);
  mcs->createSubTable("test", "st1", "st1s2", 2);
}

}

int32_t __catalogGetHandle(const char *clusterId, struct SCatalog** catalogHandle) {
  return 0;
}

int32_t __catalogGetTableMeta(struct SCatalog* pCatalog, void *pRpc, const SEpSet* pMgmtEps, const SName* pTableName, STableMeta** pTableMeta) {
  return mockCatalogService->catalogGetTableMeta(pTableName, pTableMeta);
}

int32_t __catalogGetTableHashVgroup(struct SCatalog* pCatalog, void *pRpc, const SEpSet* pMgmtEps, const SName* pTableName, SVgroupInfo* vgInfo) {
  return mockCatalogService->catalogGetTableHashVgroup(pTableName, vgInfo);
}

int32_t __catalogGetTableDistVgInfo(SCatalog* pCtg, void *pRpc, const SEpSet* pMgmtEps, const SName* pTableName, SArray** pVgList) {
  return mockCatalogService->catalogGetTableDistVgInfo(pTableName, pVgList);
}

int32_t __catalogGetDBVgVersion(SCatalog* pCtg, const char* dbFName, int32_t* version, int64_t* dbId, int32_t *tableNum) {
  return 0;
}

void initMetaDataEnv() {
  mockCatalogService.reset(new MockCatalogService());

  static Stub stub;
  stub.set(catalogGetHandle, __catalogGetHandle);
  stub.set(catalogGetTableMeta, __catalogGetTableMeta);
  stub.set(catalogGetTableHashVgroup, __catalogGetTableHashVgroup);
  stub.set(catalogGetTableDistVgInfo, __catalogGetTableDistVgInfo);
  // {
  //   AddrAny any("libcatalog.so");
  //   std::map<std::string,void*> result;
  //   any.get_global_func_addr_dynsym("^catalogGetHandle$", result);
  //   for (const auto& f : result) {
  //     stub.set(f.second, __catalogGetHandle);
  //   }
  // }
  // {
  //   AddrAny any("libcatalog.so");
  //   std::map<std::string,void*> result;
  //   any.get_global_func_addr_dynsym("^catalogGetTableMeta$", result);
  //   for (const auto& f : result) {
  //     stub.set(f.second, __catalogGetTableMeta);
  //   }
  // }
  // {
  //   AddrAny any("libcatalog.so");
  //   std::map<std::string,void*> result;
  //   any.get_global_func_addr_dynsym("^catalogGetTableHashVgroup$", result);
  //   for (const auto& f : result) {
  //     stub.set(f.second, __catalogGetTableHashVgroup);
  //   }
  // }
  // {
  //   AddrAny any("libcatalog.so");
  //   std::map<std::string,void*> result;
  //   any.get_global_func_addr_dynsym("^catalogGetTableDistVgInfo$", result);
  //   for (const auto& f : result) {
  //     stub.set(f.second, __catalogGetTableDistVgInfo);
  //   }
  // }
  // {
  //   AddrAny any("libcatalog.so");
  //   std::map<std::string,void*> result;
  //   any.get_global_func_addr_dynsym("^catalogGetDBVgVersion$", result);
  //   for (const auto& f : result) {
  //     stub.set(f.second, __catalogGetDBVgVersion);
  //   }
  // }
}

void generateMetaData() {
  generateTestT1(mockCatalogService.get());
  generateTestST1(mockCatalogService.get());
  mockCatalogService->showTables();
}

void destroyMetaDataEnv() {
  mockCatalogService.reset();
}

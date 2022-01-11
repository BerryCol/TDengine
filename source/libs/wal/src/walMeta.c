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

#include "cJSON.h"
#include "os.h"
#include "taoserror.h"
#include "tref.h"
#include "walInt.h"

#include <libgen.h>
#include <regex.h>

int64_t inline walGetFirstVer(SWal* pWal) { return pWal->vers.firstVer; }

int64_t inline walGetSnaphostVer(SWal* pWal) { return pWal->vers.snapshotVer; }

int64_t inline walGetLastVer(SWal* pWal) { return pWal->vers.lastVer; }

static inline int walBuildMetaName(SWal* pWal, int metaVer, char* buf) {
  return sprintf(buf, "%s/meta-ver%d", pWal->path, metaVer);
}

void* tmemmem(char* haystack, int hlen, char* needle, int nlen) {
  char* limit;

  if (nlen == 0 || hlen < nlen) {
    return false;
  }

  limit = haystack + hlen - nlen + 1;
  while ((haystack = (char*)memchr(
              haystack, needle[0], limit - haystack)) != NULL) {
    if (memcmp(haystack, needle, nlen) == 0) {
      return haystack;
    }
    haystack++;
  }
  return NULL;
}

static inline int64_t walScanLogGetLastVer(SWal* pWal) {
  ASSERT(pWal->fileInfoSet != NULL);
  int sz = taosArrayGetSize(pWal->fileInfoSet);
  ASSERT(sz > 0);
  for (int i = 0; i < sz; i++) {
    SWalFileInfo* pFileInfo = taosArrayGet(pWal->fileInfoSet, i);

  }
  SWalFileInfo *pLastFileInfo = taosArrayGet(pWal->fileInfoSet, sz-1);
  char fnameStr[WAL_FILE_LEN];
  walBuildLogName(pWal, pLastFileInfo->firstVer, fnameStr);

  struct stat statbuf;
  stat(fnameStr, &statbuf);
  int readSize = MIN(WAL_MAX_SIZE + 2, statbuf.st_size);

  FileFd fd = taosOpenFileRead(fnameStr);
  if (fd < 0) {
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  }

  uint64_t magic = WAL_MAGIC;

  char* buf = malloc(readSize + 5);
  if (buf == NULL) {
    taosCloseFile(fd);
    terrno = TSDB_CODE_WAL_OUT_OF_MEMORY;
    return -1;
  }

  taosLSeekFile(fd, -readSize, SEEK_END);
  if (readSize != taosReadFile(fd, buf, readSize)) {
    free(buf);
    taosCloseFile(fd);
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  }
  
  char* haystack = buf;
  char* found = NULL;
  char *candidate = NULL;
  while((candidate = tmemmem(haystack, readSize - (haystack - buf), (char*)&magic, sizeof(uint64_t))) != NULL) {
    // read and validate
    SWalHead *logContent = (SWalHead*)candidate;
    if (walValidHeadCksum(logContent) == 0 && walValidBodyCksum(logContent) == 0) {
      found = candidate;
    }
    haystack = candidate + 1;
  }
  if (found == buf) {
    SWalHead *logContent = (SWalHead*)found;
    if (walValidHeadCksum(logContent) != 0 || walValidBodyCksum(logContent) != 0) {
      // file has to be deleted
      free(buf);
      taosCloseFile(fd);
      terrno = TSDB_CODE_WAL_FILE_CORRUPTED;
      return -1;
    }
  }
  taosCloseFile(fd);
  SWalHead *lastEntry = (SWalHead*)found;

  return lastEntry->head.version;
}

int walCheckAndRepairMeta(SWal* pWal) {
  // load log files, get first/snapshot/last version info
  const char* logPattern = "^[0-9]+.log$";
  const char* idxPattern = "^[0-9]+.idx$";
  regex_t     logRegPattern;
  regex_t     idxRegPattern;
  SArray*     pLogInfoArray = taosArrayInit(8, sizeof(SWalFileInfo));

  regcomp(&logRegPattern, logPattern, REG_EXTENDED);
  regcomp(&idxRegPattern, idxPattern, REG_EXTENDED);

  DIR* dir = opendir(pWal->path);
  if (dir == NULL) {
    wError("vgId:%d, path:%s, failed to open since %s", pWal->cfg.vgId, pWal->path, strerror(errno));
    return -1;
  }

  // scan log files and build new meta
  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL) {
    char* name = basename(ent->d_name);
    int   code = regexec(&logRegPattern, name, 0, NULL, 0);
    if (code == 0) {
      SWalFileInfo fileInfo;
      memset(&fileInfo, -1, sizeof(SWalFileInfo));
      sscanf(name, "%" PRId64 ".log", &fileInfo.firstVer);
      FileFd fd = taosOpenFileRead(ent->d_name);
      //get lastVer
      //get size
      taosArrayPush(pLogInfoArray, &fileInfo);
    }
  }

  regfree(&logRegPattern);
  regfree(&idxRegPattern);

  taosArraySort(pLogInfoArray, compareWalFileInfo);
  int oldSz = 0;
  if (pWal->fileInfoSet) {
    oldSz = taosArrayGetSize(pWal->fileInfoSet);
  }
  int newSz = taosArrayGetSize(pLogInfoArray);
  // case 1. meta file not exist / cannot be parsed
  if (pWal->fileInfoSet == NULL && newSz != 0) {
    // recover fileInfo set
    pWal->fileInfoSet = pLogInfoArray;
    if (newSz != 0) {
    // recover meta version
      pWal->vers.firstVer = ((SWalFileInfo*)taosArrayGet(pLogInfoArray, 0))->firstVer;
      pWal->writeCur = newSz - 1;
    }
    // recover file size
  } else if (oldSz < newSz) {
    for (int i = oldSz; i < newSz; i++) {
      SWalFileInfo *pFileInfo = taosArrayGet(pLogInfoArray, i);
      taosArrayPush(pWal->fileInfoSet, pFileInfo);
    }
    pWal->writeCur = newSz - 1;
  }
  
  if (pWal->fileInfoSet && taosArrayGetSize(pWal->fileInfoSet) != 0) {
    pWal->vers.lastVer = walScanLogGetLastVer(pWal);
    ASSERT(pWal->vers.lastVer != -1);
  }

  // case 2. versions in meta not match log 
  //         or some log not included in meta
  // (e.g. program killed)
  //
  // case 3. other corrupt cases
  //
#if 0
  int sz = taosArrayGetSize(pLogInfoArray);
  for (int i = 0; i < sz; i++) {
    SWalFileInfo* pFileInfo = taosArrayGet(pLogInfoArray, i);
    if (i == 0 && pFileInfo->firstVer != walGetFirstVer(pWal)) {
      //repair
    }

    if (i > 0) {
      SWalFileInfo* pLastFileInfo = taosArrayGet(pLogInfoArray, i-1);
      if (pLastFileInfo->lastVer != pFileInfo->firstVer) {

      }
    }
  }
#endif

  int code = walSaveMeta(pWal);
  if (code < 0) {
    return -1;
  }

  // get last version of this file
  //
  // rebuild meta
  return 0;
}

int walCheckAndRepairIdx(SWal* pWal) {
  // iterate all idx files
  // check first and last entry of each idx file valid
  return 0;
}

int walRollFileInfo(SWal* pWal) {
  int64_t ts = taosGetTimestampSec();

  SArray* pArray = pWal->fileInfoSet;
  if (taosArrayGetSize(pArray) != 0) {
    SWalFileInfo* pInfo = taosArrayGetLast(pArray);
    pInfo->lastVer = pWal->vers.lastVer;
    pInfo->closeTs = ts;
  }

  // TODO: change to emplace back
  SWalFileInfo* pNewInfo = malloc(sizeof(SWalFileInfo));
  if (pNewInfo == NULL) {
    terrno = TSDB_CODE_WAL_OUT_OF_MEMORY;
    return -1;
  }
  pNewInfo->firstVer = pWal->vers.lastVer + 1;
  pNewInfo->lastVer = -1;
  pNewInfo->createTs = ts;
  pNewInfo->closeTs = -1;
  pNewInfo->fileSize = 0;
  taosArrayPush(pArray, pNewInfo);
  free(pNewInfo);
  return 0;
}

char* walMetaSerialize(SWal* pWal) {
  char buf[30];
  ASSERT(pWal->fileInfoSet);
  int    sz = pWal->fileInfoSet->size;
  cJSON* pRoot = cJSON_CreateObject();
  cJSON* pMeta = cJSON_CreateObject();
  cJSON* pFiles = cJSON_CreateArray();
  cJSON* pField;
  if (pRoot == NULL || pMeta == NULL || pFiles == NULL) {
    if(pRoot) {
      cJSON_Delete(pRoot);
    }
    if(pMeta) {
      cJSON_Delete(pMeta);
    }
    if(pFiles) {
      cJSON_Delete(pFiles);
    }
    terrno = TSDB_CODE_WAL_OUT_OF_MEMORY;
    return NULL;
  }
  cJSON_AddItemToObject(pRoot, "meta", pMeta);
  sprintf(buf, "%" PRId64, pWal->vers.firstVer);
  cJSON_AddStringToObject(pMeta, "firstVer", buf);
  sprintf(buf, "%" PRId64, pWal->vers.snapshotVer);
  cJSON_AddStringToObject(pMeta, "snapshotVer", buf);
  sprintf(buf, "%" PRId64, pWal->vers.commitVer);
  cJSON_AddStringToObject(pMeta, "commitVer", buf);
  sprintf(buf, "%" PRId64, pWal->vers.lastVer);
  cJSON_AddStringToObject(pMeta, "lastVer", buf);

  cJSON_AddItemToObject(pRoot, "files", pFiles);
  SWalFileInfo* pData = pWal->fileInfoSet->pData;
  for (int i = 0; i < sz; i++) {
    SWalFileInfo* pInfo = &pData[i];
    cJSON_AddItemToArray(pFiles, pField = cJSON_CreateObject());
    if (pField == NULL) {
      cJSON_Delete(pRoot);
      return NULL;
    }
    // cjson only support int32_t or double
    // string are used to prohibit the loss of precision
    sprintf(buf, "%" PRId64, pInfo->firstVer);
    cJSON_AddStringToObject(pField, "firstVer", buf);
    sprintf(buf, "%" PRId64, pInfo->lastVer);
    cJSON_AddStringToObject(pField, "lastVer", buf);
    sprintf(buf, "%" PRId64, pInfo->createTs);
    cJSON_AddStringToObject(pField, "createTs", buf);
    sprintf(buf, "%" PRId64, pInfo->closeTs);
    cJSON_AddStringToObject(pField, "closeTs", buf);
    sprintf(buf, "%" PRId64, pInfo->fileSize);
    cJSON_AddStringToObject(pField, "fileSize", buf);
  }
  char* serialized = cJSON_Print(pRoot);
  cJSON_Delete(pRoot);
  return serialized;
}

int walMetaDeserialize(SWal* pWal, const char* bytes) {
  ASSERT(taosArrayGetSize(pWal->fileInfoSet) == 0);
  cJSON *pRoot, *pMeta, *pFiles, *pInfoJson, *pField;
  pRoot = cJSON_Parse(bytes);
  pMeta = cJSON_GetObjectItem(pRoot, "meta");
  pField = cJSON_GetObjectItem(pMeta, "firstVer");
  pWal->vers.firstVer = atoll(cJSON_GetStringValue(pField));
  pField = cJSON_GetObjectItem(pMeta, "snapshotVer");
  pWal->vers.snapshotVer = atoll(cJSON_GetStringValue(pField));
  pField = cJSON_GetObjectItem(pMeta, "commitVer");
  pWal->vers.commitVer = atoll(cJSON_GetStringValue(pField));
  pField = cJSON_GetObjectItem(pMeta, "lastVer");
  pWal->vers.lastVer = atoll(cJSON_GetStringValue(pField));

  pFiles = cJSON_GetObjectItem(pRoot, "files");
  int sz = cJSON_GetArraySize(pFiles);
  // deserialize
  SArray* pArray = pWal->fileInfoSet;
  taosArrayEnsureCap(pArray, sz);
  SWalFileInfo* pData = pArray->pData;
  for (int i = 0; i < sz; i++) {
    cJSON*        pInfoJson = cJSON_GetArrayItem(pFiles, i);
    SWalFileInfo* pInfo = &pData[i];
    pField = cJSON_GetObjectItem(pInfoJson, "firstVer");
    pInfo->firstVer = atoll(cJSON_GetStringValue(pField));
    pField = cJSON_GetObjectItem(pInfoJson, "lastVer");
    pInfo->lastVer = atoll(cJSON_GetStringValue(pField));
    pField = cJSON_GetObjectItem(pInfoJson, "createTs");
    pInfo->createTs = atoll(cJSON_GetStringValue(pField));
    pField = cJSON_GetObjectItem(pInfoJson, "closeTs");
    pInfo->closeTs = atoll(cJSON_GetStringValue(pField));
    pField = cJSON_GetObjectItem(pInfoJson, "fileSize");
    pInfo->fileSize = atoll(cJSON_GetStringValue(pField));
  }
  taosArraySetSize(pArray, sz);
  pWal->fileInfoSet = pArray;
  pWal->writeCur = sz - 1;
  cJSON_Delete(pRoot);
  return 0;
}

static int walFindCurMetaVer(SWal* pWal) {
  const char* pattern = "^meta-ver[0-9]+$";
  regex_t     walMetaRegexPattern;
  regcomp(&walMetaRegexPattern, pattern, REG_EXTENDED);

  DIR* dir = opendir(pWal->path);
  if (dir == NULL) {
    wError("vgId:%d, path:%s, failed to open since %s", pWal->cfg.vgId, pWal->path, strerror(errno));
    return -1;
  }

  struct dirent* ent;

  // find existing meta-ver[x].json
  int metaVer = -1;
  while ((ent = readdir(dir)) != NULL) {
    char* name = basename(ent->d_name);
    int   code = regexec(&walMetaRegexPattern, name, 0, NULL, 0);
    if (code == 0) {
      sscanf(name, "meta-ver%d", &metaVer);
      break;
    }
  }
  closedir(dir);
  regfree(&walMetaRegexPattern);
  return metaVer;
}

int walSaveMeta(SWal* pWal) {
  int  metaVer = walFindCurMetaVer(pWal);
  char fnameStr[WAL_FILE_LEN];
  walBuildMetaName(pWal, metaVer + 1, fnameStr);
  FileFd metaFd = taosOpenFileCreateWrite(fnameStr);
  if (metaFd < 0) {
    return -1;
  }
  char* serialized = walMetaSerialize(pWal);
  int   len = strlen(serialized);
  if (len != taosWriteFile(metaFd, serialized, len)) {
    // TODO:clean file
    return -1;
  }

  taosCloseFile(metaFd);
  // delete old file
  if (metaVer > -1) {
    walBuildMetaName(pWal, metaVer, fnameStr);
    remove(fnameStr);
  }
  free(serialized);
  return 0;
}

int walLoadMeta(SWal* pWal) {
  ASSERT(pWal->fileInfoSet->size == 0);
  // find existing meta file
  int metaVer = walFindCurMetaVer(pWal);
  if (metaVer == -1) {
    return -1;
  }
  char fnameStr[WAL_FILE_LEN];
  walBuildMetaName(pWal, metaVer, fnameStr);
  // read metafile
  struct stat statbuf;
  stat(fnameStr, &statbuf);
  int   size = statbuf.st_size;
  char* buf = malloc(size + 5);
  if (buf == NULL) {
    terrno = TSDB_CODE_WAL_OUT_OF_MEMORY;
    return -1;
  }
  memset(buf, 0, size + 5);
  FileFd fd = taosOpenFileRead(fnameStr);
  if (taosReadFile(fd, buf, size) != size) {
    terrno = TAOS_SYSTEM_ERROR(errno);
    taosCloseFile(fd);
    free(buf);
    return -1;
  }
  // load into fileInfoSet
  int code = walMetaDeserialize(pWal, buf);
  taosCloseFile(fd);
  free(buf);
  return code;
}

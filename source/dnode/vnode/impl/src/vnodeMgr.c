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

#include "vnodeDef.h"

SVnodeMgr vnodeMgr = {.vnodeInitFlag = TD_MOD_UNINITIALIZED, .vnodeClearFlag = TD_MOD_UNCLEARD, .stop = false};

static void* loop(void* arg);

int vnodeInit(uint16_t nthreads) {
  if (TD_CHECK_AND_SET_MODE_INIT(&(vnodeMgr.vnodeInitFlag)) == TD_MOD_INITIALIZED) {
    return 0;
  }

  // Start commit handers
  if (nthreads > 0) {
    vnodeMgr.nthreads = nthreads;
    vnodeMgr.threads = (pthread_t*)calloc(nthreads, sizeof(pthread_t));
    if (vnodeMgr.threads == NULL) {
      return -1;
    }

    pthread_mutex_init(&(vnodeMgr.mutex), NULL);
    pthread_cond_init(&(vnodeMgr.hasTask), NULL);
    TD_DLIST_INIT(&(vnodeMgr.queue));

    for (uint16_t i = 0; i < nthreads; i++) {
      pthread_create(&(vnodeMgr.threads[i]), NULL, loop, NULL);
    }
  } else {
    // TODO: if no commit thread is set, then another mechanism should be
    // given. Otherwise, it is a false.
    ASSERT(0);
  }

  if (walInit() < 0) {
    return -1;
  }

  return 0;
}

void vnodeClear() {
  if (TD_CHECK_AND_SET_MOD_CLEAR(&(vnodeMgr.vnodeClearFlag)) == TD_MOD_CLEARD) {
    return;
  }

  walCleanUp();

  // Stop commit handler
  pthread_mutex_lock(&(vnodeMgr.mutex));
  vnodeMgr.stop = true;
  pthread_cond_broadcast(&(vnodeMgr.hasTask));
  pthread_mutex_unlock(&(vnodeMgr.mutex));

  for (uint16_t i = 0; i < vnodeMgr.nthreads; i++) {
    pthread_join(vnodeMgr.threads[i], NULL);
  }

  tfree(vnodeMgr.threads);
  pthread_cond_destroy(&(vnodeMgr.hasTask));
  pthread_mutex_destroy(&(vnodeMgr.mutex));
}

int vnodeScheduleTask(SVnodeTask* pTask) {
  pthread_mutex_lock(&(vnodeMgr.mutex));

  TD_DLIST_APPEND(&(vnodeMgr.queue), pTask);

  pthread_cond_signal(&(vnodeMgr.hasTask));

  pthread_mutex_unlock(&(vnodeMgr.mutex));

  return 0;
}

/* ------------------------ STATIC METHODS ------------------------ */
static void* loop(void* arg) {
  SVnodeTask* pTask;
  for (;;) {
    pthread_mutex_lock(&(vnodeMgr.mutex));
    for (;;) {
      pTask = TD_DLIST_HEAD(&(vnodeMgr.queue));
      if (pTask == NULL) {
        if (vnodeMgr.stop) {
          pthread_mutex_unlock(&(vnodeMgr.mutex));
          return NULL;
        } else {
          pthread_cond_wait(&(vnodeMgr.hasTask), &(vnodeMgr.mutex));
        }
      } else {
        TD_DLIST_POP(&(vnodeMgr.queue), pTask);
        break;
      }
    }

    pthread_mutex_unlock(&(vnodeMgr.mutex));

    (*(pTask->execute))(pTask->arg);
  }

  return NULL;
}
/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file wa_osa.c
 *
 * @brief This file contains main function that initialize OSA.
 */

/** @addtogroup WA_OSA
 *  @{
 */
#ifdef WA_DEBUG
#undef WA_DEBUG
#endif
/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <sys/prctl.h>
#include <sys/time.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_osa.h"
#include "wa_debug.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
#define WA_OSA_TASK_QUIT_SIGNAL SIGQUIT
/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WA_OSA_Q_NAME_PREFIX "/wa_q_hwst_" /* Fix for COLBO-67, DELIA-38417 */
#define WA_OSA_Q_NAME_SIZE (sizeof(WA_OSA_Q_NAME_PREFIX) + (sizeof(uint16_t)<<1)/* qCnt */)

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
}WA_OSA_cond_t;

typedef struct
{
    void *(*fnc) (void *);
    void *arg;
    pthread_t tid;
    void (*quitHandler)(void);
    bool quitFlag;
}WA_OSA_task_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void *TaskWrapper(void *p);
static void SigQuitHandler(int sig);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static bool osaInitialized = false;

static const int osaSchedulingPolicy[WA_OSA_SCHED_POLICY_MAX] =
{
        SCHED_OTHER,
        SCHED_RR
};

static pthread_key_t keyContext;
/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_OSA_Init(void)
{
    int status = -1;
    struct sigaction sa;

    WA_ENTER("WA_OSA_Init()\n");

    if(osaInitialized)
    {
        WA_ERROR("WA_OSA_Init(): OSA already initialized.\n");
        goto end;
    }

    status = pthread_key_create(&keyContext, NULL);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_Init(): pthread_key_create(): %d\n", status);
        goto end;
    }

    sa.sa_handler = SigQuitHandler;
    sigemptyset(&sa.sa_mask);
    status = sigaction(WA_OSA_TASK_QUIT_SIGNAL, &sa, NULL);
    if(status != 0)
    {
        pthread_key_delete(keyContext);
        WA_ERROR("WA_OSA_Init(): sigaction(): %d\n", status);
        goto end;
    }

    osaInitialized = true;
    end:
    WA_RETURN("WA_OSA_Init(): %d\n", 0);
    return 0;
}

int WA_OSA_Exit(void)
{
    int status = -1;

    WA_ENTER("WA_OSA_Exit()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_Exit(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_key_delete(keyContext);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_Exit(): pthread_key_delete(): %d\n", status);
        goto end;
    }

    osaInitialized = false;
    end:
    WA_RETURN("WA_OSA_Exit(): %d\n", 0);
    return 0;
}


int WA_OSA_TaskSetName(const char * const name)
{
    int status = -1;

    WA_ENTER("WA_OSA_TaskSetName(name=\"%s\")\n", name);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskSetName(): OSA not initialized.\n");
        goto end;
    }

    status = prctl(PR_SET_NAME, (unsigned long)(name), 0, 0, 0);
    if(status == -1)
    {
        WA_ERROR("WA_OSA_TaskSetName(): prctl(): unable to set name: %d\n", errno);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_TaskSetName(): %d\n", status);
    return status;
}

int  WA_OSA_TaskGetName(char * nameBuff)
{
    int status = -1;

    WA_ENTER("WA_OSA_TaskGetName()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskGetName(): OSA not initialized.\n");
        goto end;
    }

    if (nameBuff == NULL)
    {
        WA_ERROR("WA_OSA_TaskGetName(): buffer not valid\n");
        goto end;
    }

    status = prctl(PR_GET_NAME, (unsigned long) nameBuff, 0, 0, 0);
    if(status == -1)
    {
        WA_ERROR("WA_OSA_TaskGetName(): prctl(): unable to get name: %d\n", errno);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_TaskGetName(): %d\n", status);
    return status;
}

int WA_OSA_TaskSetSched(void *tHandle,
        const WA_OSA_schedPolicy_t alg,
        const int prio)
{
    WA_OSA_task_t *pThd = (WA_OSA_task_t *)tHandle;
    int status = -1;
    int prioMin = 0;
    int prioMax = 0;
    struct sched_param schedParams;

    WA_ENTER("WA_OSA_TaskSetSched(tHandle=%p, alg=%d, prio=%d)\n", tHandle, alg, prio);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskSetSched(): OSA not initialized.\n");
        goto end;
    }

#if !defined(_POSIX_PRIORITY_SCHEDULING)
    WA_ERROR("WA_OSA_TaskSetSched(): _POSIX_PRIORITY_SCHEDULING not supported!\n");
    goto end;
#endif

    prioMin = sched_get_priority_min(alg);
    if(prioMin == -1)
    {
        WA_ERROR("WA_OSA_TaskSetSched(): sched_get_priority_min(): errno: %d\n", errno);
        goto end;
    }

    prioMax = sched_get_priority_max(alg);
    if(prioMax == -1)
    {
        WA_ERROR("WA_OSA_TaskSetSched(): sched_get_priority_max(): errno: %d\n", errno);
        goto end;
    }

    /* map range */
    schedParams.sched_priority = prioMin + ((prioMax - prioMin) * prio)/WA_OSA_TASK_PRIORITY_MAX;
    status = pthread_setschedparam((pthread_t)(pThd->tid), osaSchedulingPolicy[alg], &schedParams);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_TaskSetSched(): pthread_setschedparam(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_TaskSetSched(): %d\n", status);
    return status;
}

void *WA_OSA_TaskCreate(const char * const name,
        unsigned int stackSize,
        void * (*fnc)(void *),
        void *fncArgs,
        WA_OSA_schedPolicy_t alg,
        int prio)
{
    int status;
    WA_OSA_task_t *pThd = NULL;

    (void)name;
    (void)stackSize;

    WA_ENTER("WA_OSA_TaskCreate(name=%s, stackSize=%d, fnc=%p, fncArgs=%p, alg=%d, prio=%d)\n",
            name, stackSize, fnc, fncArgs, alg, prio);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskCreate(): OSA not initialized.\n");
        goto end;
    }

    pThd = calloc(1, sizeof(WA_OSA_task_t));
    if(pThd == NULL)
    {
        WA_ERROR("WA_OSA_TaskCreate() calloc()");
        goto end;
    }
    pThd->fnc = fnc;
    pThd->arg = fncArgs;

    status = pthread_create((pthread_t *)(&(pThd->tid)), NULL, TaskWrapper, pThd);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_TaskCreate() pthread_create(): %d", status);
        free(pThd);
        pThd = NULL;
        goto end;
    }
    /* For normal scheduling the priorities are ignored */
#if(WA_OSA_SCHED_POLICY != WA_OSA_SCHED_POLICY_NORMAL)
    status = WA_OSA_TaskSetSched(pThd->tid, alg, prio);
    if(status != 0)
    {
        status = pthread_cancel((pthread_t)(pThd->tid));
        if(result != 0)
        {
            WA_ERROR("WA_OSA_TaskCreate() pthread_cancel(): %d\n", status);
        }
        status = pthread_join((pthread_t)(pThd->tid), retval);
        if(status != 0)
        {
            WA_ERROR("WA_OSA_TaskCreate() pthread_join(): %d\n", status);
        }
        free(pThd);
        pThd = NULL;
    }
#endif
    end:
    WA_RETURN("WA_OSA_TaskCreate(): %p\n", pThd);
    return pThd;
}


int WA_OSA_TaskJoin(void *tHandle, void **retval)
{
    WA_OSA_task_t *pThd = (WA_OSA_task_t *)tHandle;
    int status = -1;

    WA_ENTER("WA_OSA_TaskJoin(tHandle=%p)\n", tHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskJoin(): OSA not initialized.\n");
        goto end;
    }

    if (!pThd)
    {
        WA_ERROR("WA_OSA_TaskJoin(): pThd: %p\n", pThd);
        goto end;
    }

    status = pthread_join((pthread_t)(pThd->tid), retval);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_TaskJoin() pthread_join(): %d\n", status);
        goto end;
    }

    end:
    WA_RETURN("WA_OSA_TaskJoin(): %d\n", status);
    return status;
}

int WA_OSA_TaskDestroy(void *tHandle)
{
    int status = -1;

    WA_ENTER("WA_OSA_TaskDestroy(tHandle=%p)\n", tHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskDestroy(): OSA not initialized.\n");
        goto end;
    }

    if (tHandle)
    {
        free(tHandle);
    }
    status = 0;

    end:
    WA_RETURN("WA_OSA_TaskDestroy(): %d\n", status);
    return status;
}

int WA_OSA_TaskSetSignalHandler(void (*handler)(void))
{
    int status = -1, s1;
    sigset_t block, old;
    WA_OSA_task_t *pThd;

    WA_ENTER("WA_OSA_TaskSetSignalHandler()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskSetSignalHandler(): OSA not initialized.\n");
        goto end;
    }

    pThd = (WA_OSA_task_t *)pthread_getspecific(keyContext);
    if (pThd == NULL)
    {
        WA_ERROR("WA_OSA_TaskSetSignalHandler(): pthread_getspecific()\n");
        goto end;
    }

    sigemptyset(&block);
    sigaddset(&block, WA_OSA_TASK_QUIT_SIGNAL);
    status = pthread_sigmask(SIG_BLOCK, &block, &old);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_TaskSetSignalHandler() pthread_sigmask(): %d\n", status);
        goto end;
    }

    pThd->quitHandler = handler;

    s1 = pthread_sigmask(SIG_SETMASK, &old, NULL);
    if(s1 != 0)
    {
        WA_ERROR("WA_OSA_TaskSetSignalHandler() pthread_sigmask(): %d\n", s1);
    }

    end:
    WA_RETURN("WA_OSA_TaskSetSignalHandler(): %d\n", status);
    return status;
}

int WA_OSA_TaskSignalQuit(void *tHandle)
{
    WA_OSA_task_t *pThd = (WA_OSA_task_t *)tHandle;
    int status = -1;

    WA_ENTER("WA_OSA_TaskSignalQuit(tHandle=%p)\n", tHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskSignalQuit(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_kill((pthread_t)(pThd->tid), WA_OSA_TASK_QUIT_SIGNAL);
    if(status != 0)
    {
        if(status == 3)  /* DELIA-47944 : Process does not exist, hence ignoring the error */
        {
            status = 0;
            WA_DBG("WA_OSA_TaskSignalQuit() pthread_cancel(): %d\n", status);
        }
        else
        {
            WA_ERROR("WA_OSA_TaskSignalQuit() pthread_cancel(): %d\n", status);
        }
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_TaskSignalQuit(): %d\n", status);
    return status;
}

bool WA_OSA_TaskCheckQuit()
{
    WA_OSA_task_t *pThd = (WA_OSA_task_t *)pthread_getspecific(keyContext);
    return pThd && pThd->quitFlag;
}

int WA_OSA_TaskSleep(unsigned int ms)
{
    int status = -1;
    useconds_t delayUsec = ms*1000;

    WA_ENTER("WA_OSA_TaskSleep(ms=%d)\n", ms);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskSleep(): OSA not initialized.\n");
        goto end;
    }

    status = usleep(delayUsec);

    end:
    WA_RETURN("WA_OSA_TaskSleep(): %d\n", status);
    return status;
}


void *WA_OSA_TaskGet()
{
    void *pThd = NULL;

    WA_ENTER("WA_OSA_TaskGet()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_TaskGet(): OSA not initialized.\n");
        goto end;
    }

    pThd = pthread_getspecific(keyContext);

    end:
    WA_RETURN("WA_OSA_TaskGet(): %p\n", pThd);
    return pThd;
}


void *WA_OSA_MutexCreate()
{
    int status = 0;
    pthread_mutex_t *pMutex = NULL;

    WA_ENTER("WA_OSA_MutexCreate()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_MutexCreate(): OSA not initialized.\n");
        goto end;
    }

    pMutex = malloc(sizeof(pthread_mutex_t));

    status = pthread_mutex_init((pthread_mutex_t*)pMutex,
            (const pthread_mutexattr_t*)NULL);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_MutexCreate() pthread_mutex_init(): %d\n", status);
        if(pMutex != NULL)
        {
            free(pMutex);
        }
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_MutexCreate(): %p\n", pMutex);
    return (void *)pMutex;
}

int WA_OSA_MutexDestroy(void *mHandle)
{
    int status = -1;

    WA_ENTER("WA_OSA_MutexDestroy(mHandle=%p)\n", mHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_MutexDestroy(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_destroy((pthread_mutex_t *)mHandle);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_MutexDestroy() pthread_mutex_destroy(): %d\n", status);
        goto end;
    }

    free(mHandle);
    end:
    WA_RETURN("WA_OSA_MutexDestroy(): %d\n", status);
    return status;
}


int WA_OSA_MutexLock(void *mHandle)
{
    int status = -1;

    WA_ENTER("WA_OSA_MutexLock(mHandle=%p)\n", mHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_MutexLock(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_lock((pthread_mutex_t *)mHandle);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_MutexLock() pthread_mutex_lock(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_MutexLock(): %d\n", status);
    return status;
}


int WA_OSA_MutexUnlock(void *mHandle)
{
    int status = -1;

    WA_ENTER("WA_OSA_MutexUnlock(mHandle=%p)\n", mHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_MutexUnlock(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_unlock((pthread_mutex_t *)mHandle);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_MutexUnlock() pthread_mutex_unlock(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_MutexUnlock(): %d\n", status);
    return status;
}

void *WA_OSA_SemCreate(int val)
{
    int status;
    sem_t *pSem = NULL;

    WA_ENTER("WA_OSA_SemCreate(val=%d)\n", val);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_SemCreate(): OSA not initialized.\n");
        goto end;
    }

    pSem = malloc(sizeof(sem_t));

    status = sem_init(pSem, 0, val);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_SemCreate() sem_init(): %d\n", status);
        goto err_sem;
    }

    /* */
    goto end;

    err_sem:
    if(pSem != NULL)
    {
        free(pSem);
    }
    end:
    WA_RETURN("WA_OSA_SemCreate(): %p\n", pSem);
    return (void *)pSem;
}

int WA_OSA_SemDestroy(void *handle)
{
    int status = -1;
    sem_t *pSem = (sem_t *)handle;

    WA_ENTER("WA_OSA_SemDestroy(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_SemDestroy(): OSA not initialized.\n");
        goto end;
    }

    status = sem_destroy(pSem);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_SemDestroy() sem_destroy(): %d\n", status);
        goto end;
    }

    free(handle);
    end:
    WA_RETURN("WA_OSA_SemDestroy(): %d\n", status);
    return status;
}


int WA_OSA_SemWait(void *handle)
{
    int status = -1;
    sem_t *pSem = (sem_t *)handle;

    WA_ENTER("WA_OSA_SemWait(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_SemWait(): OSA not initialized.\n");
        goto end;
    }

    status = sem_wait(pSem);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_SemWait() sem_wait(): %d\n", status);
    }

    end:
    WA_RETURN("WA_OSA_SemWait(): %d\n", status);
    return status;
}


int WA_OSA_SemTimedWait(void *handle, unsigned int ms)
{
    int status = -1;
    sem_t *pSem = (sem_t *)handle;
    struct timespec absTime;
    struct timeval timeOfDay;

    WA_ENTER("WA_OSA_SemTimedWait(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_SemTimedWait(): OSA not initialized.\n");
        goto end;
    }

    gettimeofday(&timeOfDay, NULL);

    absTime.tv_nsec = timeOfDay.tv_usec * 1000 + (ms % 1000) * 1000000;
    absTime.tv_sec  = timeOfDay.tv_sec + (ms / 1000) + (absTime.tv_nsec / 1000000000);
    absTime.tv_nsec %= 1000000000;

    status = sem_timedwait(pSem, &absTime);
    if(status == ETIMEDOUT)
    {
        WA_ERROR("WA_OSA_SemTimedWait() sem_timedwait(): timeout\n");
        status = 1;
    }
    else if(status != 0)
    {
        WA_ERROR("WA_OSA_SemTimedWait(): sem_timedwait(): %d\n", status);
    }

    end:
    WA_RETURN("WA_OSA_SemTimedWait(): %d\n", status);
    return status;
}


int WA_OSA_SemSignal(void *handle)
{
    int status = -1;
    sem_t *pSem = (sem_t *)handle;

    WA_ENTER("WA_OSA_SemSignal(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_SemSignal(): OSA not initialized.\n");
        goto end;
    }

    status = sem_post(pSem);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_SemSignal() sem_post(): %d\n", status);
    }

    end:
    WA_RETURN("WA_OSA_SemSignal(): %d\n", status);
    return status;
}

void *WA_OSA_CondCreate()
{
    int status = 0,s1;
    WA_OSA_cond_t *pCond = NULL;

    WA_ENTER("WA_OSA_CondCreate()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondCreate(): OSA not initialized.\n");
        goto end;
    }

    pCond = malloc(sizeof(WA_OSA_cond_t));

    status = pthread_cond_init(&(pCond->cond), (const pthread_condattr_t*)NULL);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondCreate() pthread_cond_init(): %d\n", status);
        goto err_cond;
    }
    status = pthread_mutex_init(&(pCond->mutex), (const pthread_mutexattr_t*)NULL);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondCreate() pthread_mutex_init(): %d\n", status);
        goto err_mutex;
    }

    /* */
    goto end;

    err_mutex:
    s1 = pthread_cond_destroy(&(pCond->cond));
    if(s1 != 0)
    {
        WA_ERROR("WA_OSA_CondCreate() pthread_cond_destroy(): %d\n", s1);
    }
    err_cond:
    if(pCond != NULL)
    {
        free(pCond);
    }
    end:
    WA_RETURN("WA_OSA_CondCreate(): %p\n", pCond);
    return (void *)pCond;
}

int WA_OSA_CondDestroy(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondDestroy(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondDestroy(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_destroy(&(pCond->mutex));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondDestroy() pthread_mutex_destroy(): %d\n", status);
        goto end;
    }

    status = pthread_cond_destroy(&(pCond->cond));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondDestroy() pthread_cond_destroy(): %d\n", status);
        goto end;
    }

    free(handle);
    end:
    WA_RETURN("WA_OSA_CondDestroy(): %d\n", status);
    return status;
}


int WA_OSA_CondWait(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondWait(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondWait(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_cond_wait(&(pCond->cond), &(pCond->mutex));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondWait() pthread_cond_wait(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_CondWait(): %d\n", status);
    return status;
}

int WA_OSA_CondTimedWait(void *handle, unsigned int ms)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;
    struct timespec absTime;
    struct timeval  timeOfDay;

    WA_ENTER("WA_OSA_CondTimedWait(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondTimedWait(): OSA not initialized.\n");
        goto end;
    }

    gettimeofday(&timeOfDay, NULL);

    absTime.tv_nsec = timeOfDay.tv_usec * 1000 + (ms % 1000) * 1000000;
    absTime.tv_sec  = timeOfDay.tv_sec + (ms / 1000) + (absTime.tv_nsec / 1000000000);
    absTime.tv_nsec %= 1000000000;

    status = pthread_cond_timedwait(&(pCond->cond), &(pCond->mutex), &absTime);
    if(status == ETIMEDOUT)
    {
        WA_ERROR("WA_OSA_CondTimedWait() pthread_cond_timedwait(): timeout\n");
        status = 1;
        goto end;
    }
    else if(status != 0)
    {
        status = -1;
        WA_ERROR("WA_OSA_CondTimedWait() pthread_cond_wait(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_CondTimedWait(): %d\n", status);
    return status;
}

int WA_OSA_CondSignal(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondSignal(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondSignal(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_cond_signal(&(pCond->cond));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondSignal() pthread_cond_signal(): %d\n", status);
        goto end;
    }

    end:
    WA_RETURN("WA_OSA_SemSignal(): %d\n", status);
    return status;
}

int WA_OSA_CondSignalBroadcast(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondSignalBroadcast(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondSignalBroadcast(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_cond_broadcast(&(pCond->cond));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondSignalBroadcast() pthread_cond_broadcast(): %d\n", status);
        goto end;
    }

    end:
    WA_RETURN("WA_OSA_CondSignalBroadcast(): %d\n", status);
    return status;
}

int WA_OSA_CondLock(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondLock(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondLock(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_lock(&(pCond->mutex));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondLock() pthread_mutex_lock(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_CondLock(): %d\n", status);
    return status;
}

int WA_OSA_CondUnlock(void *handle)
{
    int status = -1;
    WA_OSA_cond_t *pCond = (WA_OSA_cond_t *)handle;

    WA_ENTER("WA_OSA_CondUnlock(handle=%p)\n", handle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_CondUnlock(): OSA not initialized.\n");
        goto end;
    }

    status = pthread_mutex_unlock(&(pCond->mutex));
    if(status != 0)
    {
        WA_ERROR("WA_OSA_CondUnlock() pthread_mutex_unlock(): %d\n", status);
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_CondUnlock(): %d\n", status);
    return status;
}

void *WA_OSA_Malloc(size_t size)
{
    return (void *)malloc(size);
}


void WA_OSA_Free(void * const ptr)
{
    free((void *)ptr);
}


uint16_t WA_OSA_AppId()
{
    uint32_t appId;

    WA_ENTER("WA_OSA_AppId()\n");

    appId = (uint16_t)getpid();

    WA_RETURN("WA_OSA_AppId(): %d(0x%04x)\n", appId, appId);

    return appId;
}


void *WA_OSA_QCreate(const unsigned int deep, long maxSize)
{
    /* Unique name for each queue */
    static uint16_t qCnt = 0;
    char qName[WA_OSA_Q_NAME_SIZE];
    mqd_t qHandle = 0;
    struct mq_attr qAttrs;

    WA_ENTER("WA_OSA_QCreate()\n");

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QCreate(): OSA not initialized.\n");
        goto end;
    }

    (void)snprintf(qName, WA_OSA_Q_NAME_SIZE, "%s%04x", WA_OSA_Q_NAME_PREFIX, qCnt++);
    qName[WA_OSA_Q_NAME_SIZE-1]='\0';

    WA_INFO("WA_OSA_QCreate(): name:\"%s\"\n", qName);

    qAttrs.mq_maxmsg = (long)deep;
    qAttrs.mq_msgsize = maxSize;
    qAttrs.mq_flags = O_RDWR;

    qHandle = mq_open(qName, O_RDWR | O_CREAT, 0666, &qAttrs);
    if(qHandle == (mqd_t)-1)
    {
        qHandle = 0;
        WA_ERROR("WA_OSA_QCreate(): mq_open(): unable to create queue: %d\n", errno);
        if(errno == EINVAL)
        {
            WA_ERROR("WA_OSA_QCreate(): Check the mq_maxmsg against /proc/sys/fs/mqueue/msg_max\n");
        }
        goto end;
    }
    end:
    WA_RETURN("WA_OSA_QCreate(): %p\n", (void *)(uintptr_t)qHandle);
    return (void *)(uintptr_t)qHandle;
}

/* unlink not implemented */
int WA_OSA_QDestroy(void * const qHandle)
{
    int status = -1;

    WA_ENTER("WA_OSA_QDestroy(qHandle=%p)\n", qHandle);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QDestroy(): OSA not initialized.\n");
        goto end;
    }

    status = mq_close((mqd_t)(uintptr_t)qHandle);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_QDestroy(): mq_close(): unable to close queue: %d\n", errno);
    }
    end:
    WA_RETURN("WA_OSA_QDestroy(): %d\n", status);
    return status;
}

int WA_OSA_QSend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio)
{
    int status = -1;

    WA_ENTER("WA_OSA_QSend(qHandle=%p, pMsg=%p, size=%zu, prio=%d)\n",
            qHandle, pMsg, size, prio);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QSend(): OSA not initialized.\n");
        goto end;
    }

    status = mq_send((mqd_t)(uintptr_t)qHandle, (const char *)pMsg, size, prio);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_QSend(): mq_send(): unable to send: %d\n", errno);
    }
    end:
    WA_RETURN("WA_OSA_QSend(): %d\n", status);
    return status;
}

int WA_OSA_QTimedSend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio,
        unsigned int ms)
{
    int status = -1;
    struct timespec absTime;
    struct timeval  timeOfDay;

    WA_ENTER("WA_OSA_QTimedSend(qHandle=%p, pMsg=%p, size=%zu, prio=%d ms=%d)\n",
            qHandle, pMsg, size, prio,ms);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QTimedSend(): OSA not initialized.\n");
        goto end;
    }

    gettimeofday(&timeOfDay, NULL);

    absTime.tv_nsec = timeOfDay.tv_usec * 1000 + (ms % 1000) * 1000000;
    absTime.tv_sec  = timeOfDay.tv_sec + (ms / 1000) + (absTime.tv_nsec / 1000000000);
    absTime.tv_nsec %= 1000000000;

    status = mq_timedsend((mqd_t)(uintptr_t)qHandle, (const char *)pMsg, size, prio, &absTime);
    if(status != 0)
    {
        WA_ERROR("WA_OSA_QTimedSend(): mq_send(): unable to send: %d\n", errno);
        if(errno == ETIMEDOUT)
        {
            status = 1;
        }
    }
    end:
    WA_RETURN("WA_OSA_QTimedSend(): %d\n", status);
    return status;
}

int WA_OSA_QTimedRetrySend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio,
        unsigned int ms,
        unsigned int retries)
{
    int status = -1;

    WA_ENTER("WA_OSA_QTimedRetrySend(qHandle=%p, pMsg=%p, size=%zu, prio=%d ms=%d, retries=%d)\n",
            qHandle, pMsg, size, prio,ms,retries);

    do
    {
        status = WA_OSA_QTimedSend(qHandle, pMsg, size, prio, ms);
        if(status == 1)
        {
            WA_INFO("WA_OSA_QTimedRetrySend(): WA_OSA_QTimedSend(): timeout, retries: %d\n", retries);
            continue;
        }
    }while(status && retries--);

    WA_RETURN("WA_OSA_QTimedRetrySend(): %d\n", status);
    return status;
}

ssize_t WA_OSA_QReceive(void * const qHandle,
        char * const pMsg,
        long maxSize,
        unsigned int * const pPrio)
{
    unsigned int prio;
    ssize_t rsize = -1;

    WA_ENTER("WA_OSA_QReceive(qHandle=%p, pMsg=%p, pPrio=%p)\n",
            qHandle, pMsg, pPrio);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QReceive(): OSA not initialized.\n");
        goto end;
    }

    rsize = mq_receive((mqd_t)(uintptr_t)qHandle,
            (char *)pMsg,
            maxSize,
            &prio);
    if(rsize == -1)
    {
        WA_ERROR("WA_OSA_QReceive(): mq_receive(): unable to receive: %d\n", errno);
        goto end;
    }
    if(pPrio != NULL)
    {
        *pPrio = prio;
    }
    end:
    WA_RETURN("WA_OSA_QReceive(prio=%d): %zd\n", prio, rsize);
    return rsize;
}


ssize_t WA_OSA_QTimedReceive(void * const qHandle,
        char * const pMsg,
        long maxSize,
        unsigned int * const pPrio,
        unsigned int ms)
{
    unsigned int prio;
    ssize_t rsize = -1;
    struct timespec absTime;
    struct timeval  timeOfDay;

    WA_ENTER("WA_OSA_QTimedReceive(qHandle=%p, pMsg=%p, pPrio=%p, ms=%u)\n",
            qHandle, pMsg, pPrio, ms);

    if(!osaInitialized)
    {
        WA_ERROR("WA_OSA_QTimedReceive(): OSA not initialized.\n");
        goto end;
    }

    gettimeofday(&timeOfDay, NULL);

    absTime.tv_nsec = timeOfDay.tv_usec * 1000 + (ms % 1000) * 1000000;
    absTime.tv_sec  = timeOfDay.tv_sec + (ms / 1000) + (absTime.tv_nsec / 1000000000);
    absTime.tv_nsec %= 1000000000;

    rsize = mq_timedreceive((mqd_t)(uintptr_t)qHandle,
            (char *)pMsg,
            maxSize,
            &prio,
            &absTime);
    if(rsize == -1)
    {
        WA_ERROR("WA_OSA_QTimedReceive(): mq_receive(): unable to receive: %d\n", errno);
        if(errno == ETIMEDOUT)
        {
            rsize = -2;
        }
        goto end;
    }
    if(pPrio != NULL)
    {
        *pPrio = prio;
    }
    end:
    WA_RETURN("WA_OSA_QTimedReceive(prio=%u): %zd\n", prio, rsize);
    return rsize;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
static void *TaskWrapper(void *p)
{
    WA_OSA_task_t *pThd = (WA_OSA_task_t *)p;
    int status;
    void *result = NULL;

    WA_ENTER("TaskWrapper(p=%p)\n", p);

    status = pthread_setspecific(keyContext, (void *)pThd);
    if(status != 0)
    {
        WA_ERROR("TaskWrapper() pthread_setspecific(): %d\n", status);
        goto end;
    }
    result = pThd->fnc(pThd->arg);

    end:
    WA_RETURN("TaskWrapper(): %p\n", result);
    return result;
}

static void SigQuitHandler(int sig)
{
    WA_OSA_task_t *pThd;

    pThd = (WA_OSA_task_t *)pthread_getspecific(keyContext);
    if (pThd)
    {
        pThd->quitFlag = true;
        if (pThd->quitHandler)
        {
            pThd->quitHandler();
        }
    }
}

/* End of doxygen group */
/*! @} */

/* EOF */

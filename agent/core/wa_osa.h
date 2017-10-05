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
 * @file wa_osa.h
 */

/** @addtogroup WA_OSA
 *  @{
 */

#ifndef WA_OSA_H
#define WA_OSA_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
/** Max size of the message queue entry. */
#define WA_OSA_Q_MAX_MSG_SIZE 256

/** Max task priority. */
#define WA_OSA_TASK_PRIORITY_MAX 100

/** Max task priority. */
#define WA_OSA_Q_PRIORITY_MAX 100

/** Number of default retries when sending on queue. */
#define WA_OSA_Q_SEND_RETRY_DEFAULT 2

/** Default timeout when sending on queue. */
#define WA_OSA_Q_SEND_TIMEOUT_DEFAULT 2

/** Max task name length (including \0)*/
#define WA_OSA_TASK_NAME_MAX_LEN 100

#define WA_OSA_TASK_QUIT_FLAG
/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

typedef struct {
    uint32_t from;
    uint32_t to;
    json_t *json;
}WA_OSA_Qjmsg_t;

typedef enum
{
    WA_OSA_SCHED_POLICY_NORMAL = 0,
    WA_OSA_SCHED_POLICY_RT,
    WA_OSA_SCHED_POLICY_MAX
}WA_OSA_schedPolicy_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * This function is called as a first thing during system
 * initialisation. It is responsible for initialising
 * the OSA internal data structures. No other call should
 * be made to any other OSA function until this function
 * returns 0. After WA_OSA_Init() returns 0,
 * repeated calls to WA_OSA_Init() should
 * also return 0.
 *
 * @brief Initialises the OSA.
 *
 * @retval 0  The OSA was successfully initialised.
 * @retval -1 The OSA initialisation failed.
 */
extern int WA_OSA_Init(void);


/**
 * This function must be called during system uninitialisation.
 * It is responsible for the uninitialisation of the OSA
 * internal data structures. No other call should be made to
 * any other OSA function after this function returns 0.
 * After WA_OSA_Exit() returns 0,
 * repeated calls to STE_OSA_Exit() should also return 0.
 *
 * @brief This function un-initialises the OSA.
 *
 * @retval 0  The OSA was successfully uninitialised.
 * @retval -1 The OSA un-initialisation failed.
 */
extern int WA_OSA_Exit(void);

/**
 * @brief Change current task name
 *
 * @param name valid task name
 *
 * @retval 0  success
 * @retval -1 error
 */
extern int WA_OSA_TaskSetName(const char * const name);


/**
 * @brief Changes task scheduling algorithm(policy) and priority
 *
 * @param tHandle valid task handle
 * @param alg valid task scheduling policy
 * @param prio valid (0 .. \c WA_OSA_TASK_PRIORITY_MAX) task priority
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_TaskSetSched(void *tHandle,
        const WA_OSA_schedPolicy_t alg,
        const int prio);

/**
 * @brief Create task
 *
 * @param name valid task name
 * @param stackSize valid task stack size
 * @param fnc valid pointer to task function
 * @param fncArgs task function arguments
 * @param alg valid task scheduling policy
 * @param prio valid (0 .. \c WA_OSA_TASK_PRIORITY_MAX)task priority
 *
 * @returns handle to the created task
 * @retval null on error
 */
extern void *WA_OSA_TaskCreate(const char * const name,
        unsigned int stackSize,
        void * (*fnc)(void *),
        void *fncArgs,
        WA_OSA_schedPolicy_t alg,
        int prio);

/**
 * @brief Join task
 *
 * @param tHandle valid handle to the task
 * @param retval pointer where the task return status is returned (might be null)
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_TaskJoin(void *tHandle, void **retval);

/**
 * @brief Destroy task resources
 *
 * @param tHandle valid handle to the task
 *
 * @retval 0 success
 * @retval !=0 error
 *
 * @note Must be called on inactive task, after \c WA_OSA_TaskJoin()
 */
extern int WA_OSA_TaskDestroy(void *tHandle);

/**
 * @brief Sleep a task
 *
 * @param ms sleep time in [ms]
 *
 * @retval 0 success
 * @retval -1 error
 */
extern int WA_OSA_TaskSleep(unsigned int ms);

/**
 * @brief Signal a task to break any pending actions
 *
 * @param tHandle task to signal
 *
 * @retval 0 success
 * @retval -1 error
 *
 * @note Any marker (e.g. in form of a bool flag) must be used to check
 *       if the task should quit.
 *       ToDo: hide the flag parameter, add blocking/non blocking
 *
 *
 */
extern int WA_OSA_TaskSignalQuit(void *tHandle);

/**
 * @brief Associate task handler function.
 *
 * @param handler A handler function that will be called upon signal arrival.
 *
 * @retval 0 success
 * @retval -1 error
 */
extern int WA_OSA_TaskSetSignalHandler(void (*handler)(void));

/**
 * @brief Check if a quit request was issued
 *
 * @retval false no quit request
 * @retval true quit request issued
 *
 * @note Function must be called from within a task
 *       created by \c WA_OSA_TaskCreate()
 *
 */
extern bool WA_OSA_TaskCheckQuit();

/**
 * @brief Get current task handle
 *
 * @returns handle to the current task
 */
extern void *WA_OSA_TaskGet();

/**
 * @brief Create mutex
 *
 * @returns handle to the created mutex
 * @retval null on error
 */
extern void *WA_OSA_MutexCreate();

/**
 * @brief Destroy mutex
 *
 * @param mHanlde valid mutex handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_MutexDestroy(void *mHanlde);

/**
 * @brief Lock mutex
 *
 * @param mHanlde valid mutex handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_MutexLock(void *mHanlde);

/**
 * @brief Unlock mutex
 *
 * @param mHanlde valid mutex handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_MutexUnlock(void *mHanlde);

/**
 * @brief Create semaphore
 *
 * @param val start value of the semaphore (0-will block, 1-can be taken)
 *
 * @returns handle to the created semaphore
 * @retval null on error
 */
extern void *WA_OSA_SemCreate(int val);

/**
 * @brief Destroy semaphore
 *
 * @param hanlde valid semaphore handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_SemDestroy(void *hanlde);

/**
 * @brief Wait semaphore
 *
 * @param hanlde valid semaphore handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_SemWait(void *hanlde);

/**
 * @brief Signal semaphore
 *
 * @param hanlde valid semaphore handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_SemSignal(void *hanlde);

/**
 * @brief Create conditional variable
 *
 * @returns handle to the created conditional variable
 * @retval null on error
 */
extern void *WA_OSA_CondCreate();

/**
 * @brief Destroy conditional variable
 *
 * @param mHanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondDestroy(void *handle);

/**
 * @brief Wait conditional variable
 *
 * @param hanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondWait(void *handle);

/**
 * @brief Wait conditional variable
 *
 * @param hanlde valid conditional variable handle
 * @param ms wait timeout in [ms]
 *
 * @retval 0 success
 * @retval 1 timeout
 * @retval -1 error
 */
extern int WA_OSA_CondTimedWait(void *handle, unsigned int ms);

/**
 * @brief Signal conditional variable
 *
 * @param hanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondSignal(void *handle);

/**
 * @brief Signal conditional variable on all waiters
 *
 * @param hanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondSignalBroadcast(void *handle);

/**
 * @brief Lock conditional variable mutex
 *
 * @param hanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondLock(void *handle);

/**
 * @brief Unlock conditional variable mutex
 *
 * @param hanlde valid conditional variable handle
 *
 * @retval 0 success
 * @retval !=0 error
 */
extern int WA_OSA_CondUnlock(void *handle);

/**
 * @brief Allocate memory.
 *
 * @size size to allocate
 *
 * @returns pointer to allocated space
 */
extern void *WA_OSA_Malloc(size_t size);

/**
 * @brief Free memory.
 *
 * @ptr valid pointer to the memory
 */
extern void WA_OSA_Free(void * const ptr);

/**
 * @brief Creates a priority queue.
 *
 * @param deep a queue deep levels
 * @param maxSize max message size
 *
 * @returns handle to the created queue
 * @retval null on error
 */
extern void * WA_OSA_QCreate(const unsigned int deep, long maxSize);

/**
 * @brief Destroys queue.
 *
 * @param qHandle valid queue handle
 *
 * @retval 0 success
 * @retval -1 error
 */
extern int WA_OSA_QDestroy(void * const qHandle);

/**
 * @brief Sends a message over queue.
 *
 * @param qHandle valid queue handle
 * @param pMsg valid pointer to the message
 * @param size message size
 * @param prio message priority (higher number is higher priority, received faster)
 *
 * @retval 0 success
 * @retval -1 error
 */
extern int WA_OSA_QSend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio);

/**
 * @brief Sends a message over queue with timeout on full queue.
 *
 * @param qHandle valid queue handle
 * @param pMsg valid pointer to the message
 * @param size message size
 * @param prio message priority (higher number is higher priority, received faster)
 * @param ms try timeout in [ms]
 *
 * @retval 0 success
 * @retval 1 timeout
 * @retval -1 error
 */
extern int WA_OSA_QTimedSend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio,
        unsigned int ms);

/**
 * @brief Sends a message over queue with timeout on full queue, and repetitions if timeout.
 *
 * @param qHandle valid queue handle
 * @param pMsg valid pointer to the message
 * @param size message size
 * @param prio message priority (higher number is higher priority, received faster)
 * @param ms try timeout in [ms]
 * @param retries number of retries
 *
 * @retval 0 success
 * @retval 1 timeout
 * @retval -1 error
 */
extern int WA_OSA_QTimedRetrySend(void * const qHandle,
        const char * const pMsg,
        const size_t size,
        const unsigned int prio,
        unsigned int ms,
        unsigned int retries);

/**
 * @brief Receives a message from queue.
 *
 * @param qHandle valid queue handle
 * @param pMsg valid pointer to preallocated buffer for the received message
 * @param maxSize max message size
 * @param prio pointer where the priority will be stored, might be null
 *
 * @returns received message size
 * @retval -1 error
 */
extern ssize_t WA_OSA_QReceive(void * const qHandle,
        char * const pMsg,
        long maxSize,
        unsigned int * const pPrio);

/**
 * @brief Receives a message from queue with timeout.
 *
 * @param qHandle valid queue handle
 * @param pMsg valid pointer to preallocated buffer for the received message
 * @param maxSize max message size
 * @param prio pointer where the priority will be stored, might be null
 * @param ms wait timeout in [ms]
 *
 * @returns received message size
 * @retval -1 error
 * @retval -2 timeout
 */
extern ssize_t WA_OSA_QTimedReceive(void * const qHandle,
        char * const pMsg,
        long maxSize,
        unsigned int * const pPrio,
        unsigned int ms);


#ifdef __cplusplus
}
#endif

#endif /* _WA_OSA_H_ */

/* End of doxygen group */
/*! @} */

/* EOF */


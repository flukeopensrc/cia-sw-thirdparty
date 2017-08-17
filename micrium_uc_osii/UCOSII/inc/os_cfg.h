/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                  uC/OS-II Configuration File for V2.8x
*
*                               (c) Copyright 2005-2007, Micrium, Weston, FL
*                                          All Rights Reserved
*
*
* File    : OS_CFG.H
* By      : Jean J. Labrosse
* Version : V2.86
*
* LICENSING TERMS:
* ---------------
*   uC/OS-II is provided in source form for FREE evaluation, for educational use or for peaceful research.
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micrim to properly license
* its use in your product. We provide ALL the source code for your convenience and to help you experience
* uC/OS-II.   The fact that the  source is provided does  NOT  mean that you can use it without  paying a
* licensing fee.
*********************************************************************************************************
*/

#ifndef OS_CFG_H
#define OS_CFG_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef __ASSEMBLER__ 
#include "sys/alt_alarm.h"
#endif /* __ASSEMBLER__ */


/*
 * The following uC/OS configuration options have not been defined as
 * configuration options in Nios II IDE or BSP tools; therefore they are
 * defined here rather than in the generated system.h content. As long
 * as they appear here, you may change their values in this file to change
 * the setting
 */
                                       /* ---------------------- MISCELLANEOUS ----------------------- */
#define OS_APP_HOOKS_EN           1u   /* Application-defined hooks are called from the uC/OS-II hooks */
#define OS_EVENT_MULTI_EN         1u   /* Include code for OSEventPendMulti()                          */
#define OS_EVENT_NAME_EN          1u   /* Enable names for Sem, Mutex, Mbox and Q                      */
#define OS_FLAG_NAME_EN           1u   /*     Enable names for event flag group                        */
#define OS_TASK_NAME_EN           1u   /*     Enable task names                                        */
#define OS_TASK_REG_TBL_SIZE      1u   /*     Size of task variables array (#of INT32U entries)        */

                                       /* -------------------- MESSAGE MAILBOXES --------------------- */
#define OS_MBOX_PEND_ABORT_EN     1u   /*     Include code for OSMboxPendAbort()                       */

                                       /* ---------------------- MESSAGE QUEUES ---------------------- */
#define OS_Q_PEND_ABORT_EN        1u   /*     Include code for OSQPendAbort()                          */

                                       /* ------------------------ SEMAPHORES ------------------------ */
#define OS_SEM_PEND_ABORT_EN      1u   /*    Include code for OSSemPendAbort()                         */

                                       /* --------------------- MEMORY MANAGEMENT -------------------- */
#define OS_MEM_NAME_EN            1u   /*     Enable memory partition names                            */                                                                                                                 
                                       /* --------------------- TIMER MANAGEMENT --------------------- */
#define OS_TMR_CFG_NAME_EN        0u   /*     Determine timer names                                    */

#include "system.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OS_CFG_H__ */

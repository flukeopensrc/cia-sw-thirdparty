#
# micrium_ucosii_sw.tcl -- A description of MicroC/OS-II RTOS for
#                          Altera Nios II BSP tools
#

# Create a new operating system called "ucosii"
create_os UCOSII

# Set UI display name
set_sw_property display_name "Micrium MicroC/OS-II"

# This OS "extends" HAL BSP type
set_sw_property extends_bsp_type HAL

# The version of this software
# Note: this reflects the version of the Altera release that this file
#       shipped in. The MicroC/OS-II version is 2.86
set_sw_property version 17.0

# Location in generated BSP that above sources will be copied into
set_sw_property bsp_subdirectory UCOSII

# Interrupt properties: This OS supports ISR preemption
# (in its interrupt entry/exit code registered with
# macros in alt_hooks.h).
set_sw_property isr_preemption_supported true
  
#
# Source file listings...
#

# C source files
add_sw_property c_source UCOSII/src/alt_env_lock.c
add_sw_property c_source UCOSII/src/alt_malloc_lock.c
add_sw_property c_source UCOSII/src/os_core.c
add_sw_property c_source UCOSII/src/os_dbg_r.c
add_sw_property c_source UCOSII/src/os_flag.c
add_sw_property c_source UCOSII/src/os_mbox.c
add_sw_property c_source UCOSII/src/os_mem.c
add_sw_property c_source UCOSII/src/os_mutex.c
add_sw_property c_source UCOSII/src/os_q.c
add_sw_property c_source UCOSII/src/os_sem.c
add_sw_property c_source UCOSII/src/os_task.c
add_sw_property c_source UCOSII/src/os_time.c
add_sw_property c_source UCOSII/src/os_tmr.c
add_sw_property c_source UCOSII/src/cpu_c.c
add_sw_property c_source UCOSII/src/cpu_core.c

# Include files
add_sw_property include_source UCOSII/inc/os/alt_flag.h
add_sw_property include_source UCOSII/inc/os/alt_hooks.h
add_sw_property include_source UCOSII/inc/os/alt_sem.h
add_sw_property include_source UCOSII/inc/priv/alt_flag_ucosii.h
add_sw_property include_source UCOSII/inc/priv/alt_sem_ucosii.h
add_sw_property include_source UCOSII/inc/os_cfg.h
add_sw_property include_source UCOSII/inc/ucos_ii.h
add_sw_property include_source UCOSII/inc/cpu.h
add_sw_property include_source UCOSII/inc/cpu_def.h
add_sw_property include_source UCOSII/inc/cpu_core.h
add_sw_property include_source UCOSII/inc/lib_def.h

# Overridden HAL files
add_sw_property excluded_hal_source HAL/src/alt_env_lock.c
add_sw_property excluded_hal_source HAL/src/alt_malloc_lock.c
add_sw_property excluded_hal_source HAL/inc/os/alt_flag.h
add_sw_property excluded_hal_source HAL/inc/os/alt_hooks.h
add_sw_property excluded_hal_source HAL/inc/os/alt_sem.h

# Include paths
add_sw_property include_directory UCOSII/inc

# Makefile additions
add_sw_property alt_cppflags_addition "-D__ucosii__"

# Register generate-time tcl script for OS_TICKS_PER_SEC definition
add_sw_property systemh_generation_script micrium_ucosii_systemh_generation.tcl

#
# MicroC/OS-II Settings
#

#
# General settings
#
add_sw_setting decimal_number system_h_define os_max_tasks OS_MAX_TASKS 10 "Maximum number of tasks"

add_sw_setting decimal_number system_h_define os_lowest_prio OS_LOWEST_PRIO 20 "Lowest assignable priority"

add_sw_setting boolean system_h_define os_thread_safe_newlib OS_THREAD_SAFE_NEWLIB 1 "Thread safe C library"

#
# Miscellaneous settings
#
add_sw_setting boolean system_h_define miscellaneous.os_arg_chk_en OS_ARG_CHK_EN 1 "Enable argument checking"

add_sw_setting boolean system_h_define miscellaneous.os_cpu_hooks_en OS_CPU_HOOKS_EN 1 "Enable uCOS-II hooks"

add_sw_setting boolean system_h_define miscellaneous.os_debug_en OS_DEBUG_EN 1 "Enable debug variables"

add_sw_setting boolean system_h_define miscellaneous.os_sched_lock_en OS_SCHED_LOCK_EN 1 "Include code for OSSchedLock() and OSSchedUnlock()"

add_sw_setting boolean system_h_define miscellaneous.os_task_stat_en OS_TASK_STAT_EN 1 "Enable statistics task"

add_sw_setting boolean system_h_define miscellaneous.os_task_stat_stk_chk_en OS_TASK_STAT_STK_CHK_EN 1 "Check task stacks from statistics task"

add_sw_setting boolean system_h_define miscellaneous.os_tick_step_en OS_TICK_STEP_EN 1 "Enable tick stepping feature for uCOS-View"

add_sw_setting decimal_number system_h_define miscellaneous.os_event_name_size OS_EVENT_NAME_SIZE 32 "Size of name of Event Control Block groups"

add_sw_setting decimal_number system_h_define miscellaneous.os_max_events OS_MAX_EVENTS 60 "Maximum number of event control blocks"

add_sw_setting decimal_number system_h_define miscellaneous.os_task_idle_stk_size OS_TASK_IDLE_STK_SIZE 512 "Idle task stack size"

add_sw_setting decimal_number system_h_define miscellaneous.os_task_stat_stk_size OS_TASK_STAT_STK_SIZE 512 "Statistics task stack size"

#
# Task settings
#
add_sw_setting boolean system_h_define task.os_task_change_prio_en OS_TASK_CHANGE_PRIO_EN 1 "Include code for OSTaskChangePrio()"

add_sw_setting boolean system_h_define task.os_task_create_en OS_TASK_CREATE_EN 1 "Include code for OSTaskCreate()"

add_sw_setting boolean system_h_define task.os_task_create_ext_en OS_TASK_CREATE_EXT_EN 1 "Include code for OSTaskCreateExt()"

add_sw_setting boolean system_h_define task.os_task_del_en OS_TASK_DEL_EN 1 "Include code for OSTaskDel()"

add_sw_setting decimal_number system_h_define task.os_task_name_size OS_TASK_NAME_SIZE 32 "Size of task name"

add_sw_setting boolean system_h_define task.os_task_profile_en OS_TASK_PROFILE_EN 1 "Include data structure for run-time task profiling"

add_sw_setting boolean system_h_define task.os_task_query_en OS_TASK_QUERY_EN 1 "Include code for OSTaskQuery"

add_sw_setting boolean system_h_define task.os_task_suspend_en OS_TASK_SUSPEND_EN 1 "Include code for OSTaskSuspend() and OSTaskResume()"

add_sw_setting boolean system_h_define task.os_task_sw_hook_en OS_TASK_SW_HOOK_EN 1 "Include code for OSTaskSwHook()"

#
# Time settings
#
add_sw_setting boolean system_h_define time.os_time_tick_hook_en OS_TIME_TICK_HOOK_EN 1 "Include code for OSTimeTickHook()"

add_sw_setting boolean system_h_define time.os_time_dly_resume_en OS_TIME_DLY_RESUME_EN 1 "Include code for OSTimeDlyResume()"

add_sw_setting boolean system_h_define time.os_time_dly_hmsm_en OS_TIME_DLY_HMSM_EN 1 "Include code for OSTimeDlyHMSM()"

add_sw_setting boolean system_h_define time.os_time_get_set_en OS_TIME_GET_SET_EN 1 "Include code for OSTimeGet and OSTimeSet()"

#
# Event flags
#
add_sw_setting boolean system_h_define os_flag_en OS_FLAG_EN 1 "Enable code for Event Flags. CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define event_flag.os_flag_wait_clr_en OS_FLAG_WAIT_CLR_EN 1 "Include code for Wait on Clear Event Flags. CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define event_flag.os_flag_accept_en OS_FLAG_ACCEPT_EN 1 "Include code for OSFlagAccept(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define event_flag.os_flag_del_en OS_FLAG_DEL_EN 1 "Include code for OSFlagDel(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define event_flag.os_flag_query_en OS_FLAG_QUERY_EN 1 "Include code for OSFlagQuery(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting decimal_number system_h_define event_flag.os_flag_name_size OS_FLAG_NAME_SIZE 32 "Size of name of Event Flags group. CAUTION: This is required by the HAL and many Altera device drivers; use caution in reducing this value."

add_sw_setting decimal_number system_h_define event_flag.os_flags_nbits OS_FLAGS_NBITS 16 "Event Flag bits (8,16,32). CAUTION: This is required by the HAL and many Altera device drivers; use caution in changing this value."

add_sw_setting decimal_number system_h_define event_flag.os_max_flags OS_MAX_FLAGS 20 "Maximum number of Event Flags groups. CAUTION: This is required by the HAL and many Altera device drivers; use caution in reducing this value."

#
# Mutex settings
#
add_sw_setting boolean system_h_define os_mutex_en OS_MUTEX_EN 1 "Enable code for Mutex Semaphores"

add_sw_setting boolean system_h_define mutex.os_mutex_accept_en OS_MUTEX_ACCEPT_EN 1 "Include code for OSMutexAccept()"

add_sw_setting boolean system_h_define mutex.os_mutex_del_en OS_MUTEX_DEL_EN 1 "Include code for OSMutexDel()"

add_sw_setting boolean system_h_define mutex.os_mutex_query_en OS_MUTEX_QUERY_EN 1 "Include code for OSMutexQuery"


#
# Semaphore settings
#
add_sw_setting boolean system_h_define os_sem_en OS_SEM_EN 1 "Enable code for semaphores. CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define semaphore.os_sem_accept_en OS_SEM_ACCEPT_EN 1 "Include code for OSSemAccept(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define semaphore.os_sem_set_en OS_SEM_SET_EN 1 "Include code for OSSemSet(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define semaphore.os_sem_del_en OS_SEM_DEL_EN 1 "Include code for OSSemDel(). CAUTION: This is required by the HAL and many Altera device drivers."

add_sw_setting boolean system_h_define semaphore.os_sem_query_en OS_SEM_QUERY_EN 1 "Include code for OSSemQuery(). CAUTION: This is required by the HAL and many Altera device drivers."

#
# Mailbox settings
#
add_sw_setting boolean system_h_define os_mbox_en OS_MBOX_EN 1 "Enable code for mailboxes"

add_sw_setting boolean system_h_define mailbox.os_mbox_accept_en OS_MBOX_ACCEPT_EN 1 "Include code for OSMboxAccept()"

add_sw_setting boolean system_h_define mailbox.os_mbox_del_en OS_MBOX_DEL_EN 1 "Include code for OSMboxDel()"

add_sw_setting boolean system_h_define mailbox.os_mbox_post_en OS_MBOX_POST_EN 1 "Include code for OSMboxPost()"

add_sw_setting boolean system_h_define mailbox.os_mbox_post_opt_en OS_MBOX_POST_OPT_EN 1 "Include code for OSMboxPostOpt()"

add_sw_setting boolean system_h_define mailbox.os_mbox_query_en OS_MBOX_QUERY_EN 1 "Include code for OSMboxQuery()"

#
# Message queue settings
#
add_sw_setting boolean system_h_define os_q_en OS_Q_EN 1 "Enable code for Queues"

add_sw_setting boolean system_h_define queue.os_q_accept_en OS_Q_ACCEPT_EN 1 "Include code for OSQAccept()"

add_sw_setting boolean system_h_define queue.os_q_del_en OS_Q_DEL_EN 1 "Include code for OSQDel()"

add_sw_setting boolean system_h_define queue.os_q_flush_en OS_Q_FLUSH_EN 1 "Include code for OSQFlush()"

add_sw_setting boolean system_h_define queue.os_q_post_en OS_Q_POST_EN 1 "Include code of OSQFlush()"

add_sw_setting boolean system_h_define queue.os_q_post_front_en OS_Q_POST_FRONT_EN 1 "Include code for OSQPostFront()"

add_sw_setting boolean system_h_define queue.os_q_post_opt_en OS_Q_POST_OPT_EN 1 "Include code for OSQPostOpt()"

add_sw_setting boolean system_h_define queue.os_q_query_en OS_Q_QUERY_EN 1 "Include code for OSQQuery()"

add_sw_setting decimal_number system_h_define queue.os_max_qs OS_MAX_QS 20 "Maximum number of Queue Control Blocks"

#
# Memory management settings
#
add_sw_setting boolean system_h_define os_mem_en OS_MEM_EN 1 "Enable code for memory management"

add_sw_setting boolean system_h_define memory.os_mem_query_en OS_MEM_QUERY_EN 1 "Include code for OSMemQuery()"

add_sw_setting decimal_number system_h_define memory.os_mem_name_size OS_MEM_NAME_SIZE 32 "Size of memory partition name"

add_sw_setting decimal_number system_h_define memory.os_max_mem_part OS_MAX_MEM_PART 60 "Maximum number of memory partitions"

#
# Timer settings
#
add_sw_setting boolean system_h_define os_tmr_en OS_TMR_EN 0 "Enable code for timers"

add_sw_setting decimal_number system_h_define timer.os_task_tmr_stk_size OS_TASK_TMR_STK_SIZE 512 "Stack size for timer task"

add_sw_setting decimal_number system_h_define timer.os_task_tmr_prio OS_TASK_TMR_PRIO 0 "Priority of timer task (0=highest)"

add_sw_setting decimal_number system_h_define timer.os_tmr_cfg_max OS_TMR_CFG_MAX 16 "Maximum number of timers"

add_sw_setting decimal_number system_h_define timer.os_tmr_cfg_name_size OS_TMR_CFG_NAME_SIZE 16 "Size of timer name"

add_sw_setting decimal_number system_h_define timer.os_tmr_cfg_ticks_per_sec OS_TMR_CFG_TICKS_PER_SEC 10 "Rate at which timer management task runs (Hz)"

add_sw_setting decimal_number system_h_define timer.os_tmr_cfg_wheel_size OS_TMR_CFG_WHEEL_SIZE 2 "Size of timer wheel (number of spokes)"

# End of file

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <mach/mach.h>
#include "stuff/openstep_mach.h"

#include "lock.h"
#include "debug.h"
#include "errors.h"
#include "dyld_init.h"
#include "allocate.h"

/* access to these by the program's thread are protected by global_lock */
volatile enum bool dyld_lock = FALSE;
volatile mach_port_t thread_that_has_dyld_lock = MACH_PORT_NULL;

volatile mach_port_t cached_thread = MACH_PORT_NULL;
volatile vm_address_t cached_stack = 0;

static void yield(unsigned long factor);
static mach_port_t dyld_mach_thread_self(void);

/*
 * set_lock() is called when a thread enters the dynamic link editor.  Only
 * one thread my be executing in the dynamic linker as it is effecting the
 * global state in the task.  The only one case that the dynamic linker
 * will transfer control back to the user's code while holding the lock is for
 * the multiply defined error handler and the link edit error handler.  The user
 * can't call the dynamic linker in his error handler but just in case he does
 * we detect the dead lock and he is killed.
 */
void
set_lock(void)
{
    mach_port_t my_thread;

	my_thread = dyld_mach_thread_self();

	/*
	 * The debug interface takes care of making sure the dyld_lock is not
	 * taken but can't do the same for the global_lock.  So if this is the
	 * debug thread do nothing as it is safe.
	 */
	if(my_thread == debug_thread)
	    return;

	/*
	 * First we check for the dead lock case by getting the global lock so
	 * thread_that_has_dyld_lock can be inspected.
	 */
	while(try_to_get_lock(global_lock) == FALSE){
	    yield(2);
	}
	/*
	 * Now that we have the global_lock, see if thread_that_has_dyld_lock
	 * is my_thread.  It will be MACH_PORT_NULL if the dyld_lock is FALSE
	 * which is never my_thread.
	 */
	if(thread_that_has_dyld_lock == my_thread)
	    dead_lock_error();

	/*
	 * Now that we know this thread does not have the dyld_lock, this loop
	 * gets the dyld_lock for this thread yielding while it can't get it.
	 */
	for(;;){
	    /* now we have the global_lock, see if dyld_lock is FALSE */
	    if(dyld_lock == FALSE){
		if(dyld_mem_protect == TRUE){
		    unprotect_data_segment();
		    unprotect_allocated_memory();
		}
		thread_that_has_dyld_lock = my_thread;
		dyld_lock = TRUE;
		clear_lock(global_lock);
		return;
	    }
	    else{
		/* dyld_lock is TRUE so release the global lock and yield */
		clear_lock(global_lock);
		yield(2);
	    }
	    /* get the global lock so dyld_lock can be inspected */
	    while(try_to_get_lock(global_lock) == FALSE){
		yield(2);
	    }
	}
}

/*
 * set_lock_or_in_multiply_defined_handler() is used in some dyld library
 * functions when tey are allowed to be made in a multiply defined handler.
 * This function must only read the dyld data structures and not write anything.
 * This function returns TRUE if it set the lock or FALSE if not (when this
 * thread is in the multiply defined handler).
 */ 
enum bool
set_lock_or_in_multiply_defined_handler(
void)
{
    mach_port_t my_thread;

	my_thread = dyld_mach_thread_self();

	/*
	 * The debug interface takes care of making sure the dyld_lock is not
	 * taken but can't do the same for the global_lock.  So if this is the
	 * debug thread do nothing as it is safe.
	 */
	if(my_thread == debug_thread)
	    return(FALSE);

	/*
	 * Get the global lock so thread_that_has_dyld_lock can be inspected.
	 */
	while(try_to_get_lock(global_lock) == FALSE){
	    yield(2);
	}

	/*
	 * If thread_that_has_dyld_lock is this thread and we are in the
	 * multiply defined handler clear the global lock and return FALSE to 
	 * the caller to indicate that the dyld lock was not taken out by this
	 * call.
	 */
	if(thread_that_has_dyld_lock == my_thread &&
	   lock_in_multiply_defined_handler == TRUE){
	    clear_lock(global_lock);
	    return(FALSE);
	}
	clear_lock(global_lock);

	/*
	 * We are not in the multiply defined handler so set the lock and return
	 * TRUE to the caller to indicate that the dyld lock was taken out by
	 * this call.
	 */
	set_lock();
	return(TRUE);
}

/*
 * release_lock() is called when a thread leaves the dynamic link editor.
 * If the debugging thread wants the lock then this thread suspends itself
 * after releasing the lock.
 */
void
release_lock(void)
{
    mach_port_t my_thread;

	my_thread = dyld_mach_thread_self();

	/*
	 * If we just finished linking a module with the
	 * LINK_OPTION_RETURN_ON_ERROR which would have set return_on_error
	 * then clear the remove_on_error flag on all the libraries loaded
	 * and clear return_on_error.
	 */
	if(return_on_error){
	    clear_remove_on_error_libraries();
	    return_on_error = FALSE;
	}

	/*
	 * The debug interface takes care of making sure the dyld_lock is not
	 * taken but can't do the same for the global_lock.  So if this is the
	 * debug thread do nothing as it is safe.
	 */
	if(my_thread == debug_thread)
	    return;

	/* first get the global lock so that dyld_lock can be released */
	while(try_to_get_lock(global_lock) == FALSE){
	    yield(1);
	}
	thread_that_has_dyld_lock = MACH_PORT_NULL;
	dyld_lock = FALSE;
	clear_lock(global_lock);

	if(dyld_mem_protect == TRUE){
	    protect_allocated_memory();
	    protect_data_segment();
	}

	/*
	 * If the debug thread is waiting for the lock and this is not the debug
	 * thread itself (in the case of server_dyld_debug_bind_module()) then
	 * suspend ourself and the debug thread can have the lock.
	 */ 
	if(debug_thread_waiting_for_lock == TRUE &&
	   debug_thread != my_thread)
	    thread_suspend(my_thread);

}

/*
 * yield() is called when a lock that is wanted can't be obtained my this
 * thread.
 */
static
void
yield(
unsigned long factor)
{
    static struct host_sched_info info = { 0 };
    unsigned int count;
    kern_return_t r;
    mach_port_t my_mach_host_self;

	/*
	 * Don't spin trying to get a lock, as this can cause dead
	 * locks if fixed priority threads are in the task.  So just
	 * let the scheduler switch to any other thread that can run.
	 * That is don't try to switch to the thread that has the
	 * lock as it may not be runnable.
	 */
	if(info.min_timeout == 0){
	    count = HOST_SCHED_INFO_COUNT;
	    my_mach_host_self = mach_host_self();
	    if((r = host_info(my_mach_host_self, HOST_SCHED_INFO, (host_info_t)
			      (&info), &count)) != KERN_SUCCESS){
		mach_port_deallocate(mach_task_self(), my_mach_host_self);
		mach_error(r, "can't get host sched info");
	    }
	    mach_port_deallocate(mach_task_self(), my_mach_host_self);
	}
	thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS,
		      factor * info.min_timeout);
}

/*
 * dyld_mach_thread_self() gets the current thread's mach_port_t as cheaply as
 * possible.  Requires the global_lock NOT to have been taken.
 */
static
mach_port_t
dyld_mach_thread_self(void)
{
    mach_port_t my_thread;
    vm_address_t stack_addr;

	my_thread = MACH_PORT_NULL;
	stack_addr = (vm_address_t)trunc_page((vm_address_t)&my_thread);

	while(try_to_get_lock(global_lock) == FALSE){
	    yield(1);
	}
	if(cached_stack == stack_addr){
	    my_thread = cached_thread;
	}
	else{
	    if(cached_thread != MACH_PORT_NULL)
#ifdef __MACH30__
	    	(void)mach_port_deallocate(mach_task_self(), cached_thread);
#else
	    	(void)port_deallocate(mach_task_self(), cached_thread);
#endif
	    my_thread = mach_thread_self();
	    cached_thread = my_thread;
	    cached_stack = stack_addr;
	}
	clear_lock(global_lock);
	return(my_thread);
}

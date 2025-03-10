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
#include <stdlib.h>
#include <errno.h>
#include <objc/zone.h>
#include "zoneprotect.h"

#include "allocate.h"
#include "errors.h"

static NXZone *zone = NULL;

void *
allocate(
unsigned long size)
{
    void *p;
    int errnum;
/*
error("allocate %lu", size);
link_edit_error(DYLD_WARNING, 0, NULL);
*/

	if(size == 0)
	    return(NULL);
	if(zone == NULL){
	    zone = NXCreateZone(vm_page_size, vm_page_size, 1);
	    if(zone == NULL){
		errnum = errno;
		system_error(errnum, "can't create NXZone (for internal data "
		    "structures)");
		link_edit_error(DYLD_UNIX_RESOURCE, errnum, NULL);
		halt();
		exit(DYLD_EXIT_FAILURE_BASE + DYLD_UNIX_RESOURCE);
	    }
	    NXNameZone(zone, "dyld");
	}
	if((p = NXZoneMalloc(zone, size)) == NULL){
	    errnum = errno;
	    system_error(errnum, "can't allocate memory (for internal data "
		"structures)");
	    link_edit_error(DYLD_UNIX_RESOURCE, errnum, NULL);
	    halt();
	    exit(DYLD_EXIT_FAILURE_BASE + DYLD_UNIX_RESOURCE);
	}
	return(p);
}

void *
allocate_with_no_error_check(
unsigned long size)
{
/*
error("allocate_with_no_error_check %lu", size);
link_edit_error(DYLD_WARNING, 0, NULL);
*/

	if(size == 0)
	    return(NULL);
	if(zone == NULL){
	    zone = NXCreateZone(vm_page_size, vm_page_size, 1);
	    if(zone == NULL){
		return(NULL);
	    }
	    NXNameZone(zone, "dyld");
	}
	return(NXZoneMalloc(zone, size));
}

void
protect_allocated_memory(
void)
{
#ifndef MALLOC_DEBUG
    int errnum;
#endif

	if(zone == NULL)
	    return;
	
#ifndef MALLOC_DEBUG
	if(NXProtectZone(zone, NXZoneReadOnly))
	    return;

	errnum = errno;
	system_error(errnum, "can't protect allocated memory (for internal "
	    "data structures");
	link_edit_error(DYLD_UNIX_RESOURCE, errnum, NULL);
	halt();
	exit(DYLD_EXIT_FAILURE_BASE + DYLD_UNIX_RESOURCE);
#endif
}

void
unprotect_allocated_memory(
void)
{
#ifndef MALLOC_DEBUG
    int errnum;
#endif

	if(zone == NULL)
	    return;
	
#ifndef MALLOC_DEBUG
	if(NXProtectZone(zone, NXZoneReadWrite))
	    return;

	errnum = errno;
	system_error(errnum, "can't unprotect allocated memory (for internal "
	    "data structures");
	link_edit_error(DYLD_UNIX_RESOURCE, errnum, NULL);
	halt();
	exit(DYLD_EXIT_FAILURE_BASE + DYLD_UNIX_RESOURCE);
#endif
}

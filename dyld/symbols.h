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
#include <mach-o/nlist.h>

#include "images.h"

/*
 * The structure of an element in a symbol list.
 *
 * If you CHANGE this structure you must change the number of bytes allocated
 * in the (__DATA,__symbol_blocks) section for _symbol_blocks in the file
 * section_order.s
 */
struct symbol_list {
    char *name;			/* name of the symbol */
    struct nlist *symbol;	/* the undefined symbol or NULL */
    struct image *image;	/* the image the symbol is in or NULL */
    enum bool remove_on_error;	/* set when return_on_error is set */
    enum bool bind_fully;	/* dependent symbols are to be bound */
    enum bool flat_reference;	/* symbol is for a flat namespace reference */
    struct symbol_list *prev;	/* previous in the chain */
    struct symbol_list *next;	/* next in the chain */
};
/* The undefined list */
extern struct symbol_list undefined_list;


/*
 * Used only internally to lookup_symbol() to determine loops of N_INDR
 * symbols.
 */
struct indr_loop_list {
    struct nlist *defined_symbol;	/* the defined symbol */
    module_state *defined_module;	/* the module the symbol is in */
    struct image *defined_image;	/* the image the module is in */
    struct library_image
		 *defined_library_image;
    struct indr_loop_list *next;
};
#define NO_INDR_LOOP ((struct indr_loop_list *)1)

extern void setup_initial_undefined_list(
    enum bool all_symbols);
extern void setup_prebound_coalesed_symbols(
    void);
extern void clear_being_linked_list(
    enum bool only_remove_on_error);
extern void clear_undefined_list(
    enum bool only_remove_on_error);
extern enum bool resolve_undefineds(
    enum bool bind_now,
    enum bool launching_with_prebound_libraries);
extern void clear_module_states_saved(
    void);
extern void clear_state_changes_to_the_modules(
    void);
extern void lookup_symbol(
    char *symbol_name,
    struct image *primary_image,
    struct twolevel_hint *hint,
    enum bool weak_symbol,
    struct nlist **defined_symbol,
    module_state **defined_module,
    struct image **defined_image,
    struct library_image **defined_library_image,
    struct indr_loop_list *indr_loop);
extern void lookup_symbol_in_hinted_library(
    char *symbol_name,
    char *hinted_library,
    struct nlist **defined_symbol,
    module_state **defined_module,
    struct image **defined_image,
    struct library_image **defined_library_image);
extern enum bool lookup_symbol_in_object_image(
    char *symbol_name,
    struct image *object_image,
    module_state *object_module,
    struct nlist **defined_symbol,
    module_state **defined_module,
    struct image **defined_image,
    struct library_image **defined_library_image,
    struct indr_loop_list *indr_loop);
extern enum bool validate_NSSymbol(
    struct nlist *symbol,
    module_state **defined_module,
    struct image **defined_image,
    struct library_image **defined_library_image);
extern enum bool validate_NSModule(
    module_state *module,
    struct image **defined_image,
    struct library_image **defined_library_image);
extern void relocate_symbol_pointers_in_object_image(
    struct image *image);
extern void relocate_symbol_pointers_in_library_image(
    struct image *image);
extern void relocate_symbol_pointers_for_defined_externs(
    struct image *image);
extern void change_symbol_pointers_in_flat_images(
    char *symbol_name,
    unsigned long value,
    enum bool only_lazy_pointers);
extern void bind_symbol_by_name(
    char *symbol_name,
    unsigned long *address,
    module_state **module,
    struct nlist **symbol,
    enum bool change_symbol_pointers);
extern enum bool is_symbol_coalesced(
    struct image *image,
    struct nlist *symbol);
extern enum bool link_library_module(
    struct library_image *library_image,
    struct image *image,
    module_state *module,
    enum bool bind_now,
    enum bool bind_fully,
    enum bool launching_with_prebound_libraries);
extern enum bool link_object_module(
    struct object_image *object_image,
    enum bool bind_now,
    enum bool bind_fully);
extern void unlink_object_module(
    struct object_image *object_image,
    enum bool reset_lazy_references);
extern enum bool link_in_need_modules(
    enum bool bind_now,
    enum bool release_lock,
    struct object_image *reloc_just_this_object_image);
extern enum bool check_executable_for_overrides(
    void);
extern enum bool check_libraries_for_overrides(
    void);
extern void discard_symbol(
    struct image *image,
    struct nlist *symbol);
/* TODO: make this a static inline */
extern struct image * get_primary_image(
    struct image *image,
    struct nlist *symbol);
/* TODO: make this a static inline */
extern struct twolevel_hint * get_hint(
    struct image *image,
    struct nlist *symbol);
/* TODO: make this a static inline */
extern enum bool get_weak(
    struct nlist *symbol);

/*
 * These are for the private ZeroLink callbacks.
 */
typedef void (*object_image_register_proc)(
    const char* imageName,
    struct object_image* image);
typedef struct object_image * (*object_image_locator_proc)(
    const char* symbol_name);
extern object_image_register_proc object_image_register;
extern object_image_locator_proc object_image_locator;

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
#include <mach-o/loader.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef EGREGIOUS_HACK_FOR_SAMPLER
#include <dyld/bool.h>
#else
#include "stuff/bool.h"
#endif
#include <sys/types.h>

enum link_state {
    UNLINKED,		/* the starting point for UNLINKED modules */
    BEING_LINKED,	/* moduled selected to be link into the program */
    RELOCATED,		/* moduled relocated dyld can now use the module */
    REGISTERING,	/* functions registered for modules being link called */
    INITIALIZING,	/* module initializers being called */
    LINKED,		/* module initializers called user can now use module */
    FULLY_LINKED,	/* module fully linked (all lazy symbols resolved) */

    PREBOUND_UNLINKED,	/* the module is prebound image but unlinked after */
			/*  the program was launch. */

    BEING_UNLINKED,	/* not yet used.  TODO unlinking */
    REPLACED,		/* not yet used.  TODO replacing */

    UNUSED		/* a module handle that is now unused */
};

struct image {
    char *physical_name;	/* physical image name (file name), same as */
				/*  name below if the NSLinkModule() option */
				/*  NSLINKMODULE_OPTION_TRAILING_PHYS_NAME is */
				/*  not used. */
    unsigned long vmaddr_slide; /* The amount the vmaddresses are slid in the */
				/*  image from the staticly link addresses. */
    struct mach_header *mh;	/* The mach header of the image. */
    unsigned long valid;	/* TRUE if this is struct is valid */
    /*
     * The above four fields can't change without breaking gdb(1) see the
     * comments in <mach-o/dyld_gdb.h> for gdb_dyld_version 1.
     */
    char *name;			/* Image name for reporting errors. */
    /*
     * Also the above field can't change without breaking gdb(1) for
     * gdb_dyld_version 2, see the comments in <mach-o/dyld_gdb.h>.
     */
    unsigned long vmaddr_size;  /* The size of the vm this image uses */
    unsigned long seg1addr;	/* The address of the first segment */
    unsigned long		/* The address of the first read-write segment*/
	segs_read_write_addr;	/*  used only for MH_SPLIT_SEGS images. */
    struct symtab_command *st;	/* The symbol table command for the image. */
    struct dysymtab_command	/* The dynamic symbol table command for the */
	*dyst;			/*  image. */
    struct segment_command	/* The link edit segment command for the */
	*linkedit_segment;	/*  image. */
    struct routines_command *rc;/* The routines command for the image */
    struct twolevel_hints_command/* The twolevel hints command for the image */
	*hints_cmd;
    struct section *init;	/* The mod init section */
    struct section *term;	/* The mod term section */
#ifdef __ppc__
    unsigned long 		/* the image's dyld_stub_binding_helper */
	dyld_stub_binding_helper; /* address */
#endif
    unsigned long
      prebound:1,		/* Link states set from prebound state */
      all_modules_linked:1,	/* set if all the modules are linked */
      change_protect_on_reloc:1,/* The image has relocations in read-only */
				/*  segments and protection needs to change. */
      cache_sync_on_reloc:1,	/* The image has relocations for instructions */
				/*  and the i cache needs to sync with d cache*/
      registered:1,		/* The functions registered for add images */
				/*  have been called */
      private:1,		/* global symbols are not used for linking */
      init_bound:1,		/* the image init routine has been bound */
      init_called:1,		/* the image init routine has been called */
      has_coalesced_sections:1, /* the image has coalesced sections */
      sub_images_setup:1,	/* the sub images have been set up */
      umbrella_images_setup:1,  /* the umbrella_images have been set up */
      two_level_debug_printed:1,/* printed when TWO_LEVEL_DEBUG is on */
      undone_prebound_lazy_pointers:1, /* all prebound lazy pointer un done */
      subtrees_twolevel_prebound_setup:1, /* state of this and deps setup */
      subtrees_twolevel_prebound:1, /* this and deps twolevel and prebound */
      image_can_use_hints:1,	/* set when the hints are usable in this image*/
      subs_can_use_hints:1,	/* set when the hints are usable for images */
				/*  that have this image as a sub image */
      dont_call_mod_init:1,	/* don't call any module init routines, this */
			        /*  is currently only for object images */
      /*
       * This is set for library images when the NSAddImage() option 
       * NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME is used.
       */
      match_filename_by_installname:1,

      /*
       * This is set library images when we are trying to use its prebinding
       * after the program is launched.  This is only done if all libraries
       * previously loaded are prebound, two-level and have all modules
       * bound.
       */
      trying_to_use_prebinding_post_launch:1,

      /*
       * This is set to TRUE in resolve_non_lazy_symbol_pointers_in_image()
       * after all the non-lazy symbol pointers in the image are set. 
       */
      non_lazy_symbol_pointers_resolved:1,

      /*
       * This is set to TRUE when some of the modules in an image are being
       * fully bound. When that happens then the code in
       * relocate_symbol_pointers_in_library_image() can't just call
       * resolve_non_lazy_symbol_pointers_in_image() but must cause the lazy
       * pointers for bound symbols used by the module to be set.
       */
      some_modules_being_bound_fully:1,

      unused:11;
    /*
     * For two-level namespace images this is the array of pointers to the
     * dependent images and the count of them.
     */
    struct image **dependent_images;
    unsigned long ndependent_images;
    /*
     * If this is a library image which has a framework name or library name
     * then this is the part that would be the umbrella name or library name
     * and the size of the name.  This points into the name and since framework
     * and library names may have suffixes the size is needed to exclude it.
     * This is only needed for two-level namespace images.  umbrella_name and
     * or library_name will be NULL and name_size will be 0 if there is no
     * umbrella name.
     */
    char *umbrella_name;
    char *library_name;
    unsigned long name_size;

    /* array of pointers to sub-frameworks and sub-umbrellas and count */
    struct image **sub_images;
    unsigned long nsub_images;
    /* array of pointers to back to umbrella frameworks and sub-umbrellas
     if any which the image is a part of and the count of them. */
    struct image **umbrella_images;
    unsigned long numbrella_images;

    /*
     * This fields is only used to point back to the library image or object
     * image structure.  This is so that the for two-level namespace lookups
     * the library_image and object_image can be quickly obtained from the image
     * structure for a calls to lookup_symbol_in_library_image() and
     * lookup_symbol_in_object_image().
     */
    void *outer_image;
};

/*
 * This is really an enum link_state.  Originally there was a module structure
 * that had an enum link_state field.  Because the minimum structure aligment 
 * is more than one-byte aligned this wasted space.  Since this is one of the
 * few allocated and written data structures of dyld it is important it is as
 * small as reasonable.  It needs to be addressable so using less than a byte
 * is not acceptable.
 */
typedef char module_state;

/*
 * To keep track of which modules are being fully bound the 0x80 bit of the
 * module state is used.  Fully bound is where all of the dependent references
 * are bound into the program.  Where fully linked in here means that the single
 * module has all it's lazy as well as it's not lazy symbols linked.  If an
 * image has an image initialization routine then the module containing that
 * routine is full bound before it is called.
 */
#define GET_LINK_STATE(m) ((enum link_state)((m) & 0xf))
#define SET_LINK_STATE(m,l) m = (((m) & 0xf0) | ((l) & 0xf))
#define GET_FULLYBOUND_STATE(m) ((m) & 0x80)
#define SET_FULLYBOUND_STATE(m) m = ((m) | 0x80)
#define CLEAR_FULLYBOUND_STATE(m) m = ((m) & 0x7f)

/*
 * To allow image initialization routines (shared library init routines) to
 * force modules in there image to have there module initialization routines
 * run (C++ initializers, via a call to __initializeCplusplus) the 0x40 bit of
 * the module state is used to keep track of if the module initialization
 * routine as been run.  The module initialization routines are normally run
 * after the the image initialization routines so this bit is needed to make
 * sure a module initialization routine is not run twice.
 */
#define GET_MODINIT_STATE(m) ((m) & 0x40)
#define SET_MODINIT_STATE(m) m = ((m) | 0x40)

/*
 * To support module termination routines (to be used for C++ destructors) the
 * 0x20 bit of the module state is used to keep track of if the module
 * termination routine has been run.
 */
#define GET_MODTERM_STATE(m) ((m) & 0x20)
#define SET_MODTERM_STATE(m) m = ((m) | 0x20)

/*
 * To support calling image initialization routines (shared library init
 * routines) in their dependency order each module that defines a shared library
 * init routine and its dependents needs to be checked.  As each module is
 * checked it is marked so that is only checked once.
 */
#define GET_IMAGE_INIT_DEPEND_STATE(m) ((m) & 0x10)
#define SET_IMAGE_INIT_DEPEND_STATE(m) m = ((m) | 0x10)

struct object_image {
    struct image image;
    module_state module;
    enum bool module_state_saved;
    module_state saved_module_state;
};

struct library_image {
    struct image image;
    unsigned long nmodules;
    module_state *modules;
    struct dylib_command *dlid;
    dev_t dev;
    ino_t ino;
    enum bool dependent_libraries_loaded;
    enum bool remove_on_error;
    enum bool module_states_saved;
    module_state *saved_module_states;
    unsigned long library_offset;
};

/*
 * Using /System/Library/CoreServices/Desktop.app/Contents/MacOS/Desktop from
 * MacOS X Public Beta (Kodiak1G7)
 * TOTAL number of bundles	4
 * TOTAL number of libraries	58
 */
enum nobject_images { NOBJECT_IMAGES = 5 };
struct object_images {
    struct object_image images[NOBJECT_IMAGES];
    unsigned long nimages;
    struct object_images *next_images;
    /*
     * The above three fields can't change without breaking gdb(1) see the
     * comments in <mach-o/dyld_gdb.h>.
     */
};
extern struct object_images object_images;

enum nlibrary_images { NLIBRARY_IMAGES = 60 };
struct library_images {
    struct library_image images[NLIBRARY_IMAGES];
    unsigned long nimages;
    struct library_images *next_images;
    /*
     * The above three fields can't change without breaking gdb(1) see the
     * comments in <mach-o/dyld_gdb.h>.
     */
};
extern struct library_images library_images;

extern const struct library_image some_weak_library_image;
extern const struct nlist some_weak_symbol;
extern const module_state some_weak_module;

extern void (*dyld_monaddition)(char *lowpc, char *highpc);

extern void load_executable_image(
    char *name,
    struct mach_header *mh_execute,
    unsigned long *entry_point);

extern enum bool load_dependent_libraries(
    void);

extern enum bool load_library_image(
    struct dylib_command *dl,
    char *dylib_name,
    enum bool force_searching,
    enum bool match_filename_by_installname,
    struct image **image_pointer,
    enum bool *already_loaded,
    enum bool reference_from_dylib);

extern void unload_remove_on_error_libraries(
    void);

extern void clear_remove_on_error_libraries(
    void);

extern void clear_trying_to_use_prebinding_post_launch(
    void);

extern struct object_image *map_bundle_image(
    char *name,
    char *physical_name,
    char *object_addr,
    unsigned long object_size);

extern void unload_bundle_image(
    struct object_image *object_image,
    enum bool keepMemoryMapped,
    enum bool reset_lazy_references);

extern void shared_pcsample_buffer(
    char *name,
    struct section *s,
    unsigned long slide_value);

extern enum bool set_images_to_prebound(
    void);

extern void set_all_twolevel_modules_prebound(
    void);

extern void undo_prebound_images(
    enum bool post_launch_libraries_only);

extern void find_twolevel_prebound_lib_subtrees(
    void);

extern void try_to_use_prebound_libraries(
    void);

extern void call_image_init_routines(
    enum bool make_delayed_calls);

extern char *executables_name;

extern char *save_string(
    char *name);

extern void unsave_string(
    char *string);

extern void create_executables_path(
    char *exec_path);

extern struct object_image *find_object_image(
    struct image *image);

extern enum bool is_library_loaded_by_name(
    char *dylib_name,
    struct dylib_command *dl,
    struct image **image_pointer,
    enum bool reference_from_dylib);

extern enum bool is_library_loaded_by_stat(
    char *dylib_name,
    struct dylib_command *dl,
    struct stat *stat_buf,
    struct image **image_pointer,
    enum bool reference_from_dylib);

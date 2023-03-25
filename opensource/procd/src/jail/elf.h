/*
 * Copyright (C) 2015 John Crispin <blogic@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ELF_H__
#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#include "log.h"

struct library {
	struct avl_node avl;
	char *name;
	char *path;
};

struct library_path {
	struct list_head list;
	char *path;
};

extern struct avl_tree libraries;

extern void alloc_library_path(const char *path);
extern char* find_lib(char *file);
extern int elf_load_deps(char *library);
extern void load_ldso_conf(const char *conf);

#endif

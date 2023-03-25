/*
 * Copyright (C) 2014 John Crispin <blogic@openwrt.org>
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

#ifndef _FS_STATE_H__
#define _FS_STATE_H__

#include <libubox/list.h>
#include <libubox/blob.h>

struct volume;

enum {
	FS_NONE,
	FS_SNAPSHOT,
	FS_JFFS2,
	FS_DEADCODE,
	FS_UBIFS,
	FS_EXT4FS,
};

extern char const *extroot_prefix;
extern int mount_extroot(void);
extern int mount_snapshot(struct volume *v);
extern int mount_overlay(struct volume *v);

extern int mount_move(char *oldroot, char *newroot, char *dir);
extern int pivot(char *new, char *old);
extern int fopivot(char *rw_root, char *ro_root);
extern int ramoverlay(void);

extern int find_overlay_mount(char *overlay);
extern char* find_mount(char *mp);
extern char* find_mount_point(char *block, int mtd_only);
extern int find_filesystem(char *fs);

extern int jffs2_switch(struct volume *v);

extern int handle_whiteout(const char *dir);
extern void foreachdir(const char *dir, int (*cb)(const char*));

#endif

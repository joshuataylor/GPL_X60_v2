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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "libfstools.h"

/* this is a raw syscall - man 2 pivot_root */
extern int pivot_root(const char *new_root, const char *put_old);

int
mount_move(char *oldroot, char *newroot, char *dir)
{
#ifndef MS_MOVE
#define MS_MOVE	(1 << 13)
#endif
	struct stat s;
	char olddir[64];
	char newdir[64];
	int ret;

	snprintf(olddir, sizeof(olddir), "%s%s", oldroot, dir);
	snprintf(newdir, sizeof(newdir), "%s%s", newroot, dir);

	if (stat(olddir, &s) || !S_ISDIR(s.st_mode))
		return -1;

	if (stat(newdir, &s) || !S_ISDIR(s.st_mode))
		return -1;

	ret = mount(olddir, newdir, NULL, MS_NOATIME | MS_MOVE, NULL);

/*	if (ret)
		fprintf(stderr, "failed %s %s: %s\n", olddir, newdir, strerror(errno));*/

	return ret;
}

int
pivot(char *new, char *old)
{
	char pivotdir[64];
	int ret;

	if (mount_move("", new, "/proc"))
		return -1;

	snprintf(pivotdir, sizeof(pivotdir), "%s%s", new, old);

	ret = pivot_root(new, pivotdir);

	if (ret < 0) {
		fprintf(stderr, "pivot_root failed %s %s: %s\n", new, pivotdir, strerror(errno));
		return -1;
	}

	mount_move(old, "", "/dev");
	mount_move(old, "", "/tmp");
	mount_move(old, "", "/sys");
	mount_move(old, "", "/overlay");

	return 0;
}

int
fopivot(char *rw_root, char *ro_root)
{
	char overlay[64], lowerdir[64];

	if (find_filesystem("overlay")) {
		fprintf(stderr, "BUG: no suitable fs found\n");
		return -1;
	}

	snprintf(overlay, sizeof(overlay), "overlayfs:%s", rw_root);

	/*
	 * First, try to mount without a workdir, for overlayfs v22 and before.
	 * If it fails, it means that we are probably using a v23 and
	 * later versions that require a workdir
	 */
	snprintf(lowerdir, sizeof(lowerdir), "lowerdir=/,upperdir=%s", rw_root);
	if (mount(overlay, "/mnt", "overlayfs", MS_NOATIME, lowerdir)) {
		char upperdir[64], workdir[64], upgrade[64], upgrade_dest[64];
		struct stat st;

		snprintf(upperdir, sizeof(upperdir), "%s/upper", rw_root);
		snprintf(workdir, sizeof(workdir), "%s/work", rw_root);
		snprintf(upgrade, sizeof(upgrade), "%s/sysupgrade.tgz", rw_root);
		snprintf(upgrade_dest, sizeof(upgrade_dest), "%s/sysupgrade.tgz", upperdir);
		snprintf(lowerdir, sizeof(lowerdir), "lowerdir=/,upperdir=%s,workdir=%s",
			 upperdir, workdir);

		/*
		 * Overlay FS v23 and later requires both a upper and
		 * a work directory, both on the same filesystem, but
		 * not part of the same subtree.
		 * We can't really deal with these constraints without
		 * creating two new subdirectories in /overlay.
		 */
		mkdir(upperdir, 0755);
		mkdir(workdir, 0755);

		if (stat(upgrade, &st) == 0)
		    rename(upgrade, upgrade_dest);

		/* Mainlined overlayfs has been renamed to "overlay", try that first */
		if (mount(overlay, "/mnt", "overlay", MS_NOATIME, lowerdir)) {
			if (mount(overlay, "/mnt", "overlayfs", MS_NOATIME, lowerdir)) {
				fprintf(stderr, "mount failed: %s, options %s\n",
					strerror(errno), lowerdir);
				return -1;
			}
		}
	}

	return pivot("/mnt", ro_root);
}

int
ramoverlay(void)
{
	mkdir("/tmp/root", 0755);
	mount("tmpfs", "/tmp/root", "tmpfs", MS_NOATIME, "mode=0755");

	return fopivot("/tmp/root", "/rom");
}

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

#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>

#include "libfstools/libfstools.h"
#include "libfstools/volume.h"

/*
 * Called in the early (PREINIT) stage, when we immediately need some writable
 * filesystem.
 */
static int
start(int argc, char *argv[1])
{
	struct volume *root;
	struct volume *data = volume_find("rootfs_data");

	if (!getenv("PREINIT"))
		return -1;

	if (!data) {
		root = volume_find("rootfs");
		volume_init(root);
		fprintf(stderr, "mounting /dev/root\n");
		mount("/dev/root", "/", NULL, MS_NOATIME | MS_REMOUNT, 0);
	}

	/*
	 * Before trying to mount and use "rootfs_data" let's check if there is
	 * extroot configured. Following call will handle reading config from
	 * the "rootfs_data" on its own.
	 */
	extroot_prefix = "";
	if (!mount_extroot()) {
		fprintf(stderr, "fs-state: switched to extroot\n");
		return 0;
	}

	/* There isn't extroot, so just try to mount "rootfs_data" */
	switch (volume_identify(data)) {
	case FS_NONE:
	case FS_DEADCODE:
		/*
		 * Filesystem isn't ready yet and we are in the preinit, so we
		 * can't afford waiting for it. Use tmpfs for now and handle it
		 * properly in the "done" call.
		 */
		return ramoverlay();

	case FS_JFFS2:
	case FS_UBIFS:
	case FS_EXT4FS:
		mount_overlay(data);
		break;

	case FS_SNAPSHOT:
		mount_snapshot(data);
		break;
	}

	return 0;
}

static int
stop(int argc, char *argv[1])
{
	if (!getenv("SHUTDOWN"))
		return -1;

	return 0;
}

/*
 * Called at the end of init, it can wait for filesystem if needed.
 */
static int
done(int argc, char *argv[1])
{
	struct volume *v = volume_find("rootfs_data");

	if (!v)
		return -1;

	switch (volume_identify(v)) {
	case FS_NONE:
	case FS_DEADCODE:
		return jffs2_switch(v);
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return start(argc, argv);
	if (!strcmp(argv[1], "stop"))
		return stop(argc, argv);
	if (!strcmp(argv[1], "done"))
		return done(argc, argv);
	return -1;
}

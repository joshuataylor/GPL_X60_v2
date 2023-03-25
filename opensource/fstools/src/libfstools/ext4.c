/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/byteorder.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mtd/mtd-user.h>
#include <glob.h>

#include "libfstools.h"

#include "volume.h"

#define ext4_sysfs_path "/sys/block/mmcblk*/mmcblk*/uevent"
#define PATH_MAX	128

#define EXT_SB_OFF	0x400
#define EXT_SB_KBOFF	(EXT_SB_OFF >> 10)
#define EXT_SB_MAGIC	"\123\357"
#define EXT_MAG_OFF	0x38

struct ext4_priv {
	char	*name;
        char    *devname;
};

static struct driver ext4_driver;

static int ext4_volume_init(struct volume *v)
{
	char buf[PATH_MAX];
	struct ext4_priv *p;

	p = (struct ext4_priv*)v->priv;
	snprintf(buf, sizeof(buf), "/dev/%s",p->devname);

	v->name = strdup(p->name);
	v->type = 5;
	v->blk = strdup(buf);
	return 0;
}

static int ext4_find_index(char *dev,char *name)
{
	int j;
	FILE *fp;
	char tmp[PATH_MAX];
	char partname[PATH_MAX];
	glob_t gl;

	if (glob(ext4_sysfs_path, GLOB_NOESCAPE | GLOB_MARK, NULL, &gl) < 0)
		return -1;

	for (j = 0; j < gl.gl_pathc; j++) {
		if ((fp = fopen(gl.gl_pathv[j], "r"))) {
			 while(fgets(tmp, sizeof(tmp), fp))  {
				if(strstr(tmp, "DEVNAME"))  {
					strcpy(partname,tmp+strlen("DEVNAME="));
				}
				if(strstr(tmp,name))  {
					partname[sizeof(partname) - 1] = '\0';
					j = strlen(partname) - 1;
					while (j > 0 && partname[j] <= ' ')
						partname[j--] = '\0';
					strcpy(dev,partname);
					fclose(fp);
					globfree(&gl);
					return 0;
				}
			}
			fclose(fp);
		}
	}
	globfree(&gl);
	return -1;
}

static int check_for_mtd(const char *mtd)
{
	FILE *fp;
	char dev[PATH_MAX];

	if ((fp = fopen("/proc/mtd", "r"))) {
		while (fgets(dev, sizeof(dev), fp)) {
			if (strstr(dev, mtd)) {
				fclose(fp);
				return -1;
			}
		}
	}
	fclose(fp);
	return 0;
}

static int ext4_volume_find(struct volume *v, char *name)
{
	char buf[PATH_MAX];
	struct ext4_priv *p;

	if (find_filesystem("ext4"))
		return -1;

	if (check_for_mtd(name))
		return -1;

	if(ext4_find_index(buf,name))
		return -1;

        p = calloc(1, sizeof(struct ext4_priv));
        if (!p)
                return -1;

        v->priv = p;
        v->drv = &ext4_driver;

        p->devname = strdup(buf);
        p->name = strdup(name);
        return ext4_volume_init(v);
}

static int ext4_volume_identify(struct volume *v)
{
	char magic[32] = { 0 };
	int off = (EXT_SB_KBOFF * 1024) + EXT_MAG_OFF;
	int fd;

	fd = open(v->blk, O_RDONLY);
	if(fd == -1)
		return -1;

	lseek(fd, off, SEEK_SET);
	read(fd, magic, sizeof(EXT_SB_MAGIC) - 1);
	close(fd);

	if (!memcmp(EXT_SB_MAGIC, magic, sizeof(EXT_SB_MAGIC) - 1)) {
		return FS_EXT4FS;
	}

	fprintf(stderr, "ext4 is not ready - marker found\n");
	return FS_DEADCODE;
}

static struct driver ext4_driver = {
        .name = "ext4",
        .find = ext4_volume_find,
        .init = ext4_volume_init,
        .identify = ext4_volume_identify,
};
DRIVER(ext4_driver);

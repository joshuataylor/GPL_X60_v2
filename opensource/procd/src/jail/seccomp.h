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

#define INFO(fmt, ...) do { \
	syslog(0,"preload-seccomp: "fmt, ## __VA_ARGS__); \
	fprintf(stderr,"preload-seccomp: "fmt, ## __VA_ARGS__); \
	} while (0)

int install_syscall_filter(const char *argv, const char *file);

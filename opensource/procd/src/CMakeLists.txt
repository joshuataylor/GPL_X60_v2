cmake_minimum_required(VERSION 2.6)

PROJECT(procd C)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

IF(CONFIG_INIT_SEQUEUE)
    ADD_DEFINITIONS(-DCONFIG_INIT_SEQUEUE)
ENDIF()

IF(CONFIG_PROCD_BOOT_TIMEOUT)
    ADD_DEFINITIONS(-DCONFIG_PROCD_BOOT_TIMEOUT)
ENDIF()

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()


ADD_LIBRARY(setlbf SHARED service/setlbf.c)
INSTALL(TARGETS setlbf
	LIBRARY DESTINATION lib
)


SET(SOURCES procd.c signal.c watchdog.c state.c	inittab.c rcS.c	ubus.c system.c
	service/service.c service/instance.c service/validate.c service/trigger.c service/watch.c
	plug/coldplug.c plug/hotplug.c utils/utils.c)

SET(LIBS ubox ubus json-c blobmsg_json json_script)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
ENDIF()

IF(ZRAM_TMPFS)
  ADD_DEFINITIONS(-DZRAM_TMPFS)
  SET(SOURCES_ZRAM initd/zram.c)
ENDIF()

IF(BUILD_UPGRADED)
  add_subdirectory(upgraded)
ENDIF()

ADD_EXECUTABLE(procd ${SOURCES})
TARGET_LINK_LIBRARIES(procd ${LIBS})
INSTALL(TARGETS procd
	RUNTIME DESTINATION sbin
)


ADD_EXECUTABLE(init initd/init.c initd/early.c initd/preinit.c initd/mkdev.c watchdog.c
	utils/utils.c ${SOURCES_ZRAM})
TARGET_LINK_LIBRARIES(init ${LIBS})
INSTALL(TARGETS init
	RUNTIME DESTINATION sbin
)


ADD_EXECUTABLE(udevtrigger plug/udevtrigger.c)
INSTALL(TARGETS udevtrigger
	RUNTIME DESTINATION sbin
)


ADD_EXECUTABLE(askfirst utils/askfirst.c)
INSTALL(TARGETS askfirst
	RUNTIME DESTINATION sbin
)

ADD_CUSTOM_COMMAND(
	OUTPUT syscall-names.h
	COMMAND ./make_syscall_h.sh ${CMAKE_C_COMPILER} > ./syscall-names.h
	DEPENDS ./make_syscall_h.sh
)
ADD_CUSTOM_TARGET(syscall-names-h DEPENDS syscall-names.h)

ADD_CUSTOM_COMMAND(
	OUTPUT capabilities-names.h
	COMMAND ./make_capabilities_h.sh ${CMAKE_C_COMPILER} > ./capabilities-names.h
	DEPENDS ./make_capabilities_h.sh
)
ADD_CUSTOM_TARGET(capabilities-names-h DEPENDS capabilities-names.h)

IF(SECCOMP_SUPPORT)
ADD_LIBRARY(preload-seccomp SHARED jail/preload.c jail/seccomp.c)
TARGET_LINK_LIBRARIES(preload-seccomp dl ubox blobmsg_json)
INSTALL(TARGETS preload-seccomp
	LIBRARY DESTINATION lib
)
ADD_DEPENDENCIES(preload-seccomp syscall-names-h)
endif()

IF(JAIL_SUPPORT)
ADD_EXECUTABLE(ujail jail/jail.c jail/elf.c jail/capabilities.c)
TARGET_LINK_LIBRARIES(ujail ubox blobmsg_json)
INSTALL(TARGETS ujail
	RUNTIME DESTINATION sbin
)
ADD_DEPENDENCIES(ujail capabilities-names-h)
endif()

IF(UTRACE_SUPPORT)
ADD_EXECUTABLE(utrace trace/trace.c)
TARGET_LINK_LIBRARIES(utrace ubox ${json} blobmsg_json)
INSTALL(TARGETS utrace
	RUNTIME DESTINATION sbin
)
ADD_DEPENDENCIES(utrace syscall-names-h)

ADD_LIBRARY(preload-trace SHARED trace/preload.c)
TARGET_LINK_LIBRARIES(preload-trace dl)
INSTALL(TARGETS preload-trace
	LIBRARY DESTINATION lib
)
endif()

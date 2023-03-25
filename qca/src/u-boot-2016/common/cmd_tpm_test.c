/*
 * Copyright (c) 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <tpm.h>

/* Prints error and returns on failure */
#define TPM_CHECK(tpm_command) do { \
	uint32_t result; \
	\
	result = (tpm_command); \
	if (result != TPM_SUCCESS) { \
		printf("TEST FAILED: line %d: " #tpm_command ": 0x%x\n", \
			__LINE__, result); \
		return result; \
	} \
} while (0)

#define INDEX0			0xda70
#define INDEX1			0xda71
#define INDEX2			0xda72
#define INDEX3			0xda73
#define INDEX_INITIALISED	0xda80
#define PHYS_PRESENCE		4
#define PRESENCE		8

static uint32_t TlclStartupIfNeeded(void)
{
	uint32_t result = tpm_startup(TPM_ST_CLEAR);

	return result == TPM_INVALID_POSTINIT ? TPM_SUCCESS : result;
}

static int test_timer(void)
{
	printf("get_timer(0) = %lu\n", get_timer(0));
	return 0;
}

static uint32_t tpm_get_flags(uint8_t *disable, uint8_t *deactivated,
			      uint8_t *nvlocked)
{
	struct tpm_permanent_flags pflags;
	uint32_t result;

	result = tpm_get_permanent_flags(&pflags);
	if (result)
		return result;
	if (disable)
		*disable = pflags.disable;
	if (deactivated)
		*deactivated = pflags.deactivated;
	if (nvlocked)
		*nvlocked = pflags.nv_locked;
	debug("TPM: Got flags disable=%d, deactivated=%d, nvlocked=%d\n",
	      pflags.disable, pflags.deactivated, pflags.nv_locked);

	return 0;
}

static uint32_t tpm_set_global_lock(void)
{
	uint32_t x;

	debug("TPM: Set global lock\n");
	return tpm_nv_write_value(INDEX0, (uint8_t *)&x, 0);
}

static uint32_t tpm_nv_write_value_lock(uint32_t index)
{
	debug("TPM: Write lock 0x%x\n", index);

	return tpm_nv_write_value(index, NULL, 0);
}

static uint32_t tpm_nv_set_locked(void)
{
	debug("TPM: Set NV locked\n");

	return tpm_nv_define_space(TPM_NV_INDEX_LOCK, 0, 0);
}

static int tpm_is_owned(void)
{
	uint8_t response[TPM_PUBEK_SIZE];
	uint32_t result;

	result = tpm_read_pubek(response, sizeof(response));

	return result != TPM_SUCCESS;
}

static int test_early_extend(void)
{
	uint8_t value_in[20];
	uint8_t value_out[20];

	printf("Testing earlyextend ...");
	tpm_init();
	TPM_CHECK(tpm_startup(TPM_ST_CLEAR));
	TPM_CHECK(tpm_continue_self_test());
	TPM_CHECK(tpm_extend(1, value_in, value_out));
	printf("done\n");
	return 0;
}

static int test_early_nvram(void)
{
	uint32_t x;

	printf("Testing earlynvram ...");
	tpm_init();
	TPM_CHECK(tpm_startup(TPM_ST_CLEAR));
	TPM_CHECK(tpm_continue_self_test());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_nv_read_value(INDEX0, (uint8_t *)&x, sizeof(x)));
	printf("done\n");
	return 0;
}

static int test_early_nvram2(void)
{
	uint32_t x;

	printf("Testing earlynvram2 ...");
	tpm_init();
	TPM_CHECK(tpm_startup(TPM_ST_CLEAR));
	TPM_CHECK(tpm_continue_self_test());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_nv_write_value(INDEX0, (uint8_t *)&x, sizeof(x)));
	printf("done\n");
	return 0;
}

static int test_enable(void)
{
	uint8_t disable = 0, deactivated = 0;

	printf("Testing enable ...\n");
	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_self_test_full());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_get_flags(&disable, &deactivated, NULL));
	printf("\tdisable is %d, deactivated is %d\n", disable, deactivated);
	TPM_CHECK(tpm_physical_enable());
	TPM_CHECK(tpm_physical_set_deactivated(0));
	TPM_CHECK(tpm_get_flags(&disable, &deactivated, NULL));
	printf("\tdisable is %d, deactivated is %d\n", disable, deactivated);
	if (disable == 1 || deactivated == 1)
		printf("\tfailed to enable or activate\n");
	printf("\tdone\n");
	return 0;
}

#define reboot() do { \
	printf("\trebooting...\n"); \
	reset_cpu(0); \
} while (0)

static int test_fast_enable(void)
{
	uint8_t disable = 0, deactivated = 0;
	int i;

	printf("Testing fastenable ...\n");
	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_self_test_full());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_get_flags(&disable, &deactivated, NULL));
	printf("\tdisable is %d, deactivated is %d\n", disable, deactivated);
	for (i = 0; i < 2; i++) {
		TPM_CHECK(tpm_force_clear());
		TPM_CHECK(tpm_get_flags(&disable, &deactivated, NULL));
		printf("\tdisable is %d, deactivated is %d\n", disable,
		       deactivated);
		assert(disable == 1 && deactivated == 1);
		TPM_CHECK(tpm_physical_enable());
		TPM_CHECK(tpm_physical_set_deactivated(0));
		TPM_CHECK(tpm_get_flags(&disable, &deactivated, NULL));
		printf("\tdisable is %d, deactivated is %d\n", disable,
		       deactivated);
		assert(disable == 0 && deactivated == 0);
	}
	printf("\tdone\n");
	return 0;
}

static int test_global_lock(void)
{
	uint32_t zero = 0;
	uint32_t result;
	uint32_t x;

	printf("Testing globallock ...\n");
	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_self_test_full());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_nv_read_value(INDEX0, (uint8_t *)&x, sizeof(x)));
	TPM_CHECK(tpm_nv_write_value(INDEX0, (uint8_t *)&zero,
				     sizeof(uint32_t)));
	TPM_CHECK(tpm_nv_read_value(INDEX1, (uint8_t *)&x, sizeof(x)));
	TPM_CHECK(tpm_nv_write_value(INDEX1, (uint8_t *)&zero,
				     sizeof(uint32_t)));
	TPM_CHECK(tpm_set_global_lock());
	/* Verifies that write to index0 fails */
	x = 1;
	result = tpm_nv_write_value(INDEX0, (uint8_t *)&x, sizeof(x));
	assert(result == TPM_AREA_LOCKED);
	TPM_CHECK(tpm_nv_read_value(INDEX0, (uint8_t *)&x, sizeof(x)));
	assert(x == 0);
	/* Verifies that write to index1 is still possible */
	x = 2;
	TPM_CHECK(tpm_nv_write_value(INDEX1, (uint8_t *)&x, sizeof(x)));
	TPM_CHECK(tpm_nv_read_value(INDEX1, (uint8_t *)&x, sizeof(x)));
	assert(x == 2);
	/* Turns off PP */
	tpm_tsc_physical_presence(PHYS_PRESENCE);
	/* Verifies that write to index1 fails */
	x = 3;
	result = tpm_nv_write_value(INDEX1, (uint8_t *)&x, sizeof(x));
	assert(result == TPM_BAD_PRESENCE);
	TPM_CHECK(tpm_nv_read_value(INDEX1, (uint8_t *)&x, sizeof(x)));
	assert(x == 2);
	printf("\tdone\n");
	return 0;
}

static int test_lock(void)
{
	printf("Testing lock ...\n");
	tpm_init();
	tpm_startup(TPM_ST_CLEAR);
	tpm_self_test_full();
	tpm_tsc_physical_presence(PRESENCE);
	tpm_nv_write_value_lock(INDEX0);
	printf("\tLocked 0x%x\n", INDEX0);
	printf("\tdone\n");
	return 0;
}

static void initialise_spaces(void)
{
	uint32_t zero = 0;
	uint32_t perm = TPM_NV_PER_WRITE_STCLEAR | TPM_NV_PER_PPWRITE;

	printf("\tInitialising spaces\n");
	tpm_nv_set_locked();  /* useful only the first time */
	tpm_nv_define_space(INDEX0, perm, 4);
	tpm_nv_write_value(INDEX0, (uint8_t *)&zero, 4);
	tpm_nv_define_space(INDEX1, perm, 4);
	tpm_nv_write_value(INDEX1, (uint8_t *)&zero, 4);
	tpm_nv_define_space(INDEX2, perm, 4);
	tpm_nv_write_value(INDEX2, (uint8_t *)&zero, 4);
	tpm_nv_define_space(INDEX3, perm, 4);
	tpm_nv_write_value(INDEX3, (uint8_t *)&zero, 4);
	perm = TPM_NV_PER_READ_STCLEAR | TPM_NV_PER_WRITE_STCLEAR |
		TPM_NV_PER_PPWRITE;
	tpm_nv_define_space(INDEX_INITIALISED, perm, 1);
}

static int test_readonly(void)
{
	uint8_t c;
	uint32_t index_0, index_1, index_2, index_3;
	int read0, read1, read2, read3;

	printf("Testing readonly ...\n");
	tpm_init();
	tpm_startup(TPM_ST_CLEAR);
	tpm_self_test_full();
	tpm_tsc_physical_presence(PRESENCE);
	/*
	 * Checks if initialisation has completed by trying to read-lock a
	 * space that's created at the end of initialisation
	 */
	if (tpm_nv_read_value(INDEX_INITIALISED, &c, 0) == TPM_BADINDEX) {
		/* The initialisation did not complete */
		initialise_spaces();
	}

	/* Checks if spaces are OK or messed up */
	read0 = tpm_nv_read_value(INDEX0, (uint8_t *)&index_0, sizeof(index_0));
	read1 = tpm_nv_read_value(INDEX1, (uint8_t *)&index_1, sizeof(index_1));
	read2 = tpm_nv_read_value(INDEX2, (uint8_t *)&index_2, sizeof(index_2));
	read3 = tpm_nv_read_value(INDEX3, (uint8_t *)&index_3, sizeof(index_3));
	if (read0 || read1 || read2 || read3) {
		printf("Invalid contents\n");
		return 0;
	}

	/*
	 * Writes space, and locks it.  Then attempts to write again.
	 * I really wish I could use the imperative.
	 */
	index_0 += 1;
	if (tpm_nv_write_value(INDEX0, (uint8_t *)&index_0, sizeof(index_0) !=
		TPM_SUCCESS)) {
		error("\tcould not write index 0\n");
	}
	tpm_nv_write_value_lock(INDEX0);
	if (tpm_nv_write_value(INDEX0, (uint8_t *)&index_0, sizeof(index_0)) ==
			TPM_SUCCESS)
		error("\tindex 0 is not locked\n");

	printf("\tdone\n");
	return 0;
}

static int test_redefine_unowned(void)
{
	uint32_t perm;
	uint32_t result;
	uint32_t x;

	printf("Testing redefine_unowned ...");
	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_self_test_full());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	assert(!tpm_is_owned());

	/* Ensures spaces exist. */
	TPM_CHECK(tpm_nv_read_value(INDEX0, (uint8_t *)&x, sizeof(x)));
	TPM_CHECK(tpm_nv_read_value(INDEX1, (uint8_t *)&x, sizeof(x)));

	/* Redefines spaces a couple of times. */
	perm = TPM_NV_PER_PPWRITE | TPM_NV_PER_GLOBALLOCK;
	TPM_CHECK(tpm_nv_define_space(INDEX0, perm, 2 * sizeof(uint32_t)));
	TPM_CHECK(tpm_nv_define_space(INDEX0, perm, sizeof(uint32_t)));
	perm = TPM_NV_PER_PPWRITE;
	TPM_CHECK(tpm_nv_define_space(INDEX1, perm, 2 * sizeof(uint32_t)));
	TPM_CHECK(tpm_nv_define_space(INDEX1, perm, sizeof(uint32_t)));

	/* Sets the global lock */
	tpm_set_global_lock();

	/* Verifies that index0 cannot be redefined */
	result = tpm_nv_define_space(INDEX0, perm, sizeof(uint32_t));
	assert(result == TPM_AREA_LOCKED);

	/* Checks that index1 can */
	TPM_CHECK(tpm_nv_define_space(INDEX1, perm, 2 * sizeof(uint32_t)));
	TPM_CHECK(tpm_nv_define_space(INDEX1, perm, sizeof(uint32_t)));

	/* Turns off PP */
	tpm_tsc_physical_presence(PHYS_PRESENCE);

	/* Verifies that neither index0 nor index1 can be redefined */
	result = tpm_nv_define_space(INDEX0, perm, sizeof(uint32_t));
	assert(result == TPM_BAD_PRESENCE);
	result = tpm_nv_define_space(INDEX1, perm, sizeof(uint32_t));
	assert(result == TPM_BAD_PRESENCE);

	printf("done\n");
	return 0;
}

#define PERMPPGL (TPM_NV_PER_PPWRITE | TPM_NV_PER_GLOBALLOCK)
#define PERMPP TPM_NV_PER_PPWRITE

static int test_space_perm(void)
{
	uint32_t perm;

	printf("Testing spaceperm ...");
	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_continue_self_test());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_get_permissions(INDEX0, &perm));
	assert((perm & PERMPPGL) == PERMPPGL);
	TPM_CHECK(tpm_get_permissions(INDEX1, &perm));
	assert((perm & PERMPP) == PERMPP);
	printf("done\n");
	return 0;
}

static int test_startup(void)
{
	uint32_t result;
	printf("Testing startup ...\n");

	tpm_init();
	result = tpm_startup(TPM_ST_CLEAR);
	if (result != 0 && result != TPM_INVALID_POSTINIT)
		printf("\ttpm startup failed with 0x%x\n", result);
	result = tpm_get_flags(NULL, NULL, NULL);
	if (result != 0)
		printf("\ttpm getflags failed with 0x%x\n", result);
	printf("\texecuting SelfTestFull\n");
	tpm_self_test_full();
	result = tpm_get_flags(NULL, NULL, NULL);
	if (result != 0)
		printf("\ttpm getflags failed with 0x%x\n", result);
	printf("\tdone\n");
	return 0;
}

/*
 * Runs [op] and ensures it returns success and doesn't run longer than
 * [time_limit] in milliseconds.
 */
#define TTPM_CHECK(op, time_limit) do { \
	ulong start, time; \
	uint32_t __result; \
	\
	start = get_timer(0); \
	__result = op; \
	if (__result != TPM_SUCCESS) { \
		printf("\t" #op ": error 0x%x\n", __result); \
		return -1; \
	} \
	time = get_timer(start); \
	printf("\t" #op ": %lu ms\n", time); \
	if (time > (ulong)time_limit) { \
		printf("\t" #op " exceeded " #time_limit " ms\n"); \
	} \
} while (0)


static int test_timing(void)
{
	uint32_t x;
	uint8_t in[20], out[20];

	printf("Testing timing ...");
	tpm_init();
	TTPM_CHECK(TlclStartupIfNeeded(), 50);
	TTPM_CHECK(tpm_continue_self_test(), 100);
	TTPM_CHECK(tpm_self_test_full(), 1000);
	TTPM_CHECK(tpm_tsc_physical_presence(PRESENCE), 100);
	TTPM_CHECK(tpm_nv_write_value(INDEX0, (uint8_t *)&x, sizeof(x)), 100);
	TTPM_CHECK(tpm_nv_read_value(INDEX0, (uint8_t *)&x, sizeof(x)), 100);
	TTPM_CHECK(tpm_extend(0, in, out), 200);
	TTPM_CHECK(tpm_set_global_lock(), 50);
	TTPM_CHECK(tpm_tsc_physical_presence(PHYS_PRESENCE), 100);
	printf("done\n");
	return 0;
}

#define TPM_MAX_NV_WRITES_NOOWNER 64

static int test_write_limit(void)
{
	printf("Testing writelimit ...\n");
	int i;
	uint32_t result;

	tpm_init();
	TPM_CHECK(TlclStartupIfNeeded());
	TPM_CHECK(tpm_self_test_full());
	TPM_CHECK(tpm_tsc_physical_presence(PRESENCE));
	TPM_CHECK(tpm_force_clear());
	TPM_CHECK(tpm_physical_enable());
	TPM_CHECK(tpm_physical_set_deactivated(0));

	for (i = 0; i < TPM_MAX_NV_WRITES_NOOWNER + 2; i++) {
		printf("\twriting %d\n", i);
		result = tpm_nv_write_value(INDEX0, (uint8_t *)&i, sizeof(i));
		switch (result) {
		case TPM_SUCCESS:
			break;
		case TPM_MAXNVWRITES:
			assert(i >= TPM_MAX_NV_WRITES_NOOWNER);
		default:
			error("\tunexpected error code %d (0x%x)\n",
			      result, result);
		}
	}

	/* Reset write count */
	TPM_CHECK(tpm_force_clear());
	TPM_CHECK(tpm_physical_enable());
	TPM_CHECK(tpm_physical_set_deactivated(0));

	/* Try writing again. */
	TPM_CHECK(tpm_nv_write_value(INDEX0, (uint8_t *)&i, sizeof(i)));
	printf("\tdone\n");
	return 0;
}

#define VOIDTEST(XFUNC) \
	int do_test_##XFUNC(cmd_tbl_t *cmd_tbl, int flag, int argc, \
	char * const argv[]) \
	{ \
		return test_##XFUNC(); \
	}

#define VOIDENT(XNAME) \
	U_BOOT_CMD_MKENT(XNAME, 0, 1, do_test_##XNAME, "", ""),

VOIDTEST(early_extend)
VOIDTEST(early_nvram)
VOIDTEST(early_nvram2)
VOIDTEST(enable)
VOIDTEST(fast_enable)
VOIDTEST(global_lock)
VOIDTEST(lock)
VOIDTEST(readonly)
VOIDTEST(redefine_unowned)
VOIDTEST(space_perm)
VOIDTEST(startup)
VOIDTEST(timing)
VOIDTEST(write_limit)
VOIDTEST(timer)

static cmd_tbl_t cmd_cros_tpm_sub[] = {
	VOIDENT(early_extend)
	VOIDENT(early_nvram)
	VOIDENT(early_nvram2)
	VOIDENT(enable)
	VOIDENT(fast_enable)
	VOIDENT(global_lock)
	VOIDENT(lock)
	VOIDENT(readonly)
	VOIDENT(redefine_unowned)
	VOIDENT(space_perm)
	VOIDENT(startup)
	VOIDENT(timing)
	VOIDENT(write_limit)
	VOIDENT(timer)
};

static int do_tpmtest(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	printf("argc = %d, argv = ", argc);
	do {
		int i = 0;

		for (i = 0; i < argc; i++)
			printf(" %s", argv[i]);
			printf("\n------\n");
		} while (0);
	argc--;
	argv++;
	c = find_cmd_tbl(argv[0], cmd_cros_tpm_sub,
			 ARRAY_SIZE(cmd_cros_tpm_sub));
	return c ? c->cmd(cmdtp, flag, argc, argv) : cmd_usage(cmdtp);
}

U_BOOT_CMD(tpmtest, 2, 1, do_tpmtest, "TPM tests",
	"\n\tearly_extend\n"
	"\tearly_nvram\n"
	"\tearly_nvram2\n"
	"\tenable\n"
	"\tfast_enable\n"
	"\tglobal_lock\n"
	"\tlock\n"
	"\treadonly\n"
	"\tredefine_unowned\n"
	"\tspace_perm\n"
	"\tstartup\n"
	"\ttiming\n"
	"\twrite_limit\n");

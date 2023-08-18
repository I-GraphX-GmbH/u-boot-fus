/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * F&S image processing
 *
 * When the SPL starts it needs to know what board it is running on. Usually
 * this information is read from NAND or eMMC. It then selects the correct DDR
 * RAM settings, initializes DRAM and then loads the ATF (and probably other
 * images like opTEE).
 *
 * Originally all these parts are all separate files: DRAM timings, DRAM
 * training firmware, the SPL code itself, the ATF code, board configurations,
 * etc. By using F&S headers, these files are all combined in one image that
 * is called NBoot.
 *
 * However if the board is empty (after having been assembled), or if the
 * configuration does not work for some reason, it is necessary to use the SDP
 * Serial Download Protocol to provide the configuration information.
 * Unfortunately the NBoot file is too big to fit in any available OCRAM and
 * TCM memory. But splitting NBoot in smaller parts and downloading separately
 * one after the other is also very uncomfortable and error prone. So the idea
 * was to interpret the data *while* it is downloaded. We call this
 * "streaming" the NBoot configuration file. Every time a meaningful part is
 * fully received, it is used to do the next part of the initialization.
 *
 * This detection is done by looking at the F&S headers. Each header gives
 * information for the next part of data. The following is a general view of
 * the NBoot structure.
 *
 *   +--------------------------------------------+
 *   | BOARD-ID (id) (optional)                   | CRC32 header
 *   |   +----------------------------------------+
 *   |   | NBOOT (arch)                           | Signed / CRC32 header+image
 *   |   |   +------------------------------------+
 *   |   |   | SPL (arch)                         | Signed / CRC32 header+image
 *   |   |   +------------------------------------+
 *   |   |   | BOARD-INFO (arch)                  | CRC32 header
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | ...                            |
 *   |   |   |---+--------------------------------+
 *   |   |   | FIRMWARE (arch)                    | CRC32 header
 *   |   |   |   +--------------------------------+
 *   |   |   |   | DRAM-INFO (arch)               | CRC32 header
 *   |   |   |   |   +----------------------------+
 *   |   |   |   |   | DRAM-TYPE (ddr3l)          | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (ddr3l)        | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (ddr4)           | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (ddr4)         | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (lpddr4)         | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (lpddr4)       | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   +---+---+------------------------+
 *   |   |   |   | ATF (arch)                     | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | TEE (arch) (optional)          | Signed / CRC32 header+image
 *   |   |   +---+--------------------------------+
 *   |   |   | EXTRAS                             |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BASH-SCRIPT (addfsheader.sh)   |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BASH-SCRIPT (fsimage.sh)       |
 *   +---+---+---+--------------------------------+
 *
 * Comments
 *
 * - The BOARD-ID is only present when the board is empty; from a customers
 *   view, NBoot has no such BOARD-ID prepended.
 * - The BASH-SCRIPT fsimage.sh can be used to extract the individual parts
 *   from an NBOOT image. It should be the last part in NBoot, so that it can
 *   easily be found and extracted with some grep/tail commands.
 *
 * Only a subset from such an NBoot image needs to be stored on the board:
 *
 * - Exactly one BOARD-CFG is stored; it identifies the board.
 * - SPL is stored in a special way so that the ROM loader can execute it.
 * - The FIRMWARE section is stored for DRAM settings, ATF and TEE. In fact
 *   from the DRAM settings only the specific setting for the specific board
 *   would be needed, but it may be too complicated to extract this part only
 *   when saving the data.
 *
 * When NBoot is downloaded via USB, then data is coming in automatically. We
 * have no way of triggering the next part of data. So we need a state machine
 * to interpret the stream data and do the necessary initializations at the
 * time when the data is available.
 *
 * When loading the data from NAND or eMMC, this could theoretically be done
 * differently, because here we can actively read the necessary parts. But
 * as we do not want to implement two versions of the interpreter, we try to
 * imitate the USB data flow for NAND and eMMC, too. So in the end we can use
 * the same state machine for all configuration scenarios.
 *
 * When the "download" is started, we get a list of jobs that have to be done.
 * For example:
 *
 * 1. Load board configuration
 * 2. Initialize DRAM
 * 3. Load ATF
 *
 * The state machine then always loads the next F&S header and decides from its
 * type what to do next. If a specific kind of initialization is in the job
 * list, it loads the image part and performs the necessary initialization
 * steps. Otherwise this image part is skipped.
 *
 * Not all parts of an NBoot image must be present. For example as only the
 * FIRMWARE part is stored on the board, the state machine can only perform the
 * last two steps. This has to be taken care of by the caller. If a previous
 * step is missing for some specific job, the job can also not be done. For
 * example a TEE file can only be loaded to DRAM, so DRAM initialization must
 * be done before that.
 */

#include <common.h>
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <spl.h>
#include <mmc.h>
#include <nand.h>
#include <sdp.h>
#include <asm/sections.h>
#include <u-boot/crc.h>			/* crc32() */

#include "fs_board_common.h"		/* fs_board_*() */
#include "fs_dram_common.h"		/* fs_dram_init_common() */
#include "fs_image_common.h"		/* Own interface */

#ifdef CONFIG_FS_SECURE_BOOT
#include <asm/mach-imx/hab.h>
#include <asm/mach-imx/checkboot.h>
#include <stdbool.h>
#include <hang.h>
#endif

/* Structure to handle board name and revision separately */
struct bnr {
	char name[MAX_DESCR_LEN];
	unsigned int rev;
};

static struct bnr compare_bnr;		/* Used for BOARD-ID comparisons */

static char board_id[MAX_DESCR_LEN + 1]; /* Current board-id */


/* ------------- Functions in SPL and U-Boot ------------------------------- */

/* Return the F&S architecture */
const char *fs_image_get_arch(void)
{
	return CONFIG_SYS_BOARD;
}

/* Check if this is an F&S image */
bool fs_image_is_fs_image(const struct fs_header_v1_0 *fsh)
{
	return !strncmp(fsh->info.magic, "FSLX", sizeof(fsh->info.magic));
}

/* Return the intended address of the board configuration in OCRAM */
void *fs_image_get_regular_cfg_addr(void)
{
	return (void*)CONFIG_FUS_BOARDCFG_ADDR;
}

/* Return the real address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(void)
{
	void *cfg;

	if (!gd->board_cfg)
		gd->board_cfg = (ulong)fs_image_get_regular_cfg_addr();

	cfg = (void *)gd->board_cfg;

	return cfg;
}

/* Return the fdt part of the given board configuration */
void *fs_image_find_cfg_fdt(struct fs_header_v1_0 *fsh)
{
	void *fdt = fsh + 1;

#ifdef CONFIG_FS_SECURE_BOOT
	if (((struct ivt *)fdt)->hdr.magic == IVT_HEADER_MAGIC)
		fdt += HAB_HEADER;
#endif

	return fdt;
}

/* Return the fdt part of the board configuration in OCRAM */
void *fs_image_get_cfg_fdt(void)
{
	return fs_image_find_cfg_fdt(fs_image_get_cfg_addr());
}

/* Return the address of the /nboot-info node */
int fs_image_get_nboot_info_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/nboot-info");
}

/* Return the address of the /board-cfg node */
int fs_image_get_board_cfg_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/board-cfg");
}

/* Return pointer to string with NBoot version */
const char *fs_image_get_nboot_version(void *fdt)
{
	int offs;

	if (!fdt)
		fdt = fs_image_get_cfg_fdt();

	offs = fs_image_get_nboot_info_offs(fdt);
	return fdt_getprop(fdt, offs, "version", NULL);
}

/* Read the image size (incl. padding) from an F&S header */
unsigned int fs_image_get_size(const struct fs_header_v1_0 *fsh,
			       bool with_fs_header)
{
	/* We ignore the high word, boot images are definitely < 4GB */
	return fsh->info.file_size_low + (with_fs_header ? FSH_SIZE : 0);
}

/* Check image magic, type and descr; return true on match */
bool fs_image_match(const struct fs_header_v1_0 *fsh,
		    const char *type, const char *descr)
{
	if (!type)
		return false;

	if (!fs_image_is_fs_image(fsh))
		return false;

	if (strncmp(fsh->type, type, MAX_TYPE_LEN))
		return false;

	if (descr) {
		if (!(fsh->info.flags & FSH_FLAGS_DESCR))
			return false;
		if (strncmp(fsh->param.descr, descr, MAX_DESCR_LEN))
			return false;
	}

	return true;
}

static void fs_image_get_board_name_rev(const char id[MAX_DESCR_LEN],
					struct bnr *bnr)
{
	char c;
	int i;
	int rev = -1;

	/* Copy string and look for rightmost '.' */
	bnr->rev = 0;
	i = 0;
	do {
		c = id[i];
		bnr->name[i] = c;
		if (!c)
			break;
		if (c == '.')
			rev = i;
	} while (++i < sizeof(bnr->name));

	/* No revision found, assume 0 */
	if (rev < 0)
		return;

	bnr->name[rev] = '\0';
	while (++rev < i) {
		char c = bnr->name[rev];

		if ((c < '0') || (c > '9'))
			break;
		bnr->rev = bnr->rev * 10 + c - '0';
	}
}

/* Check if ID of the given BOARD-CFG matches the compare_id */
bool fs_image_match_board_id(struct fs_header_v1_0 *cfg_fsh)
{
	struct bnr bnr;

	/* Compare magic and type */
	if (!fs_image_match(cfg_fsh, "BOARD-CFG", NULL))
		return false;

	/* A config must include a description, this is the board ID */
	if (!(cfg_fsh->info.flags & FSH_FLAGS_DESCR))
		return false;

	/* Split board ID of the config we look at into name and rev */
	fs_image_get_board_name_rev(cfg_fsh->param.descr, &bnr);

	/*
	 * Compare with name and rev of the BOARD-ID we are looking for (in
	 * compare_bnr). In the new layout, the BOARD-CFG does not have a
	 * revision anymore, so here bnr.rev is 0 and is accepted for any
	 * BOARD-ID of this board type.
	 */
	if (strncmp(bnr.name, compare_bnr.name, sizeof(bnr.name)))
		return false;
	if (bnr.rev > compare_bnr.rev)
		return false;

	return true;
}

/*
 * Check CRC32; return: 0: No CRC32, >0: CRC32 OK, <0: error (CRC32 failed)
 * If CRC32 is OK, the result also gives information about the type of CRC32:
 * 1: CRC32 was Header only (only FSH_FLAGS_SECURE set)
 * 2: CRC32 was Image only (only FSH_FLAGS_CRC32 set)
 * 3: CRC32 was Header+Image (both FSH_FLAGS_SECURE and FSH_FLAGS_CRC32 set)
 */
int fs_image_check_crc32(struct fs_header_v1_0 *fsh)
{
	u32 expected_cs;
	u32 computed_cs;
	u32 *pcs;
	unsigned int size;
	unsigned char *start;
	int ret = 0;

	if (!(fsh->info.flags & (FSH_FLAGS_SECURE | FSH_FLAGS_CRC32)))
		return 0;		/* No CRC32 */

	if (fsh->info.flags & FSH_FLAGS_SECURE) {
		start = (unsigned char *)fsh;
		size = FSH_SIZE;
		ret |= 1;
	} else {
		start = (unsigned char *)(fsh + 1);
		size = 0;
	}

	if (fsh->info.flags & FSH_FLAGS_CRC32) {
		size += fs_image_get_size(fsh, false);
		ret |= 2;
	}

	/* CRC32 is in type[12..15]; temporarily set to 0 while computing */
	pcs = (u32 *)&fsh->type[12];
	expected_cs = *pcs;
	*pcs = 0;
	computed_cs = crc32(0, start, size);
	*pcs = expected_cs;
	if (computed_cs != expected_cs)
		return -EILSEQ;

	return ret;
}

/* Add the board revision as BOARD-ID to the given BOARD-CFG and update CRC32 */
void fs_image_board_cfg_set_board_rev(struct fs_header_v1_0 *cfg_fsh)
{
	u32 *pcs = (u32 *)&cfg_fsh->type[12];

	/* Set compare_id rev in file_size_high, compute new CRC32 */
	cfg_fsh->info.file_size_high = compare_bnr.rev;

	cfg_fsh->info.flags |= FSH_FLAGS_SECURE | FSH_FLAGS_CRC32;
	*pcs = 0;
	*pcs = crc32(0, (uchar *)cfg_fsh, fs_image_get_size(cfg_fsh, true));
}

/* Store current compare_id as board_id */
static void fs_image_set_board_id(void)
{
	char c;
	int i;

	/* Copy base name */
	for (i = 0; i < MAX_DESCR_LEN; i++) {
		c = compare_bnr.name[i];
		if (!c)
			break;
		board_id[i] = c;
	}

	/* Add board revision */
	snprintf(&board_id[i], MAX_DESCR_LEN - i, ".%03d", compare_bnr.rev);
	board_id[MAX_DESCR_LEN] = '\0';
}

/* Set the compare_id that will be used in fs_image_match_board_id() */
void fs_image_set_compare_id(const char id[MAX_DESCR_LEN])
{
	fs_image_get_board_name_rev(id, &compare_bnr);
}

/* Set the board_id and compare_id from the BOARD-CFG */
void fs_image_set_board_id_from_cfg(void)
{
	struct fs_header_v1_0 *cfg_fsh = fs_image_get_cfg_addr();
	unsigned int rev;

	/* Take base ID from descr; in old layout, this includes the rev */
	fs_image_get_board_name_rev(cfg_fsh->param.descr, &compare_bnr);

	/* The BOARD-ID rev is stored in file_size_high, 0 for old layout */
	rev = cfg_fsh->info.file_size_high;
	if (rev) {
		debug("Taking BOARD-ID rev from BOARD-CFG: %d\n", rev);
		compare_bnr.rev = rev;
	}

	fs_image_set_board_id();
}

/*
 * Make sure that BOARD-CFG in OCRAM is valid. This function is called early
 * in the boot_f phase of U-Boot, and therefore must not access any variables.
 */
bool fs_image_is_ocram_cfg_valid(void)
{
	struct fs_header_v1_0 *cfg_fsh = fs_image_get_cfg_addr();
	int err;

	if (!fs_image_match(cfg_fsh, "BOARD-CFG", NULL))
		return false;

	/*
	 * Check the additional CRC32. The BOARD-CFG in OCRAM also holds the
	 * BOARD-ID, which is not covered by the signature, but it is covered
	 * by the CRC32. So also do the check in case of Secure Boot.
	 *
	 * The BOARD-ID is given by the board-revision that is stored (as
	 * number) in unused entry file_size_high and is typically between 100
	 * and 999. This is the only part that may differ, the base name is
	 * always the same as of the ID of the BOARD-CFG itself (in descr).
	 */
	err = fs_image_check_crc32(cfg_fsh);
	if (err < 0)
		return false;

#ifdef CONFIG_FS_SECURE_BOOT
	{
		u16 flags;
		u32 file_size_high;
		u32 *pcs = (u32 *)&cfg_fsh->type[12];

		/*
		 * Signature was made without board-rev in file_size_high
		 * and without CRC32, so temporarily clear these values.
		 */
		file_size_high = cfg_fsh->info.file_size_high;
		cfg_fsh->info.file_size_high = 0;
		flags = cfg_fsh->info.flags;
		cfg_fsh->info.flags &= ~(FSH_FLAGS_SECURE | FSH_FLAGS_CRC32);
		crc32 = *pcs;
		*pcs = 0;

		err = imx_hab_authenticate_image((u32)cfg_fsh,
						 fs_image_get_size(cfg_fsh),
						 FSH_SIZE);

		/* Bring back the saved values */
		cfg_fsh->info.file_size_high = file_size_high;
		cfg_fsh->info.flags = flags;
		*pcs = crc32;

		if (err)
			return false;
	}
#endif

	return true;
}


/* ------------- Functions only in U-Boot, not SPL ------------------------- */

#ifndef CONFIG_SPL_BUILD

/* Return the current BOARD-ID */
const char *fs_image_get_board_id(void)
{
	return board_id;
}

/*
 * Return address of board configuration in OCRAM; search for it if not
 * available at expected place. This function is called early in boot_f phase
 * of U-Boot and must not access any variables. Use global data instead.
 */
bool fs_image_find_cfg_in_ocram(void)
{
	struct fs_header_v1_0 *fsh;
	const char *type = "BOARD-CFG";

	/* Try expected location first */
	fsh = fs_image_get_regular_cfg_addr();
	if (fs_image_match(fsh, type, NULL))
		return true;

	/*
	 * Search it from beginning of OCRAM.
	 *
	 * To avoid having to search this location over and over again, save a
	 * pointer to it in global data.
	 */
	fsh = (struct fs_header_v1_0 *)CONFIG_SYS_OCRAM_BASE;
	do {
		if (fs_image_match(fsh, type, NULL)) {
			gd->board_cfg = (ulong)fsh;
			return true;
		}
		fsh++;
	} while ((ulong)fsh < (CONFIG_SYS_OCRAM_BASE + CONFIG_SYS_OCRAM_SIZE));

	return false;
}

#endif /* !CONFIG_SPL_BUILD */

/* ------------- Functions only in SPL, not U-Boot ------------------------- */

#ifdef CONFIG_SPL_BUILD

/* Jobs to do when streaming image data */
#define FSIMG_JOB_CFG BIT(0)
#define FSIMG_JOB_DRAM BIT(1)
#define FSIMG_JOB_ATF BIT(2)
#define FSIMG_JOB_TEE BIT(3)
#ifdef CONFIG_IMX_OPTEE
#define FSIMG_FW_JOBS (FSIMG_JOB_DRAM | FSIMG_JOB_ATF | FSIMG_JOB_TEE)
#else
#define FSIMG_FW_JOBS (FSIMG_JOB_DRAM | FSIMG_JOB_ATF)
#endif

/* Load mode */
enum fsimg_mode {
	FSIMG_MODE_HEADER,		/* Loading F&S header */
	FSIMG_MODE_IMAGE,		/* Loading F&S image */
	FSIMG_MODE_SKIP,		/* Skipping data */
	FSIMG_MODE_DONE,		/* F&S image done */
};

static enum fsimg_state {
	FSIMG_STATE_ANY,
	FSIMG_STATE_BOARD_CFG,
	FSIMG_STATE_DRAM,
	FSIMG_STATE_DRAM_TYPE,
	FSIMG_STATE_DRAM_FW,
	FSIMG_STATE_DRAM_TIMING,
	FSIMG_STATE_ATF,
	FSIMG_STATE_TEE,
} state;

/* Function to load a chunk of data frome either NAND or MMC */
typedef int (*load_function_t)(uint32_t offs, unsigned int size, void *buf);

static enum fsimg_mode mode;
static unsigned int count;
static void *addr;
static unsigned int jobs;
static int nest_level;
static const char *ram_type;
static const char *ram_timing;
static basic_init_t basic_init_callback;
static struct fs_header_v1_0 one_fsh;	/* Buffer for one F&S header */
#ifdef CONFIG_FS_SECURE_BOOT
static void *final_load_addr = 0;
static char* state_names[] = { "Board-Config", "", "", "", "DRAM-Firmware",
						"DRAM Timing", "ATF", "TEE" };
#endif

#define MAX_NEST_LEVEL 8

static struct fsimg {
	unsigned int size;		/* Size of the main image data */
	unsigned int remaining;		/* Remaining bytes in this level */
} fsimg_stack[MAX_NEST_LEVEL];

#define reloc(addr, offs) addr = ((void *)addr + (unsigned long)offs)

/* Relocate dram_timing_info structure and initialize DRAM */
static int fs_image_init_dram(void)
{
	unsigned long *p;

	/* Before we can init DRAM, we have to init the board config */
	basic_init_callback();

	/* The image starts with a pointer to the dram_timing variable */
	p = (unsigned long *)CONFIG_SPL_DRAM_TIMING_ADDR;

	return !fs_dram_init_common(p);
}

/* State machine: Load next header in sub-image */
static void fs_image_next_header(enum fsimg_state new_state)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	state = new_state;
	count = FSH_SIZE;
	if (count > fsimg->remaining) {
		/* Image too small, no room for F&S header, skip */
		count = fsimg->remaining;
		fsimg->remaining = 0;
		mode = FSIMG_MODE_SKIP;
	} else {
		/* Load F&S header */
		addr = &one_fsh;
		fsimg->remaining -= count;
		mode = FSIMG_MODE_HEADER;
	}
}

/* State machine: Enter a new sub-image with given size and load the header */
static void fs_image_enter(unsigned int size, enum fsimg_state new_state)
{
	fsimg_stack[nest_level].remaining -= size;
	fsimg_stack[++nest_level].remaining = size;
	fsimg_stack[nest_level].size = size;
	fs_image_next_header(new_state);
}

/* State machine: Load data of given size to given address, go to new state */
static void fs_image_copy(void *buf, unsigned int size)
{
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	addr = buf;
	mode = FSIMG_MODE_IMAGE;
}

/* State machine: Skip data of given size */
static void fs_image_skip(unsigned int size)
{
	debug("%d: skip=0x%x, state=0x%x\n", nest_level, size, state);
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	mode = FSIMG_MODE_SKIP;
}

/* State machine: If match, load, otherwise skip this sub-image */
static void fs_image_copy_or_skip(struct fs_header_v1_0 *fsh,
				  const char *type, const char *descr,
				  void *addr, unsigned int size)
{
	if (fs_image_match(fsh, type, descr))
		fs_image_copy(addr, size);
	else
		fs_image_skip(size);
}

/* State machine: Get the next FIRMWARE job */
static enum fsimg_state fs_image_get_fw_state(void)
{
	if (jobs & FSIMG_JOB_DRAM)
		return FSIMG_STATE_DRAM;
	if (jobs & FSIMG_JOB_ATF)
		return FSIMG_STATE_ATF;
	if (jobs & FSIMG_JOB_TEE)
		return FSIMG_STATE_TEE;

	return FSIMG_STATE_ANY;
}

/* State machine: Switch to next FIRMWARE state or skip remaining images */
static void fs_image_next_fw(void)
{
	enum fsimg_state next = fs_image_get_fw_state();

	if (next != FSIMG_STATE_ANY)
		fs_image_next_header(next);
	else {
		state = next;
		fs_image_skip(fsimg_stack[nest_level].remaining);
	}
}

#ifdef CONFIG_FS_SECURE_BOOT
static int check_fs_image(struct sb_info info, char* name, bool isSigned) {
	if(isSigned) {
		struct fs_header_v1_0* fsh = info.check_addr;
		int size = fs_image_get_size(fsh, false) + 0x40;
		uintptr_t check_ptr = (ulong)info.check_addr;

		if (imx_hab_authenticate_image(check_ptr, size, FSH_SIZE)) {
			printf("ERROR: %s IS NOT VALID\n", name);
			return 1;
		}
#ifdef CONFIG_FS_SECURE_BOOT_DEBUG
		printf("signed %s\n", name);
#endif
	}
	else if(imx_hab_is_enabled()) {
		printf("ERROR: UNSIGNED %s ON CLOSED BOARD\n", name);
		return 1;
	}
	else {
#ifdef CONFIG_FS_SECURE_BOOT_DEBUG
		printf("unsigned %s\n", name);
#endif
	}
	return 0;
}

static int auth_prepare(struct sb_info info, size_t* image_length,
				char **name, ulong* offset, bool *isSigned) {
	*name = state_names[info.image_type];
	*isSigned = (info.image_ivt->hdr.magic == IVT_HEADER_MAGIC)
			? true : false;

	if(*isSigned) {
		uintptr_t bd_ptr = (ulong)info.image_ivt->boot;
		struct boot_data * bd = (struct boot_data*)bd_ptr;
		*image_length = bd->length;
		*offset = HAB_HEADER;
	}
	else
	{
		struct fs_header_v1_0* fsh = info.check_addr;
		*image_length = fs_image_get_size(fsh, false);
		*offset = 0;
	}
	return 0;
}

static int copy_valid_fs_image(struct sb_info info, size_t img_length,
							ulong offset) {
	void  *img_dst  = info.final_addr;
	void  *img_src  = info.check_addr + FSH_SIZE + offset;

	if (info.header == true) {
		memcpy(info.final_addr, info.check_addr, FSH_SIZE);
		img_dst += FSH_SIZE;
	}

	memcpy(img_dst, img_src, img_length);
	return 0;
}

int copy_if_valid(void *final_addr, void *check_addr, ulong ivt_offset,
						int image_type, bool header) {
	size_t image_length = 0;
	ulong offset = 0;
	char* name = "";
	bool isSigned;

	struct sb_info info = {
		.final_addr = final_addr,
		.check_addr = check_addr,
		.image_ivt = (struct ivt*)ivt_offset,
		.header = header,
		.image_type = image_type
	};

	auth_prepare(info, &image_length, &name, &offset, &isSigned);
	if (check_fs_image(info, name, isSigned))
		return 1;

	copy_valid_fs_image(info, image_length, offset);
	return 0;
}
#endif

/* State machine: Loading the F&S header of the (sub-)image is done */
static void fs_image_handle_header(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];
	unsigned int size;
	const char *arch;

	/* Check if magic is correct */
	if (!fs_image_is_fs_image(&one_fsh)) {
		/* This is no F&S header; skip remaining image */
		fs_image_skip(fsimg->remaining);
		return;
	}

	/* Get the image size (incuding padding) */
	size = fs_image_get_size(&one_fsh, false);

	/* Fill in size on topmost level, if we did not know it (NAND, MMC) */
	if (!nest_level && !fsimg->remaining)
		fsimg->remaining = size;

	debug("%d: Found %s, size=0x%x remaining=0x%x state=%d\n",
	      nest_level, one_fsh.type, size, fsimg->remaining, state);

	arch = fs_image_get_arch();
	switch (state) {
	case FSIMG_STATE_ANY:
		if (fs_image_match(&one_fsh, "BOARD-ID", NULL)) {
			/* Save ID and add job to load BOARD-CFG */
			fs_image_set_compare_id(one_fsh.param.descr);
			fs_image_set_board_id();
			jobs |= FSIMG_JOB_CFG;
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&one_fsh, "NBOOT", arch)) {
			/* Simply enter image, no further action */
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&one_fsh, "BOARD-INFO", arch)) {
			fs_image_enter(size, FSIMG_STATE_BOARD_CFG);
			break;
		} else if (fs_image_match(&one_fsh, "FIRMWARE", arch)
			   && !(jobs & FSIMG_JOB_CFG)) {
			enum fsimg_state next = fs_image_get_fw_state();

			if (next != FSIMG_STATE_ANY) {
				fs_image_enter(size, next);
				break;
			}
		}

		/* Skip unknown or unneeded images */
		fs_image_skip(size);
		break;

	case FSIMG_STATE_BOARD_CFG:
		if (fs_image_match_board_id(&one_fsh)) {
#ifndef CONFIG_FS_SECURE_BOOT
			memcpy(fs_image_get_cfg_addr(), &one_fsh, FSH_SIZE);
			fs_image_copy(fs_image_get_cfg_addr() + FSH_SIZE, size);
#else
			fs_image_copy((void*)(CONFIG_SPL_ATF_ADDR) + FSH_SIZE,
				      size);
			final_load_addr = fs_image_get_cfg_addr();
#endif
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM:
		/* Get DRAM type and DRAM timing from BOARD-CFG */
		if (fs_image_match(&one_fsh, "DRAM-INFO", arch)) {
			void *fdt = fs_image_get_cfg_fdt();
			int off = fs_image_get_board_cfg_offs(fdt);

			ram_type = fdt_getprop(fdt, off, "dram-type", NULL);
			ram_timing = fdt_getprop(fdt, off, "dram-timing", NULL);

			debug("Looking for: %s, %s\n", ram_type, ram_timing);

			fs_image_enter(size, FSIMG_STATE_DRAM_TYPE);
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		if (fs_image_match(&one_fsh, "DRAM-TYPE", ram_type))
#ifdef CONFIG_IMX8
			fs_image_enter(size, FSIMG_STATE_DRAM_TIMING);
#else
			fs_image_enter(size, FSIMG_STATE_DRAM_FW);
#endif
		else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* Load DDR training firmware behind SPL code */
#ifndef CONFIG_FS_SECURE_BOOT
		fs_image_copy_or_skip(&one_fsh, "DRAM-FW", ram_type,  &_end,
				      size);
#else
		fs_image_copy_or_skip(&one_fsh, "DRAM-FW", ram_type,  
				      (void*)(CONFIG_SPL_ATF_ADDR + FSH_SIZE),
				      size);
		final_load_addr = &_end;
#endif
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* This may overlap ATF */
#ifndef CONFIG_FS_SECURE_BOOT
		fs_image_copy_or_skip(&one_fsh, "DRAM-TIMING", ram_timing,
				      (void *)CONFIG_SPL_DRAM_TIMING_ADDR,
				      size);
#else
		fs_image_copy_or_skip(&one_fsh, "DRAM-TIMING", ram_timing,
				      (void *)CONFIG_SPL_ATF_ADDR + FSH_SIZE,
				      size);
		final_load_addr = (void*)CONFIG_SPL_DRAM_TIMING_ADDR;
#endif
		break;

	case FSIMG_STATE_ATF:
#ifndef CONFIG_FS_SECURE_BOOT
		fs_image_copy_or_skip(&one_fsh, "ATF", arch,
			(void *)(CONFIG_SPL_ATF_ADDR), size);
#else
		fs_image_copy_or_skip(&one_fsh, "ATF", arch,
			(void *)CONFIG_SPL_ATF_ADDR + FSH_SIZE, size);
		final_load_addr = (void *)CONFIG_SPL_ATF_ADDR;
#endif
		break;

	case FSIMG_STATE_TEE:
#ifndef CONFIG_FS_SECURE_BOOT
		fs_image_copy_or_skip(&one_fsh, "TEE", arch,
				      (void *)CONFIG_SPL_TEE_ADDR, size);
#else
		fs_image_copy_or_skip(&one_fsh, "TEE", arch,
			(void *)CONFIG_SPL_TEE_ADDR + FSH_SIZE, size);
		final_load_addr = (void *)CONFIG_SPL_TEE_ADDR;
#endif
		break;
	}
}

/* State machine: Loading the data part of a sub-image is complete */
static void fs_image_handle_image(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	switch (state) {
	case FSIMG_STATE_ANY:
	case FSIMG_STATE_DRAM:
	case FSIMG_STATE_DRAM_TYPE:
		/* Should not happen, we do not load these images */
		break;

	case FSIMG_STATE_BOARD_CFG:
		/* We have our config, skip remaining configs */
		debug("Got BOARD-CFG, ID=%s\n", board_id);
		fs_image_board_cfg_set_board_rev(fs_image_get_cfg_addr());
		jobs &= ~FSIMG_JOB_CFG;
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* DRAM training firmware loaded, now look for DRAM timing */
		debug("Got DRAM-FW (%s)\n", ram_type);
		fs_image_next_header(FSIMG_STATE_DRAM_TIMING);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* DRAM info complete, start it; job done if successful */
		debug("Got DRAM-TIMING (%s)\n", ram_timing);
		if (fs_image_init_dram())
			jobs &= ~FSIMG_JOB_DRAM;
		else
			debug("Init DDR failed\n");

		/* Skip remaining DRAM timings */
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_ATF:
		/* ATF loaded, job done */
		debug("Got ATF\n");
		jobs &= ~FSIMG_JOB_ATF;
		fs_image_next_fw();
		break;

	case FSIMG_STATE_TEE:
		/* TEE loaded, job done */
		debug("Got TEE\n");
		jobs &= ~FSIMG_JOB_TEE;
		fs_image_next_fw();
		break;
	}
}

/* State machine: Skipping a part of a sub-image is complete */
static void fs_image_handle_skip(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	if (fsimg->remaining) {
		debug("%d: skip: remaining=0x%x state=0x%x\n",
		      nest_level, fsimg->remaining, state);
		fs_image_next_header(state);
		return;
	}

	if (!nest_level) {
		mode = FSIMG_MODE_DONE;
		return;
	}

	nest_level--;
	fsimg--;

	switch (state) {
	case FSIMG_STATE_ANY:
		fs_image_next_header(state);
		break;

	case FSIMG_STATE_BOARD_CFG:
		fs_image_next_header(FSIMG_STATE_ANY);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		fs_image_next_fw();
		break;

	case FSIMG_STATE_DRAM_FW:
	case FSIMG_STATE_DRAM_TIMING:
		state = FSIMG_STATE_DRAM_TYPE;
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM:
	case FSIMG_STATE_ATF:
	case FSIMG_STATE_TEE:
		fs_image_next_header(FSIMG_STATE_ANY);
		break;
	}
}

/* State machine: Handle the next part of the image when data is loaded */
static void fs_image_handle(void)
{
	switch (mode) {
	case FSIMG_MODE_HEADER:
		fs_image_handle_header();
		break;

	case FSIMG_MODE_IMAGE:
		fs_image_handle_image();
		break;

	case FSIMG_MODE_SKIP:
		fs_image_handle_skip();
		break;

	case FSIMG_MODE_DONE:
		/* Should not happen, caller has to drop all incoming data
		   when mode is FSIMG_MODE_DONE */
		break;
	}
}

/* Start state machine for a new F&S image */
static void fs_image_start(unsigned int size, unsigned int jobs_todo,
			   basic_init_t basic_init)
{
	jobs = jobs_todo;
	basic_init_callback = basic_init;
	nest_level = -1;
	fs_image_enter(size, FSIMG_STATE_ANY);
}

#ifdef CONFIG_FS_SECURE_BOOT
static bool ready_to_move_header(void) {
	if ((mode == FSIMG_MODE_HEADER) && ((state == FSIMG_STATE_DRAM_FW) ||
					(state == FSIMG_STATE_DRAM_TIMING) ||
						(state == FSIMG_STATE_ATF) ||
						(state == FSIMG_STATE_TEE) ||
			(fs_image_match_board_id(&one_fsh)))) {
		return true;
	}
	return false;
}

static void move_header(void){
	if(state == FSIMG_STATE_TEE)
		memcpy((void*)CONFIG_SPL_TEE_ADDR, &one_fsh, FSH_SIZE);
	else
		memcpy((void*)CONFIG_SPL_ATF_ADDR, &one_fsh, FSH_SIZE);
}

static bool ready_to_move_image(bool eq) {
	if((mode == FSIMG_MODE_IMAGE) && eq)
		return true;
	return false;
}

void move_image_and_validate(void) {
	bool header = false;
	uintptr_t test_addr = CONFIG_SPL_ATF_ADDR;

	if(fs_image_match_board_id(&one_fsh))
		header = true;
	if(state == FSIMG_STATE_TEE)
		test_addr = CONFIG_SPL_TEE_ADDR;

	if(copy_if_valid(final_load_addr, (void *)(uintptr_t)test_addr,
				test_addr + FSH_SIZE, state, header))
		hang();
}
#endif

/* Handle a chunk of data that was received via SDP on USB */
static void fs_image_sdp_rx_data(u8 *data_buf, int data_len)
{
	unsigned int chunk;

	/* We have data_len bytes, we need count bytes (which may be zero) */
	while ((data_len > 0) && (mode != FSIMG_MODE_DONE)) {
		chunk = min((unsigned int)data_len, count);
		if ((mode == FSIMG_MODE_IMAGE) || (mode == FSIMG_MODE_HEADER))
			memcpy(addr, data_buf, chunk);

#ifdef CONFIG_FS_SECURE_BOOT
		/* For Secure Boot additional steps are nedded, do it here */
		if (ready_to_move_header())
			move_header();
		if (ready_to_move_image(count == chunk))
			move_image_and_validate();
#endif
		addr += chunk;
		data_buf += chunk;
		data_len -= chunk;
		count -= chunk;

		/* The next block for the interpreter is loaded, process it */
		while (!count && (mode != FSIMG_MODE_DONE))
			fs_image_handle();
	}
}

/* This is called when the SDP protocol starts a new file */
static void fs_image_sdp_new_file(u32 dnl_address, u32 size)
{
	fs_image_start(size, jobs, basic_init_callback);
}

static const struct sdp_stream_ops fs_image_sdp_stream_ops = {
	.new_file = fs_image_sdp_new_file,
	.rx_data = fs_image_sdp_rx_data,
};

/* Load FIRMWARE and optionally BOARD-CFG via SDP from USB */
void fs_image_all_sdp(bool need_cfg, basic_init_t basic_init)
{
	unsigned int jobs_todo = FSIMG_FW_JOBS;

	if (need_cfg)
		jobs |= FSIMG_JOB_CFG;

	jobs = jobs_todo;
	basic_init_callback = basic_init;

	/* Stream the file and load appropriate parts */
	spl_sdp_stream_image(&fs_image_sdp_stream_ops, true);

	/* Stream until a valid NBoot with all jobs was downloaded */
	while (jobs) {
		debug("Jobs not done: 0x%x\n", jobs);
		jobs = jobs_todo;
		spl_sdp_stream_continue(&fs_image_sdp_stream_ops, true);
	};
}

#ifdef CONFIG_MMC

/* Load MMC data from arbitrary offsets, not necessarily MMC block aligned */
static int fs_image_gen_load_mmc(uint32_t offs, unsigned int size, void *buf)
{
	unsigned long n;
	unsigned int chunk_offs;
	unsigned int chunk_size;
	static u8 *local_buffer;	/* Space for one MMC block */
	struct mmc *mmc;
	struct blk_desc *blk_desc;
	unsigned long blksz;
	int err;
	unsigned int cur_part, boot_part;

	mmc = find_mmc_device(0);
	if (!mmc) {
		puts("MMC boot device not found\n");
		return -ENODEV;
	}
	err = mmc_init(mmc);
	if (err) {
		printf("mmc_init() failed (%d)\n", err);
		return err;
	}
	blk_desc = mmc_get_blk_desc(mmc);
	blksz = blk_desc->blksz;

	/* We need a buffer for one MMC block; only allocate once */
	if (!local_buffer) {
		local_buffer = malloc(blksz);
		if (!local_buffer) {
			puts("Can not allocate local buffer for MMC\n");
			return -ENOMEM;
		}
	}

	/* Select partition where system boots from */
	cur_part = blk_desc->hwpart;
	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;
	if (cur_part != boot_part) {
		err = blk_dselect_hwpart(blk_desc, boot_part);
		if (err) {
			printf("Cannot switch to part %d on mmc0\n", boot_part);
			return err;
		}
	}

	chunk_offs = offs % blksz;
	offs /= blksz;			/* From now on offs is in MMC blocks */

	/*
	 * If not starting from an MMC block boundary, load one block to local
	 * buffer and take some bytes from the end to get aligned.
	 */
	if (chunk_offs) {
		chunk_size = blksz - chunk_offs;
		if (chunk_size > size)
			chunk_size = size;
		n = blk_dread(blk_desc, offs, 1, local_buffer);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < 1)
			return -EIO;
		memcpy(buf, local_buffer + chunk_offs, chunk_size);
		offs++;
		buf += chunk_size;
		size -= chunk_size;
	}

	/*
	 * Load full blocks directly to target address. This assumes that buf
	 * is 32 bit aligned all the time. Our F&S images are always padded to
	 * 16 bytes, so this should be no problem.
	 */
	if (size >= blksz) {
		if ((unsigned long)buf & 3)
			puts("### Aaargh! buf not 32-bit aligned!\n");
		chunk_size = size / blksz;
		n = blk_dread(blk_desc, offs, chunk_size, buf);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < chunk_size)
			return -EIO;
		offs += chunk_size;
		chunk_size *= blksz;
		buf += chunk_size;
		size -= chunk_size;
	}

	/*
	 * If there are some bytes remaining, load one block to local buffer
	 * and take these bytes from there.
	 */
	if (size > 0) {
		n = blk_dread(blk_desc, offs, 1, local_buffer);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < 1)
			return -EIO;
		memcpy(buf, local_buffer, size);
	}

	return 0;
}

#endif /* CONFIG_MMC */

/* Load FIRMWARE from MMC/NAND using state machine */
static int fs_image_loop(struct fs_header_v1_0 *cfg, unsigned int start,
			 load_function_t load)
{
	int err;
	unsigned int end;
	void *fdt = cfg + 1;
	int offs = fs_image_get_nboot_info_offs(fdt);
	unsigned int board_cfg_size, nboot_size;

	if (!fdt)
		return -EINVAL;
        board_cfg_size = fdt_getprop_u32_default_node(fdt, offs, 0,
						      "board-cfg-size", 0);
	if (!board_cfg_size)
		return -EINVAL;
	nboot_size = fdt_getprop_u32_default_node(fdt, offs, 0, "nboot-size", 0);
	if (!nboot_size)
		return -EINVAL;

	end = start + nboot_size;
	start += board_cfg_size;

	/*
	 * ### TODO: Handle (=skip) bad blocks in case of NAND (if load ==
	 * nand_spl_load_image) Basic idea: nand_spl_load_image() already
	 * skips bad blocks. So when incrementing offs, check the region for
	 * bad blocks and increase offs again according to the number of
	 * bad blocks in the region. Problem: we do not have info about NAND
	 * here like block sizes, so maybe have such a function in NAND driver.
	 */
	do {
		if (count) {
			if ((mode == FSIMG_MODE_IMAGE)
			    || (mode == FSIMG_MODE_HEADER)) {
				if (start + count >= end)
					return -EFBIG;
				err = load(start, count, addr);
#ifdef CONFIG_FS_SECURE_BOOT
				if (ready_to_move_header())
					move_header();
				if (ready_to_move_image(true))
					move_image_and_validate();
#endif
				if (err)
					return err;
				addr += count;
			}
			start += count;
		}
		fs_image_handle();
	} while (mode != FSIMG_MODE_DONE);

	return 0;
}

/* Load the BOARD-CFG and (if basic_init is not NULL) the FIRMWARE */
int fs_image_load_system(enum boot_device boot_dev, bool secondary,
			 basic_init_t basic_init)
{
	int copy, start_copy;
	load_function_t load;
	unsigned int offs[2];
	unsigned int start;
	void *target = fs_image_get_cfg_addr();

	switch (boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		offs[0] = CONFIG_FUS_BOARDCFG_NAND0;
		offs[1] = CONFIG_FUS_BOARDCFG_NAND1;
		load = nand_spl_load_image;
		break;
#endif
#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC3_BOOT:
		offs[0] = CONFIG_FUS_BOARDCFG_MMC0;
		offs[1] = CONFIG_FUS_BOARDCFG_MMC1;
		load = fs_image_gen_load_mmc;
		break;
#endif
	default:
		return -ENODEV;
	}

	/* Try both copies (second copy first if running on secondary SPL) */
	start_copy = secondary ? 1 : 0;
	copy = start_copy;
	do {
		start = offs[copy];

		/* Try to load BOARD-CFG (normal load) */
		if (!load(start, FSH_SIZE, &one_fsh)
		    && fs_image_match(&one_fsh, "BOARD-CFG", NULL)
		    && !load(start, fs_image_get_size(&one_fsh, true), target)
		    && fs_image_is_ocram_cfg_valid())
 		{
			fs_image_set_board_id_from_cfg();

			/* basic_init == NULL means only load BOARD-CFG */
			if (!basic_init)
				return 0;

			/* Try to load FIRMWARE (with state machine) */
			fs_image_start(FSH_SIZE, FSIMG_FW_JOBS, basic_init);
			if (!fs_image_loop(target, start, load) && !jobs)
				return 0;
		}

		/* No, did not work, try other copy */
		copy = 1 - copy;
	} while (copy != start_copy);

	return -ENOENT;
}

#endif /* CONFIG_SPL_BUILD */

/*
 * fs_fdt_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common FDT access and manipulation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

#ifdef CONFIG_OF_BOARD_SETUP

#include <common.h>			/* types, get_board_name(), ... */
#include <version.h>			/* version_string[] */
#include <cli.h>			/* get_board_name() */
#include <net.h>			/* eth_env_get_enetaddr_by_index() */
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */
#include <asm/arch/sys_proto.h>		/* get_reset_cause() */
#include "fs_fdt_common.h"		/* Own interface */
#include "fs_board_common.h"		/* fs_board_get_nboot_args() */
#include "fs_image_common.h"	/* fs_image_*() */
#ifdef CONFIG_FS_SELFTEST
#include "fs_processor_info.h"	/* fs_get_processorInfo() */
#endif

/* Set a generic value, if it was not already set in the device tree */
void fs_fdt_set_val(void *fdt, int offs, const char *name, const void *val,
		    int len, int force)
{
	int err;

	/* Keep value if "no-uboot-override" is set */
	if (fdt_get_property(fdt, offs, "no-uboot-override", NULL) != NULL)
		force = 0;

	/* Warn if property already exists in device tree */
	if (fdt_get_property(fdt, offs, name, NULL) != NULL) {
		printf("## %s property %s/%s from device tree!\n",
		       force ? "Overwriting": "Keeping",
		       fdt_get_name(fdt, offs, NULL), name);
		if (!force)
			return;
	}

	err = fdt_setprop(fdt, offs, name, val, len);
	if (err) {
		printf("## Unable to update property %s/%s: err=%s\n",
		       fdt_get_name(fdt, offs, NULL), name, fdt_strerror(err));
	}
}

/* Set a string value */
void fs_fdt_set_string(void *fdt, int offs, const char *name, const char *str,
		       int force)
{
	fs_fdt_set_val(fdt, offs, name, str, strlen(str) + 1, force);
}

/* Set a u32 value as a string (usually for bdinfo) */
void fs_fdt_set_u32str(void *fdt, int offs, const char *name, u32 val,
		       int force)
{
	char str[12];

	sprintf(str, "%u", val);
	fs_fdt_set_string(fdt, offs, name, str, force);
}

/* Set a u32 value */
void fs_fdt_set_u32(void *fdt, int offs, const char *name, u32 val, int force)
{
	fdt32_t tmp = cpu_to_fdt32(val);

	fs_fdt_set_val(fdt, offs, name, &tmp, sizeof(tmp), force);
}

/* Set ethernet MAC address aa:bb:cc:dd:ee:ff for given index */
void fs_fdt_set_macaddr(void *fdt, int offs, int id)
{
	uchar enetaddr[6];
	char name[10];
	char str[20];

	if (eth_env_get_enetaddr_by_index("eth", id, enetaddr)) {
		sprintf(name, "MAC%d", id);
		sprintf(str, "%pM", enetaddr);
		fs_fdt_set_string(fdt, offs, name, str, 1);
	}
}

/* Set MAC address in bdinfo as MAC_WLAN and in case of Silex as Silex-MAC */
void fs_fdt_set_wlan_macaddr(void *fdt, int offs, int id, int silex)
{
	uchar enetaddr[6];
	char str[30];

	if (eth_env_get_enetaddr_by_index("eth", id, enetaddr)) {
		sprintf(str, "%pM", enetaddr);
		fs_fdt_set_string(fdt, offs, "MAC_WLAN", str, 1);
		if (silex) {
			sprintf(str, "Intf0MacAddress=%02X%02X%02X%02X%02X%02X",
				enetaddr[0], enetaddr[1], enetaddr[2],
				enetaddr[3], enetaddr[4], enetaddr[5]);
			fs_fdt_set_string(fdt, offs, "Silex-MAC", str, 1);
		}
	}
}

/* If environment variable exists, set a string property with the same name */
void fs_fdt_set_getenv(void *fdt, int offs, const char *name, int force)
{
	const char *str;

	str = env_get(name);
	if (str)
		fs_fdt_set_string(fdt, offs, name, str, force);
}

/* Open a node, warn if the node does not exist */
int fs_fdt_path_offset(void *fdt, const char *path)
{
	int offs;

	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		printf("## Can not access node %s: err=%s\n",
		       path, fdt_strerror(offs));
	}

	return offs;
}

/* Enable or disable node given by path, overwrite any existing status value */
int fs_fdt_enable(void *fdt, const char *path, int enable)
{
	int offs, err, len;
	const void *val;
	char *str = enable ? "okay" : "disabled";

	offs = fdt_path_offset(fdt, path);
	if (offs < 0)
		return offs;

	/* Do not change if status already exists and has this value */
	val = fdt_getprop(fdt, offs, "status", &len);
	if (val && len && !strcmp(val, str))
		return -1;

	/* No, set new value */
	err = fdt_setprop_string(fdt, offs, "status", str);
	if (err) {
		printf("## Can not set status of node %s: err=%s\n",
		       path, fdt_strerror(err));
		return err;
	}

	return  0;
}

/* Store common board specific values in node bdinfo */
void fs_fdt_set_bdinfo(void *fdt, int offs)
{
	char rev[6];
	unsigned int board_rev = fs_board_get_rev();

	/* Add board-config to bdinfo node */
#ifdef CONFIG_FUS_BOARDCFG_ADDR
	void *fdt_cfg = fs_image_get_cfg_fdt();
	int offs_cfg = fs_image_get_board_cfg_offs(fdt_cfg);
	int offs_bdinfo_cfg = fdt_add_subnode(fdt, offs, "board-cfg");
	fdt_overlay_apply_node(fdt, offs_bdinfo_cfg, fdt_cfg, offs_cfg);

	fs_image_set_board_id_from_cfg();
	fs_fdt_set_string(fdt, offs, "board-id", fs_image_get_board_id(), 1);
#endif

#ifdef CONFIG_FS_SELFTEST
	char cpu_info[128];
	fs_get_processorInfo(cpu_info);
	fs_fdt_set_string(fdt, offs, "cpu_info", cpu_info, 1);
	fs_fdt_set_string(fdt, offs, "dram_test_result", get_dram_result(), 1);
#endif

#ifndef CONFIG_FS_BOARD_CFG
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();

	/* NAND info, names and features */
#ifdef CONFIG_TARGET_FSVYBRID
	static unsigned char ecc_strength[8] = {0, 4, 6, 8, 12, 16, 24, 32};

	fs_fdt_set_u32str(fdt, offs, "ecc_mode", pargs->chECCtype, 1);
	fs_fdt_set_u32str(fdt, offs, "ecc_strength",
			  ecc_strength[pargs->chECCtype], 1);
#else
	fs_fdt_set_u32str(fdt, offs, "ecc_strength", pargs->chECCtype, 1);
#endif
	fs_fdt_set_u32str(fdt, offs, "features1", pargs->chFeatures1, 1);
	fs_fdt_set_u32str(fdt, offs, "features2", pargs->chFeatures2, 1);
	fs_fdt_set_u32str(fdt, offs, "nand_state", pargs->chECCstate, 1);
#endif /* !CONFIG_FS_BOARD_CFG */
	fs_fdt_set_string(fdt, offs, "boot_dev",
		fs_board_get_name_from_boot_dev(fs_board_get_boot_dev()), 1);
	fs_fdt_set_string(fdt, offs, "board_name", get_board_name(), 0);
	sprintf(rev, "%d.%02d", board_rev / 100, board_rev % 100);
	fs_fdt_set_string(fdt, offs, "board_revision", rev, 1);
	fs_fdt_set_getenv(fdt, offs, "platform", 0);
	fs_fdt_set_getenv(fdt, offs, "arch", 1);
#ifndef CONFIG_ARCH_IMX8
	fs_fdt_set_string(fdt, offs, "reset_cause", get_reset_cause(), 1);
#endif
	fs_fdt_set_string(fdt, offs, "nboot_version",
			  fs_board_get_nboot_version(), 1);
	fs_fdt_set_string(fdt, offs, "u-boot_version", version_string, 1);
}

#endif /* CONFIG_OF_BOARD_SETUP */

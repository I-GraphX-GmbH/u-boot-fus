/*
 * Copyright (C) 2017 F&S Elektronik Systeme GmbH
 * Copyright (C) 2018-2019 F&S Elektronik Systeme GmbH
 * Copyright (C) 2021 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX8MP. This is
 * PicoCoreMX8MP.
 *
 * Activate with one of the following targets:
 *   make fsimx8mp_defconfig   Configure for i.MX8MP boards
 *   make                     Build uboot-spl.bin, u-boot.bin and u-boot-nodtb.bin.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
#define OCRAM_BASE		0x900000

 * TCM layout (SPL)
 * ----------------
 * unused
 *
 * OCRAM layout SPL                 U-Boot
 * ---------------------------------------------------------
 * 0x0090_0000: (Region reserved by ROM loader)(96KB)
 * 0x0091_8000: BOARD-CFG           BOARD-CFG (8KB)  CONFIG_FUS_BOARDCFG_ADDR
 * 0x0091_A000: BSS data            cfg_info  (2KB)  CONFIG_SPL_BSS_START_ADDR
 * 0x0091_A800: MALLOC_F pool       ---       (34KB) CONFIG_MALLOC_F_ADDR
 * 0x0092_3000: ---                 ---       (4KB)
 * 0x0092_7FF0: Global Data + Stack ---       (20KB) CONFIG_SPL_STACK
 * 0x0092_8000: SPL (<= ~208KB) (loaded by ROM-Loader, address defined by ATF)
 *     DRAM-FW: Training Firmware (up to 96KB, immediately behind end of SPL)
 * 0x0096_4000: DRAM Timing Data    ---       (16KB) CONFIG_SPL_DRAM_TIMING_ADDR
 * 0x0096_8000: ATF (8MP)           ATF       (96KB) CONFIG_SPL_ATF_ADDR
 * 0x0098_FFFF: END (8MP) #####!!!!!##### (MP hat 576KB OCRAM, nicht nur 512KB)
 *
 * The sum of SPL and DDR_FW must not exceed 240KB (0x3C000). However there is
 * still room to extend this region if SPL grows larger in the future, e.g. by
 * letting DRAM Timing Data overlap with ATF region.
 * After DRAM is available, SPL uses a MALLOC_R pool at 0x4220_0000.
 *
 * NAND flash layout
 * -----------------
 * There are no boards with NAND flash available. If we ever build one, we can
 * use settings similar to fsimx8mn.
 *
 * eMMC Layout
 * -----------
 * The boot process from eMMC can be configured to boot from a Boot partition
 * or from the User partition. In the latter case, there needs to be a reserved
 * area of 8MB at the beginning of the User partition.
 *
 * 0x0000_0000: Space for GPT (32KB)
 * 0x0000_8000: NBoot (see nboot/nboot-info.dtsi for details)
 * 0x0080_0000: End of reserved area, start of regular filesystem partitions
 *
 * Remarks:
 * - nboot-start[] in nboot-info is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the
 *   Makefile. This is the only value where SPL and nboot-info must match.
 * - The reserved region size is the same as in NXP layouts (8MB).
 * - The space in the reserved region when booting from Boot partition, can be
 *   used to store an M4 image or as UserDef region.
 */

#ifndef __FSIMX8MP_H
#define __FSIMX8MP_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/* disable FAT write because its doesn't work
 *  with F&S FAT driver
 */
#undef CONFIG_FAT_WRITE

/* disable FASTBOOT_USB_DEV so both ports can be used */
#undef CONFIG_FASTBOOT_USB_DEV

/* need for F&S bootaux */
#define M4_BOOTROM_BASE_ADDR		MCU_BOOTROM_BASE_ADDR
#define IMX_SIP_SRC_M4_START		IMX_SIP_SRC_MCU_START
#define IMX_SIP_SRC_M4_STARTED		IMX_SIP_SRC_MCU_STARTED

#define CONFIG_SYS_BOOTM_LEN		(64 * SZ_1M)

#define CONFIG_SPL_MAX_SIZE		(152 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(1024 * 1024)
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

/*
 * The memory layout on stack:  DATA section save + gd + early malloc
 * the idea is re-use the early malloc (CONFIG_SYS_MALLOC_F_LEN) with
 * CONFIG_SYS_SPL_MALLOC_START
 */
#define CONFIG_FUS_BOARDCFG_ADDR	0x00918000
#define CONFIG_SPL_BSS_START_ADDR	0x0091a000
#define CONFIG_SPL_BSS_MAX_SIZE		0x2000	/* 8 KB */

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x927FF0
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT

/* Offsets in eMMC where BOARD-CFG and FIRMWARE are stored */
#define CONFIG_FUS_BOARDCFG_MMC0	0x00048000
#define CONFIG_FUS_BOARDCFG_MMC1	0x00448000

#define CONFIG_SYS_SPL_MALLOC_START	0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_1M
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

/* These addresses are hardcoded in ATF */
#define CONFIG_SPL_USE_ATF_ENTRYPOINT
#define CONFIG_SPL_ATF_ADDR		0x00968000
#define CONFIG_SPL_TEE_ADDR		0x56000000

/* TCM Address where DRAM Timings are loaded to */
#define CONFIG_SPL_DRAM_TIMING_ADDR	0x00964000

#define CONFIG_MALLOC_F_ADDR		0x91A800 /* malloc f used before GD_FLG_FULL_MALLOC_INIT set */

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PCA9450

#define CONFIG_SYS_I2C

#endif

/* Add F&S update */
#define CONFIG_CMD_UPDATE
#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG

#define CONFIG_REMAKE_ELF
/* ENET Config */
/* ENET1 */
#define CONFIG_SYS_DISCOVER_PHY

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth0" /* Set qos to primary since we use its MDIO */
#define FDT_SEQ_MACADDR_FROM_ENV

#define CONFIG_FEC_XCV_TYPE             RGMII
//#define CONFIG_FEC_MXC_PHYADDR          5
#define FEC_QUIRK_ENET_MAC

//#define DWC_NET_PHYADDR					4
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY     (1 * SZ_1M)     /* 1M */
#endif

#define PHY_ANEG_TIMEOUT 20000

#define CONFIG_PHY_ATHEROS
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"

#endif



#define CONFIG_BOOTFILE		"Image"
#define CONFIG_PREBOOT
#ifdef CONFIG_FS_UPDATE_SUPPORT
	#define CONFIG_BOOTCOMMAND	"run selector; run set_bootargs; run kernel; run fdt; run failed_update_reset"
#else
	#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"
#endif

#define MTDIDS_DEFAULT		""
#define MTDPART_DEFAULT		""
#define MTDPARTS_STD		"setenv mtdparts "
#define MTDPARTS_UBIONLY	"setenv mtdparts "

/* Add some variables that are not predefined in U-Boot. For example set
   fdt_high to 0xffffffff to avoid that the device tree is relocated to the
   end of memory before booting, which is not necessary in our setup (and
   would result in problems if RAM is larger than ~1,7GB).

   All entries with content "undef" will be updated in board_late_init() with
   a board-specific value (detected at runtime).

   We use ${...} here to access variable names because this will work with the
   simple command line parser, who accepts $(...) and ${...}, and also the
   hush parser, who accepts ${...} and plain $... without any separator.

   If a variable is meant to be called with "run" and wants to set an
   environment variable that contains a ';', we can either enclose the whole
   string to set in (escaped) double quotes, or we have to escape the ';' with
   a backslash. However when U-Boot imports the environment from NAND into its
   hash table in RAM, it interprets escape sequences, which will remove a
   single backslash. So we actually need an escaped backslash, i.e. two
   backslashes. Which finally results in having to type four backslashes here,
   as each backslash must also be escaped with a backslash in C. */
#define BOOT_WITH_FDT "\\\\; booti ${loadaddr} - ${fdtaddr}\0"

#ifdef CONFIG_FS_UPDATE_SUPPORT
/*
 * F&S updates are based on an A/B mechanism. All storage regions for U-Boot,
 * kernel, device tree and rootfs are doubled, there is a slot A and a slot B.
 * One slot is always active and holds the current software. The other slot is
 * passive and can be used to install new software versions. When all new
 * versions are installed, the roles of the slots are swapped. This means the
 * previously passive slot with the new software gets active and the
 * previously active slot with the old software gets passive. This
 * configuration is then started. If it proves to work, then the new roles get
 * permanent and the now passive slot is available for future versions. If the
 * system will not start successfully, the roles will be switched back and the
 * system will be working with the old software again.
 */

/* In case of NAND, load kernel and device tree from MTD partitions. */
#ifdef CONFIG_CMD_NAND
#define MTDPARTS_DEFAULT						\
	"mtdparts=" MTDPARTS_1 MTDPARTS_2_U MTDPARTS_3_A MTDPARTS_3_B MTDPARTS_4
#define BOOT_FROM_NAND							\
	".mtdparts_std=setenv mtdparts " MTDPARTS_DEFAULT "\0"		\
	".kernel_nand_A=setenv kernel nand read ${loadaddr} Kernel_A\0" \
	".kernel_nand_B=setenv kernel nand read ${loadaddr} Kernel_B\0" \
	".fdt_nand_A=setenv fdt nand read ${fdtaddr} FDT_A" BOOT_WITH_FDT \
	".fdt_nand_B=setenv fdt nand read ${fdtaddr} FDT_B" BOOT_WITH_FDT
#else
#define BOOT_FROM_NAND
#endif

/* In case of UBI, load kernel and FDT directly from UBI volumes */
#ifdef CONFIG_CMD_UBI
#define BOOT_FROM_UBI							\
	".mtdparts_ubionly=setenv mtdparts mtdparts="			\
	  MTDPARTS_1 MTDPARTS_2_U MTDPARTS_4 "\0"			\
	".ubivol_std=ubi part TargetFS;"				\
	" ubi create rootfs_A ${rootfs_size};"				\
	" ubi create rootfs_B ${rootfs_size};"				\
	" ubi create data\0"						\
	".ubivol_ubi=ubi part TargetFS;"				\
	" ubi create kernel_A ${kernel_size} s;"			\
	" ubi create kernel_B ${kernel_size} s;"			\
	" ubi create fdt_A ${fdt_size} s;"				\
	" ubi create fdt_B ${fdt_size} s;"				\
	" ubi create rootfs_A ${rootfs_size};"				\
	" ubi create rootfs_B ${rootfs_size};"				\
	" ubi create data\0"						\
	".kernel_ubi_A=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel_A\0"					\
	".kernel_ubi_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel_B\0"					\
	".fdt_ubi_A=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt_A" BOOT_WITH_FDT			\
	".fdt_ubi_B=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt_B" BOOT_WITH_FDT
#else
#define BOOT_FROM_UBI
#endif

/*
 * In case of UBIFS, the rootfs is loaded from a UBI volume. If Kernel and/or
 * device tree are loaded from UBIFS, they are supposed to be part of the
 * rootfs in directory /boot.
 */
#ifdef CONFIG_CMD_UBIFS
#define BOOT_FROM_UBIFS							\
	".kernel_ubifs_A=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_A\\\\; ubifsload . /boot/${bootfile}\0"\
	".kernel_ubifs_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\; ubifsload . /boot/${bootfile}\0"\
	".fdt_ubifs_A=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_A\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".fdt_ubifs_B=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_ubifs_A=setenv rootfs 'rootfstype=squashfs"		\
	" ubi.block=0,rootfs_A ubi.mtd=TargetFS,2048"			\
	" root=/dev/ubiblock0_0 rootwait ro'\0"				\
	".rootfs_ubifs_B=setenv rootfs 'rootfstype=squashfs"		\
	" ubi.block=0,rootfs_B ubi.mtd=TargetFS,2048"			\
	" root=/dev/ubiblock0_1 rootwait ro'\0"
#else
#define BOOT_FROM_UBIFS
#endif

/*
 * In case of (e)MMC, the rootfs is loaded from a separate partition. Kernel
 * and device tree are loaded as files from a different partition that is
 * typically formated with FAT.
 */
#ifdef CONFIG_CMD_MMC
#define BOOT_FROM_MMC							\
	".boot_part_A=1\0"							\
	".boot_part_B=2\0"							\
	".rootfs_part_A=5\0"							\
	".rootfs_part_B=6\0"							\
	".kernel_mmc_A=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev}:${.boot_part_A}\0"					\
	".kernel_mmc_B=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev}:${.boot_part_B}\0"					\
	".fdt_mmc_A=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev}:${.boot_part_A} ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
	".fdt_mmc_B=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev}:${.boot_part_B} ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
	".rootfs_mmc_A=setenv rootfs root=/dev/mmcblk${mmcdev}p${.rootfs_part_A}"	\
	" rootfstype=squashfs rootwait\0"				\
	".rootfs_mmc_B=setenv rootfs root=/dev/mmcblk${mmcdev}p${.rootfs_part_B}"	\
	" rootfstype=squashfs rootwait\0"
#else
#define BOOT_FROM_MMC
#endif

/* Loading from USB is not supported for updates yet */
#define BOOT_FROM_USB

/* Loading from TFTP is not supported for updates yet */
#define BOOT_FROM_TFTP

/* Loading from NFS is not supported for updates yet */
#define BOOT_FROM_NFS

#define FAILED_UPDATE_RESET \
	"failed_update_reset=" \
		"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then	" \
			"reset; "\
		"fi;\0"

#define BOOTARGS "set_bootargs=setenv bootargs ${console} ${login} ${mtdparts}"\
	" ${network} ${rootfs} ${mode} ${init} ${extra} ${rauc_cmd}\0"

/* Generic settings for booting with updates on A/B */
#define BOOT_SYSTEM		\
	".init_fs_updater=setenv init init=/sbin/preinit.sh\0"		\
	"BOOT_ORDER=A B\0"						\
	"BOOT_ORDER_OLD=A B\0"						\
	"BOOT_A_LEFT=3\0"						\
	"BOOT_B_LEFT=3\0"						\
	"update_reboot_state=0\0"					\
	"update=0000\0"							\
	"application=A\0"						\
	"rauc_cmd=rauc.slot=A\0"					\
	"selector="							\
	"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then "	\
		"setenv slot_cnt 0;"	\
		"for slot in \"${BOOT_ORDER}\"; do "	\
			"setexpr slot_cnt $slot_cnt + 1; "	\
		"done;"	\
		"echo \"Available slots $slot_cnt for booting.\";"	\
		"if test $slot_cnt -eq 1; then "	\
			"if test \"x${BOOT_ORDER}\" == \"xA\"; then " \
				"setenv BOOT_ORDER \"A B\";"	\
			"elif test \"x${BOOT_ORDER}\" == \"xB\"; then "	\
				"setenv BOOT_ORDER \"B A\";"	\
			"fi;"	\
		"fi;"	\
		"env delete slot_cnt;"	\
		"saveenv;"	\
	"fi;"	\
	"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then "			\
		"if test $update_reboot_state -eq 0; then "	\
			"setenv BOOT_ORDER $BOOT_ORDER_OLD;"	\
		"else "	\
			"setenv rauc_cmd undef;"						\
			"for slot in \"${BOOT_ORDER}\"; do "					\
				"setenv sname \"BOOT_\"\"$slot\"\"_LEFT\"; "			\
				"if test \"${!sname}\" -gt 0; then "				\
					"echo \"Current rootfs boot_partition is $slot\"; "	\
					"setexpr $sname ${!sname} - 1; "			\
					"run .kernel_${bd_kernel}_${slot}; "			\
					"run .fdt_${bd_fdt}_${slot}; "				\
					"run .rootfs_${bd_rootfs}_${slot}; "			\
					"setenv rauc_cmd rauc.slot=${slot}; "			\
					"setenv sname ; "					\
					"saveenv;"						\
					"exit;"							\
				"else "								\
					"for slot_a in \"${BOOT_ORDER_OLD}\"; do "		\
						"run .kernel_${bd_kernel}_${slot_a}; "		\
						"run .fdt_${bd_fdt}_${slot_a}; "		\
						"run .rootfs_${bd_rootfs}_${slot_a}; "		\
						"setenv rauc_cmd rauc.slot=${slot_a}; "		\
						"setenv sname ;"				\
						"saveenv;"					\
						"exit;"						\
					"done;"							\
				"fi;"								\
			"done;"									\
		"fi;"	\
	"fi;\0"

#else /* CONFIG_FS_UPDATE_SUPPORT */

/*
 * In a regular environment, all storage regions for U-Boot, kernel, device
 * tree and rootfs are only available once, no A and B. This provides more
 * free space.
 */

/* In case of NAND, load kernel and device tree from MTD partitions. */
#ifdef CONFIG_CMD_NAND
#define MTDPARTS_DEFAULT						\
	"mtdparts=" MTDPARTS_1 MTDPARTS_2 MTDPARTS_3 MTDPARTS_4
#define BOOT_FROM_NAND							\
	".mtdparts_std=setenv mtdparts " MTDPARTS_DEFAULT "\0"		\
	".kernel_nand=setenv kernel nand read ${loadaddr} Kernel\0"	\
 	".fdt_nand=setenv fdt nand read ${fdtaddr} FDT" BOOT_WITH_FDT
#else
#define BOOT_FROM_NAND
#endif

/* In case of UBI, load kernel and FDT directly from UBI volumes */
#ifdef CONFIG_CMD_UBI
#define BOOT_FROM_UBI							\
	".mtdparts_ubionly=setenv mtdparts mtdparts="			\
	  MTDPARTS_1 MTDPARTS_2 MTDPARTS_4 "\0"				\
	".ubivol_std=ubi part TargetFS; ubi create rootfs\0"		\
	".ubivol_ubi=ubi part TargetFS; ubi create kernel ${kernel_size} s;" \
	" ubi create fdt ${fdt_size} s; ubi create rootfs\0"		\
	".kernel_ubi=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel\0"						\
	".fdt_ubi=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt" BOOT_WITH_FDT
#else
#define BOOT_FROM_UBI
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_FROM_UBIFS							\
	".kernel_ubifs=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs\\\\; ubifsload . /boot/${bootfile}\0"	\
	".fdt_ubifs=setenv fdt ubi part TargetFS\\\\;"			\
	" ubifsmount ubi0:rootfs\\\\;"					\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_ubifs=setenv rootfs rootfstype=ubifs ubi.mtd=TargetFS" \
	" root=ubi0:rootfs\0"
#else
#define BOOT_FROM_UBIFS
#endif

/*
 * In case of (e)MMC, the rootfs is loaded from a separate partition. Kernel
 * and device tree are loaded as files from a different partition that is
 * typically formated with FAT.
 */
#ifdef CONFIG_CMD_MMC
#define BOOT_FROM_MMC							\
	".kernel_mmc=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev} . ${bootfile}\0"				\
	".fdt_mmc=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev} ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"
#else
#define BOOT_FROM_MMC
#endif

/* In case of USB, the layout is the same as on MMC. */
#define BOOT_FROM_USB							\
	".kernel_usb=setenv kernel usb start\\\\;"			\
	" load usb 0 . ${bootfile}\0"					\
	".fdt_usb=setenv fdt usb start\\\\;"				\
	" load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"

/* In case of TFTP, kernel and device tree are loaded from TFTP server */
#define BOOT_FROM_TFTP							\
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0"		\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

/* In case of NFS, kernel, device tree and rootfs are loaded from NFS server */
#define BOOT_FROM_NFS							\
	".kernel_nfs=setenv kernel nfs ."				\
	" ${serverip}:${rootpath}/${bootfile}\0"			\
	".fdt_nfs=setenv fdt nfs ${fdtaddr}"				\
	" ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_nfs=setenv rootfs root=/dev/nfs"			\
	" nfsroot=${serverip}:${rootpath}\0"

#define BOOTARGS "set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"
/* Generic settings when not booting with updates A/B */
#define BOOT_SYSTEM
#define FAILED_UPDATE_RESET

#endif /* CONFIG_FS_UPDATE_SUPPORT */

#ifdef CONFIG_BOOTDELAY
#define FSBOOTDELAY
#else
#define FSBOOTDELAY "bootdelay=undef\0"
#endif

#if defined(CONFIG_ENV_IS_IN_MMC)
	#define FILSEIZE2BLOCKCOUNT "block_size=200\0" 	\
		"filesize2blockcount=" \
			"setexpr test_rest \\${filesize} % \\${block_size}; " \
			"if test \\${test_rest} = 0; then " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
			"else " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
				"setexpr blocckount \\${blockcount} + 1; " \
			"fi;\0"
#else
	#define FILSEIZE2BLOCKCOUNT
#endif

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"bd_kernel=undef\0"						\
	"bd_fdt=undef\0"							\
	"bd_rootfs=undef\0"						\
	"initrd_addr=0x43800000\0"					\
	"initrd_high=0xffffffffffffffff\0"				\
	"console=undef\0"						\
	".console_none=setenv console\0"				\
	".console_serial=setenv console console=${sercon},${baudrate}\0" \
	".console_display=setenv console console=tty1\0"		\
	"login=undef\0"							\
	".login_none=setenv login login_tty=null\0"			\
	".login_serial=setenv login login_tty=${sercon},${baudrate}\0"	\
	".login_display=setenv login login_tty=tty1\0"			\
	"mtdids=undef\0"						\
	"mtdparts=undef\0"						\
	"mmcdev=undef\0"						\
	".network_off=setenv network\0"					\
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0"			\
	"rootfs=undef\0"						\
	"kernel=undef\0"						\
	"fdt=undef\0"							\
	"fdtaddr=0x43100000\0"						\
	".fdt_none=setenv fdt booti\0"					\
	BOOT_FROM_MMC						\
	BOOT_FROM_USB						\
	BOOT_FROM_TFTP						\
	BOOT_FROM_NFS						\
	BOOT_SYSTEM							\
	FILSEIZE2BLOCKCOUNT					\
	FAILED_UPDATE_RESET					\
	"mode=undef\0"							\
	".mode_rw=setenv mode rw\0"					\
	".mode_ro=setenv mode ro\0"					\
	"netdev=eth0\0"							\
	"init=undef\0"							\
	".init_init=setenv init\0"					\
	".init_linuxrc=setenv init init=linuxrc\0"			\
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8mp\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	FSBOOTDELAY							\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	BOOTARGS

/* Link Definitions */
#define CONFIG_SYS_LOAD_ADDR		0x40480000

#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x00080000
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_INIT_RAM_SIZE
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define SECURE_PARTITIONS	"UBoot", "Kernel", "FDT", "Images"

/************************************************************************
 * Environment
 ************************************************************************/

/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define CONFIG_ENV_OVERWRITE			/* Allow overwriting ethaddr */

/* Fallback values if values in the device tree are missing/damaged */
//#define CONFIG_ENV_OFFSET_REDUND 0x104000

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_32M

/* Totally 1GB LPDDR4 */
#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define CONFIG_SYS_OCRAM_BASE		0x00900000
#define CONFIG_SYS_OCRAM_SIZE		0x00090000

/* have to define for F&S serial_mxc driver */
#define UART1_BASE			UART1_BASE_ADDR
#define UART2_BASE			UART2_BASE_ADDR
#define UART3_BASE			UART3_BASE_ADDR
#define UART4_BASE			UART4_BASE_ADDR
#define UART5_BASE			0xFFFFFFFF

/* Not used on F&S boards. Detection depends on
 * board type is preferred.
 * */
#define CONFIG_MXC_UART_BASE		0

/* Monitor Command Prompt */

/************************************************************************
 * Command Line Editor (Shell)
 ************************************************************************/
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

/* Input and print buffer sizes */
#define CONFIG_SYS_CBSIZE	512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */

#define CONFIG_IMX_BOOTAUX

#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_FSL_FSPI
#define FSL_FSPI_FLASH_SIZE		SZ_256M
#define FSL_FSPI_FLASH_NUM		1
#define FSPI0_BASE_ADDR			0x30bb0000
#define FSPI0_AMBA_BASE			0x0
#define CONFIG_FSPI_QUAD_SUPPORT

#define CONFIG_SYS_FSL_FSPI_AHB
#endif
#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#define CONFIG_USBD_HS

#define CONFIG_USB_MAX_CONTROLLER_COUNT		2
#define CONFIG_USB_GADGET_VBUS_DRAW		2

#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

#endif

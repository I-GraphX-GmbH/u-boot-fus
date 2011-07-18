/*
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.

 * (C) Copyright 2000-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define DEBUG */

#include <common.h>

#if defined(CFG_ENV_IS_IN_AUTO) /* Environment is in Nand Flash */

#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <nand.h>
#include <movi.h>
#include <regs.h>

#if defined(CONFIG_CMD_ENV) || defined(CONFIG_CMD_NAND) || defined(CONFIG_CMD_MOVINAND) || defined(CONFIG_CMD_ONENAND)
#define CMD_SAVEENV
#endif

/* info for NAND chips, defined in drivers/nand/nand.c */
extern nand_info_t nand_info[];

/* references to names in env_common.c */
extern uchar default_environment[];
extern int default_environment_size;

char * env_name_spec = "SMDK bootable device";

#ifdef ENV_IS_EMBEDDED
extern uchar environment[];
env_t *env_ptr = (env_t *)(&environment[0]);
#else /* ! ENV_IS_EMBEDDED */
env_t *env_ptr = 0;
#endif /* ENV_IS_EMBEDDED */


/* local functions */
#if !defined(ENV_IS_EMBEDDED)
static void use_default(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

uchar env_get_char_spec (int index)
{
	return ( *((uchar *)(gd->env_addr + index)) );
}


/* this is called before nand_init()
 * so we can't read Nand to validate env data.
 * Mark it OK for now. env_relocate() in env_common.c
 * will call our relocate function which will does
 * the real validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash. This way
 * the SPL loads not only the U-Boot image from NAND but also the
 * environment.
 */
int env_init(void)
{
#if defined(ENV_IS_EMBEDDED)
	ulong total;
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1, *tmp_env2;

	total = CFG_ENV_SIZE;

	tmp_env1 = env_ptr;
	tmp_env2 = (env_t *)((ulong)env_ptr + CFG_ENV_SIZE);

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if (!crc1_ok && !crc2_ok)
		gd->env_valid = 0;
	else if(crc1_ok && !crc2_ok)
		gd->env_valid = 1;
	else if(!crc1_ok && crc2_ok)
		gd->env_valid = 2;
	else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	if (gd->env_valid == 1)
		env_ptr = tmp_env1;
	else if (gd->env_valid == 2)
		env_ptr = tmp_env2;
#else /* ENV_IS_EMBEDDED */
	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 1;
#endif /* ENV_IS_EMBEDDED */

	return (0);
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
int saveenv_nand(void)
{
	size_t total;
	int ret = 0;

	puts("Erasing Nand...");
	nand_erase(&nand_info[0], CFG_ENV_OFFSET, CFG_ENV_SIZE);

//#ifndef CONFIG_S5PC100_EVT1
	if (nand_erase(&nand_info[0], CFG_ENV_OFFSET, CFG_ENV_SIZE))
		return 1;
//#endif

	puts("Writing to Nand... ");
	total = CFG_ENV_SIZE;

//#ifndef CONFIG_S5PC100_EVT1
	ret = nand_write(&nand_info[0], CFG_ENV_OFFSET, &total, (u_char*) env_ptr);
	if (ret || total != CFG_ENV_SIZE)
		return 1;
//#endif

	puts("done\n");
	return ret;
}

int saveenv_nand_adv(void)
{
	size_t total;
	int ret = 0;

	u_char *tmp;
	total = CFG_ENV_OFFSET;

	tmp = (u_char *) malloc(total);
	nand_read(&nand_info[0], 0x0, &total, (u_char *) tmp);

	puts("Erasing Nand...");
	nand_erase(&nand_info[0], 0x0, CFG_ENV_OFFSET + CFG_ENV_SIZE);

//#ifndef CONFIG_S5PC100_EVT1
	if (nand_erase(&nand_info[0], 0x0, CFG_ENV_OFFSET + CFG_ENV_SIZE)) {
		free(tmp);
		return 1;
	}
//#endif

	puts("Writing to Nand... ");
	ret = nand_write(&nand_info[0], 0x0, &total, (u_char *) tmp);
	total = CFG_ENV_SIZE;

	ret = nand_write(&nand_info[0], CFG_ENV_OFFSET, &total, (u_char *) env_ptr);

//#ifndef CONFIG_S5PC100_EVT1
	if (ret || total != CFG_ENV_SIZE) {
		free(tmp);
		return 1;
	}
//#endif
	puts("done\n");
	free(tmp);

	return ret;
}

#ifdef CONFIG_MOVINAND
int saveenv_movinand(void)
{
#ifndef CONFIG_S5PC100
	movi_write_env(virt_to_phys((ulong)env_ptr));
#endif
	puts("done\n");

	return 1;
}
#endif /*CONFIG_MOVINAND*/

#ifdef CONFIG_ONENAND
int saveenv_onenand(void)
{
	printf("OneNAND does not support the saveenv command\n");
	return 1;
}
#endif /*CONFIG_ONENAND*/

int saveenv(void)
{
#if !defined(CONFIG_SMDK6440)
	if (INF_REG3_REG == 2 || INF_REG3_REG == 3)
		saveenv_nand();
	else if (INF_REG3_REG == 4 || INF_REG3_REG == 5 || INF_REG3_REG == 6)
		saveenv_nand_adv();
#ifdef CONFIG_MOVINAND
	else if (INF_REG3_REG == 0 || INF_REG3_REG == 7)
		saveenv_movinand();
#endif
#ifdef CONFIG_ONENAND
	else if (INF_REG3_REG == 1)
		saveenv_onenand();
#endif
	else
		printf("Unknown boot device\n");
#else
	if (INF_REG3_REG == 3)
		saveenv_nand();
	else if (INF_REG3_REG == 4 || INF_REG3_REG == 5 || INF_REG3_REG == 6)
		saveenv_nand_adv();
	else if (INF_REG3_REG == 0 || INF_REG3_REG == 1 || INF_REG3_REG == 7)
		saveenv_movinand();
	else
		printf("Unknown boot device\n");
#endif

	return 0;
}

#endif /* CMD_SAVEENV */

/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
void env_relocate_spec_nand(void)
{
#if !defined(ENV_IS_EMBEDDED)
	size_t total;
	int ret;
        unsigned int crc32val;

	total = CFG_ENV_SIZE;
	ret = nand_read(&nand_info[0], CFG_ENV_OFFSET, &total, (u_char*)env_ptr);
  	if (ret || total != CFG_ENV_SIZE)
        {
            printf("####### env_relocate_spec_nand(): ret=%d, total=0x%x\n", ret, total);
		return use_default();
        }

        crc32val = crc32(0, env_ptr->data, ENV_SIZE);
	if (crc32val != env_ptr->crc)
        {
            printf("####### env_relocate_spec_nand(): crc=0x%x, env_ptr->crc=0x%x\n", crc32val, env_ptr->crc);
		return use_default();
        }

#endif /* ! ENV_IS_EMBEDDED */
}

#ifdef CONFIG_MOVINAND
void env_relocate_spec_movinand(void)
{
#if !defined(ENV_IS_EMBEDDED)
	uint *magic = (uint*)(PHYS_SDRAM_1);

	if ((0x24564236 != magic[0]) || (0x20764316 != magic[1]))
#ifndef CONFIG_S5PC100
		movi_read_env(virt_to_phys((ulong)env_ptr));
#endif
	if (crc32(0, env_ptr->data, ENV_SIZE) != env_ptr->crc)
		return use_default();
#endif /* ! ENV_IS_EMBEDDED */
}
#endif /*CONFIG_MOVINAND*/

#ifdef CONFIG_ONENAND
void env_relocate_spec_onenand(void)
{
	use_default();
}
#endif

void env_relocate_spec(void)
{
#if !defined(CONFIG_SMDK6440)
	if (INF_REG3_REG >= 2 && INF_REG3_REG <= 6)
		env_relocate_spec_nand();
#ifdef CONFIG_MOVINAND
	else if (INF_REG3_REG == 0 || INF_REG3_REG == 7)
		env_relocate_spec_movinand();
#endif
#ifdef CONFIG_ONENAND
	else if (INF_REG3_REG == 1)
		env_relocate_spec_onenand();
#endif
	else
		printf("Unknown boot device\n");
#else
	if (INF_REG3_REG >= 3 && INF_REG3_REG <= 6)
		env_relocate_spec_nand();
#ifdef CONFIG_MOVINAND
	else if (INF_REG3_REG == 0 || INF_REG3_REG == 1 || INF_REG3_REG == 7)
		env_relocate_spec_movinand();
#endif
#endif
}

#if !defined(ENV_IS_EMBEDDED)
static void use_default()
{
	puts("*** Warning - using default environment\n\n");

	if (default_environment_size > CFG_ENV_SIZE) {
		puts("*** Error - default environment is too large\n\n");
		return;
	}

	memset (env_ptr, 0, sizeof(env_t));
	memcpy (env_ptr->data,
			default_environment,
			default_environment_size);
	env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE);
	gd->env_valid = 1;

}
#endif

#endif /* CFG_ENV_IS_IN_NAND */

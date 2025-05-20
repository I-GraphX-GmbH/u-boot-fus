/*
 * dram_test.c
 *
 *  Created on: Apr 28, 2020
 *      Author: developer
 */
#include <common.h>
#include <command.h>
#include "fs_dram_test.h"
#include "fs_board_common.h"/* fs_board_*() */
#include <asm/global_data.h>
#include <asm/arch/sys_proto.h>
/* =============== SDRAM Test ============================================== */

#define SIZETESTVALUE 0xA5                /* Testvalue for size detection */

/* TODO: Add more RAM types
 * 		 Get RAM Size/Num
 * 		 Check failure case
 * 		 Check if Uboot gets destroyed by the test
 */
/* Information for SDRAM size and test cycles */
struct memtestinfo
{
    u64 dwRowOffset;                    /* Offset from row to row */
    int nColRepeat;                       /* Number of column repeats */
    int nRowBankRepeat;                   /* Number of row and bank repeats */
    char *pchRamSize;                     /* RAM size in MB */
};

struct ramInfo
{
	phys_size_t ramSize; /* In Byte */
	u8  numChips;
	u64* pRamBase; /* Address */
	u64* pUbootBase; /* Address */

};

static void getRamInfo(struct ramInfo *rI){

	DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_IMX8MM) || defined(CONFIG_IMX8MN) || defined (CONFIG_IMX8MP)
	// HOTFIX: Assume rom_pointer[1] == 0x1
	rI->ramSize = gd->ram_size;
	// TODO: rom_pointer[1] changes to 0x0, so we would need to save the value from SPL and load it in UBoot
	//if(rom_pointer[1])
	//	rI->ramSize += rom_pointer[1];
	rI->numChips = fs_board_get_cfg_info()->dram_chips;
	rI->pRamBase = (u64*)CONFIG_SYS_SDRAM_BASE;
	rI->pUbootBase = (u64*)gd->start_addr_sp;
#endif

}


/******************************************************************************
*** Function:   DWORD TestRamCheck(volatile BYTE *pchRam, DWORD dwInvert,   ***
                                        const struct memtestinfo *pMemInfo) ***
***                                                                         ***
*** Parameters: pchRam:   Start of RAM                                      ***
***             dwInvert: 0x00000000: do not invert; 0xFFFFFFFF: do invert  ***
***             pMemInfo: Pointer to memory structure information           ***
***                                                                         ***
*** Return:     0:   OK, all values correct                                 ***
***             !=0: Failed address                                         ***
***                                                                         ***
*** Description:                                                            ***
*** ------------                                                            ***
*** Read back the written values. If errors occur, return the bad address.  ***
******************************************************************************/
static u64 TestRamCheck(volatile u8 *pchRam, u64 dwlInvert,
                          const struct memtestinfo *pMemInfo)
{
	u64 *pdwlRam;
    u64 dwlValue;
    int nRowsBanks;
    int nColumns;

    pdwlRam = ((u64 *)&pchRam[pMemInfo->dwRowOffset]) - pMemInfo->nColRepeat;
	dwlValue = 0x0003800200018000LU;
    nRowsBanks = pMemInfo->nRowBankRepeat;

    do
    {
    	u64 *pdwlTemp = pdwlRam;

		nColumns = pMemInfo->nColRepeat;
        do
        {
			if ((dwlInvert ^ *pdwlTemp++) != dwlValue)
			{
                return (u64)pdwlTemp;
			}
            dwlValue += 0x0001000100010001LU;
		} while (--nColumns);

        pdwlRam = (u64 *)((u8 *)pdwlRam + pMemInfo->dwRowOffset);
    } while (--nRowsBanks);

    return 0;
}

/******************************************************************************
*** Function:   void TestRAM(void)                                          ***
***                                                                         ***
*** Parameters: -                                                           ***
***                                                                         ***
*** Return:     -                                                           ***
***                                                                         ***
*** Description:                                                            ***
*** ------------                                                            ***
*** Test the main memory.                                                   ***
***                                                                         ***
*** SDRAM is divided in banks, rows and columns. The i.MX6 boards have one, ***
*** two or four 16 bit chips in parallel, using 16, 32 or 64 bit bus width. ***
*** Each chip can have 1, 2, 4 or 8 Gbit, allowing for 128MB up to 4GB of   ***
*** RAM. (When using 4GB, only 3840MB are visible). The RAM region starts   ***
*** at address 0x10000000.                                                  ***
***                                                                         ***
***   RAM size   RAM chips  Banks  Rows Columns Bits                        ***
***   -----------------------------------------------------                 ***
***    128MB     1x 1Gb       8    8192  1024    16                         ***
***    256MB     1x 2Gb       8   16384  1024    16                         ***
***    512MB     1x 4Gb       8   32768  1024    16                         ***
***   1024MB     1x 8Gb       8   65536  1024    16                         ***
***    256MB     2x 1Gb       8    8192  1024    32                         ***
***    512MB     2x 2Gb       8   16384  1024    32                         ***
***   1024MB     2x 4Gb       8   32768  1024    32                         ***
***   2048MB     2x 8Gb       8   65536  1024    32                         ***
***    512MB     4x 1Gb       8    8192  1024    64                         ***
***   1024MB     4x 2Gb       8   16384  1024    64                         ***
***   2048MB     4x 4Gb       8   32768  1024    64                         ***
***   4096MB     4x 8Gb       8   65536  1024    64 (3840MB usable)         ***
***                                                                         ***
*** The banks are selected by three of the more significant address bits,   ***
*** rows and columns are selected by a multiplexed address access (RAS/CAS).***
*** The address bus has 16 bits (SDA0-SDA15), the bank address has 3 bits   ***
*** (SDBA0-SDBA2).                                                          ***
***                                                                         ***
*** Without Bank-Interleave (MSB:Banks/Rows/Colums:LSB):                    ***
***   RAM size          Banks        Rows        Columns                    ***
***   -----------------------------------------------------                 ***
***    128MB (1x 1Gb)   A24-A26      A11-A23     A1-A10                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA12  SDA0-SDA9                  ***
***    256MB (1x 2Gb)   A14-A27      A11-A13     A1-A10                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA13  SDA0-SDA9                  ***
***    512MB (1x 4Gb)   A14-A28      A11-A13     A1-A10                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA14  SDA0-SDA9                  ***
***   1024MB (1x 8Gb)   A14-A29      A11-A13     A1-A10                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA15  SDA0-SDA9                  ***
***    256MB (2x 1Gb)   A15-A27      A12-A14     A2-A11                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA12  SDA0-SDA9                  ***
***    512MB (2x 2Gb)   A15-A28      A12-A14     A2-A11                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA13  SDA0-SDA9                  ***
***   1024MB (2x 4Gb)   A15-A29      A12-A14     A2-A11                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA14  SDA0-SDA9                  ***
***   2048MB (2x 8Gb)   A15-A30      A12-A14     A2-A11                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA15  SDA0-SDA9                  ***
***    512MB (4x 1Gb)   A16-A28      A13-A15     A3-A12                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA12  SDA0-SDA9                  ***
***   1024MB (4x 2Gb)   A16-A29      A13-A15     A3-A12                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA13  SDA0-SDA9                  ***
***   2048MB (4x 4Gb)   A16-A30      A13-A15     A3-A12                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA14  SDA0-SDA9                  ***
***   4096MB (4x 8Gb)   A16-A31      A13-A15     A3-A12                     ***
***          mapped to  SDBA0-SDBA2  SDA0-SDA15  SDA0-SDA9                  ***
***                                                                         ***
*** With Bank-Interleave (MSB:Rows/Banks/Colums:LSB):                       ***
***   RAM size          Rows        Banks        Columns                    ***
***   -----------------------------------------------------                 ***
***    128MB (1x 1Gb)   A14-A26     A11-A13      A1-A10                     ***
***          mapped to  SDA0-SDA12  SDBA0-SDBA2  SDA0-SDA9                  ***
***    256MB (1x 2Gb)   A14-A27     A11-A13      A1-A10                     ***
***          mapped to  SDA0-SDA13  SDBA0-SDBA2  SDA0-SDA9                  ***
***    512MB (1x 4Gb)   A14-A28     A11-A13      A1-A10                     ***
***          mapped to  SDA0-SDA14  SDBA0-SDBA2  SDA0-SDA9                  ***
***   1024MB (1x 8Gb)   A14-A29     A11-A13      A1-A10                     ***
***          mapped to  SDA0-SDA15  SDBA0-SDBA2  SDA0-SDA9                  ***
***    256MB (2x 1Gb)   A15-A27     A12-A14      A2-A11                     ***
***          mapped to  SDA0-SDA12  SDBA0-SDBA2  SDA0-SDA9                  ***
***    512MB (2x 2Gb)   A15-A28     A12-A14      A2-A11                     ***
***          mapped to  SDA0-SDA13  SDBA0-SDBA2  SDA0-SDA9                  ***
***   1024MB (2x 4Gb)   A15-A29     A12-A14      A2-A11                     ***
***          mapped to  SDA0-SDA14  SDBA0-SDBA2  SDA0-SDA9                  ***
***   2048MB (2x 8Gb)   A15-A30     A12-A14      A2-A11                     ***
***          mapped to  SDA0-SDA15  SDBA0-SDBA2  SDA0-SDA9                  ***
***    512MB (4x 1Gb)   A16-A28     A13-A15      A3-A12                     ***
***          mapped to  SDA0-SDA12  SDBA0-SDBA2  SDA0-SDA9                  ***
***   1024MB (4x 2Gb)   A16-A29     A13-A15      A3-A12                     ***
***          mapped to  SDA0-SDA13  SDBA0-SDBA2  SDA0-SDA9                  ***
***   2048MB (4x 4Gb)   A16-A30     A13-A15      A3-A12                     ***
***          mapped to  SDA0-SDA14  SDBA0-SDBA2  SDA0-SDA9                  ***
***   4096MB (4x 8Gb)   A16-A31     A13-A15      A3-A12                     ***
***          mapped to  SDA0-SDA15  SDBA0-SDBA2  SDA0-SDA9                  ***
***                                                                         ***
*** The idea of this test is: write only as much data, that every connected ***
*** address bit is checked at least once with 0 and 1. Write different data ***
*** so that copy effects can be seen.                                       ***
***                                                                         ***
*** The first idea would be to write one value in every row to check all of ***
*** the (multiplexed) address lines.  This requires writing one 16, 32 or   ***
*** 64 bit value every 2K, 4K, 8K or 16K (depending on RAM size). But as    ***
*** the boot monitor itself and the MMU page tables are located in RAM too, ***
*** we would destroy memory access and our own program with this method.    ***
***                                                                         ***
*** However as the address lines are multiplexed,  it is as easy to use the ***
*** columns for testing the address lines.  This only requires a continuous ***
*** block of 2K, 4K, 8K or 16K to test the lower 10 address lines SDA0 to   ***
*** SDA9. To test the remaining address lines SDA10 to SDA12 (or even SDA15 ***
*** on the largest RAMs), we can again write to the rows as in the first    ***
*** idea. So after the block of continuous data,  there will be a gap that  ***
*** will not be modified by our test. These gaps are big enough to hold the ***
*** boot monitor program, the MMU page tables, and all other stuff that     ***
*** must not be destroyed during the memory test.                           ***
***                                                                         ***
*** In addition we have to test the bank address bits. But these immediately***
*** follow the row address bits (when now bank interleave is used) or the   ***
*** column address bits (when bank interleave is used). So these bits can   ***
*** be tested in one continuous go with the row or the column bits. So we   ***
*** do not need three loops (columns, rows, banks, or columns, banks, rows) ***
*** but only two loops (columns, rows+banks or columns+banks, rows).        ***
***
*** When writing 64 bit words at a time, we only modify 64KB on 128MB RAM   ***
*** for example. On 4GB RAM, we only need to write/verify 512KB. So this is ***
*** a considerable saving.                                                  ***
***                                                                         ***
*** One more thought: By using an uncached address we must make sure that   ***
*** we really test the RAM and not the data cache.                          ***
******************************************************************************/

int fs_test_ram(char * szStrBuffer)
{
    volatile u8 *pchRam;                /* Uncached RAM, BYTE access */
    u64 *pdwlRam;                   /* Uncached RAM, 64bit access */
    u64 dwlValue;                   /* Value to write/compare */
     struct memtestinfo *pMemInfo;   /* Pointer to memtable entry above */
    int nRowsBanks;
    int nColumns;
	int index;
	int err = 0;
	struct ramInfo rI;
	u32 dwChipSize, ramsize;
	u64 dwBadAddress;
	u64 skip_area;

	/* Clear reason-string */
	szStrBuffer[0] = '\0';
#if 0
	 struct memtestinfo memtable_with_interleave[] =
	{
		/* using 64 bit accesses in all cases */
		/* With bank interleave */
	    {0x01000000,  8*256,   8, "128MB."},     /*   128MB (1x  1Gb) */
	    {0x01000000,  8*256,  16, "256MB."},     /*   256MB (1x  2Gb) */
		{0x01000000,  8*256,  32, "512MB."},     /*   512MB (1x  4Gb) */
		{0x01000000,  8*256,  64, "1024MB"},     /*  1024MB (1x  8Gb) */
		{0x01000000,  8*256, 128, "2048MB"},     /*  2048MB (1x 16Gb) */
		{0x01000000,  8*256, 256, "4096MB"},     /*  4096MB (1x 32Gb) */
	    {0x01000000,  8*512,   8, "256MB."},     /*   256MB (2x  1Gb) */
	    {0x01000000,  8*512,  16, "512MB."},     /*   512MB (2x  2Gb) */
		{0x01000000,  8*512,  32, "1024MB"},     /*  1024MB (2x  4Gb) */
		{0x01000000,  8*512,  64, "2048MB"},     /*  2048MB (2x  8Gb) */
		{0x01000000,  8*512, 128, "4096MB"},     /*  4096MB (2x 16Gb) */
		{0x01000000,  8*512, 256, "8192MB"},     /*  8192MB (2x 32Gb) */
	    {0x01000000, 8*1024,   8, "512MB."},     /*   512MB (4x  1Gb) */
	    {0x01000000, 8*1024,  16, "1024MB"},     /*  1024MB (4x  2Gb) */
		{0x01000000, 8*1024,  32, "2048MB"},     /*  2048MB (4x  4Gb) */
		{0x01000000, 8*1024,  64, "4096MB"},     /*  4096MB (4x  8Gb) */
		{0x01000000, 8*1024, 128, "8192MB"},     /*  8192MB (4x 16Gb) */
		{0x01000000, 8*1024, 256, "16384MB"},    /* 16384MB (4x 32Gb) */
	};
#endif
	 struct memtestinfo memtable_no_interleave[] =
	{	/* using 64 bit accesses in all cases */
		/* No bank interleave */
	    {0x00200000,  256,   8*8, "128MB."},     /*   128MB (1x  1Gb) */
	    {0x00200000,  256,  8*16, "256MB."},     /*   256MB (1x  2Gb) */
		{0x00200000,  256,  8*32, "512MB."},     /*   512MB (1x  4Gb) */
		{0x00200000,  256,  8*64, "1024MB"},     /*  1024MB (1x  8Gb) */
		{0x00200000,  256, 8*128, "2048MB"},     /*  2048MB (1x 16Gb) */
		{0x00200000,  256, 8*256, "4096MB"},     /*  4096MB (1x 32Gb) */
	    {0x00200000,  512,   8*8, "256MB."},     /*   256MB (2x  1Gb) */
	    {0x00200000,  512,  8*16, "512MB."},     /*   512MB (2x  2Gb) */
		{0x00200000,  512,  8*32, "1024MB"},     /*  1024MB (2x  4Gb) */
		{0x00200000,  512,  8*64, "2048MB"},     /*  2048MB (2x  8Gb) */
		{0x00200000,  512, 8*128, "4096MB"},     /*  4096MB (2x 16Gb) */
		{0x00200000,  512, 8*256, "8192MB"},     /*  8192MB (2x 32Gb) */
	    {0x00200000, 1024,   8*8, "512MB."},     /*   512MB (4x  1Gb) */
	    {0x00200000, 1024,  8*16, "1024MB"},     /*  1024MB (4x  2Gb) */
		{0x00200000, 1024,  8*32, "2048MB"},     /*  2048MB (4x  4Gb) */
		{0x00200000, 1024,  8*64, "4096MB"},     /*  4096MB (4x  8Gb) */
		{0x00200000, 1024, 8*128, "8192MB"},     /*  8192MB (4x 16Gb) */
		{0x00200000, 1024, 8*256, "16384MB"},    /* 16384MB (4x 32Gb) */
	};

	getRamInfo(&rI);

	ramsize = (rI.ramSize/1024)/1024;
	dwChipSize = ramsize/ rI.numChips;

	if (dwChipSize == 4096)
		index = 5;
	else if (dwChipSize == 2048)
		index = 4;
	else if (dwChipSize == 1024)
		index = 3;
	else if (dwChipSize == 512)
		index = 2;
	else if (dwChipSize == 256)
		index = 1;
	else
		index = 0;

	if (rI.numChips == 4)
		index += 12;
	else if (rI.numChips == 2)
		index += 6;

	pMemInfo = &memtable_no_interleave[index];

	pchRam = (volatile u8 *)rI.pRamBase; //DRAM_BASE_PA_START;

	/* Do not test the mem area of the uboot */
	skip_area = (((u64)rI.ramSize + (u64)rI.pRamBase) - (u64)rI.pUbootBase);
	/*round up */
	skip_area = (skip_area+(u32)pMemInfo->dwRowOffset+1)/(u32)pMemInfo->dwRowOffset;
	pMemInfo->nRowBankRepeat -= skip_area;

	/* Write increasing values to increasing addresses */
	pdwlRam = ((u64 *)&pchRam[pMemInfo->dwRowOffset]) - pMemInfo->nColRepeat;

	dwlValue = 0x0003800200018000LU;
	nRowsBanks = pMemInfo->nRowBankRepeat;
	do
	{
		volatile u64 *pdwlTemp = pdwlRam;

		nColumns = pMemInfo->nColRepeat;
		do
		{
			*pdwlTemp++ = dwlValue;
			dwlValue += 0x0001000100010001LU;

		} while (--nColumns);
		pdwlRam = (u64 *)((u8 *)pdwlRam + pMemInfo->dwRowOffset);
	} while (--nRowsBanks);

	/* Read back values and check for correctness */
	dwBadAddress = TestRamCheck(pchRam, 0x0000000000000000LU, pMemInfo);

	if (!dwBadAddress)
	{
		/* Step 2c: Write inverted values to decreasing addresses */
		pdwlRam = (u64 *)&pchRam[pMemInfo->dwRowOffset
												  * pMemInfo->nRowBankRepeat];
		dwlValue = 0x0003800200018000LU + 0x0001000100010001LU * pMemInfo->nColRepeat *
													 pMemInfo->nRowBankRepeat;
		nRowsBanks = pMemInfo->nRowBankRepeat;
		do
		{
			volatile u64 *pdwlTemp = pdwlRam;
			nColumns = pMemInfo->nColRepeat;
			do
			{
				dwlValue -= 0x0001000100010001LU;
				*(--pdwlTemp) = ~dwlValue;
			} while (--nColumns);

			pdwlRam = (u64 *)((u8 *)pdwlRam - pMemInfo->dwRowOffset);
		} while (--nRowsBanks);

		/* Step 2d: Read back inverted values and check for correctness */
		dwBadAddress = TestRamCheck(pchRam, 0xFFFFFFFFFFFFFFFFLU, pMemInfo);
	}

	/* Print test result */
	if (dwBadAddress)
	{
		sprintf(szStrBuffer,"FAILED (Bad address: 0x%llX)",dwBadAddress);
		err = -1;
	}
	else
		sprintf(szStrBuffer,"OK");

	return err;
}




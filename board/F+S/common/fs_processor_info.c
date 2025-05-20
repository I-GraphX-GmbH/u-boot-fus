#include <common.h>
#include <asm/mach-imx/sys_proto.h>
#include <asm/arch/clock.h>
#include <imx_thermal.h>


 struct cpuinfo {
	u32 freq;
	const char *temp;
	const char *type;
	u32 rev_h, rev_l;
};


static int get_processorInfo_soc(struct cpuinfo *ci){

#if defined(CONFIG_IMX8MM) ||  defined(CONFIG_IMX8MN) || defined (CONFIG_IMX8MP)
	u32 cpurev = get_cpu_rev();
	u32 max_freq;
	int minc,maxc;

	ci->type = get_imx_type((cpurev & 0x1FF000) >> 12);
	ci->rev_h = (cpurev & 0x000F0) >> 4;
	ci->rev_l = (cpurev & 0x0000F) >> 0;

	max_freq = get_cpu_speed_grade_hz();
	ci->freq = mxc_get_clock(MXC_ARM_CLK) / 1000000;
	if (!max_freq || max_freq == mxc_get_clock(MXC_ARM_CLK)) {
		ci->freq = mxc_get_clock(MXC_ARM_CLK) / 1000000;
	} else {
		ci->freq = max_freq / 1000000;
	}

	switch (get_cpu_temp_grade(&minc, &maxc)) {
	case TEMP_AUTOMOTIVE:
		ci->temp = "Automotive";
		break;
	case TEMP_INDUSTRIAL:
		ci->temp = "Industrial";
		break;
	case TEMP_EXTCOMMERCIAL:
		ci->temp = "Extended Commercial";
		break;
	default:
		ci->temp = "Commercial";
		break;
	}

	return 0;
#else
	return -1;
#endif
}


void fs_get_processorInfo (char *processortext){

	struct cpuinfo ci;
	int ret = 0;

	ret = get_processorInfo_soc(&ci);

	if (ret)
		sprintf(processortext, "No CPU-Info found");
	else
		sprintf(processortext, "CPU: i.MX %s %s %dMHz Rev %d.%d",
				ci.type, ci.temp, ci.freq, ci.rev_h, ci.rev_l);
}

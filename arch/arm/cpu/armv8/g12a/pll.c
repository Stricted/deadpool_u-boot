/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
* *
This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* *
This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
* *
You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
* *
Description:
*/


#include <common.h>
#include <command.h>
#include <asm/cpu_id.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/secure_apb.h>
#include <asm/arch/timer.h>
#include <asm/arch/pll.h>

#define STR_PLL_TEST_ALL	"all"
#define STR_PLL_TEST_SYS	"sys"
#define STR_PLL_TEST_FIX	"fix"
#define STR_PLL_TEST_DDR	"ddr"
#define STR_PLL_TEST_HDMI	"hdmi"
#define STR_PLL_TEST_GP0	"gp0"
#define STR_PLL_TEST_HIFI	"hifi"
#define STR_PLL_TEST_PCIE	"pcie"
#define STR_PLL_TEST_ETHPHY	"ethphy"
#define STR_PLL_TEST_USBPHY	"usbphy"


#define PLL_LOCK_CHECK_MAX		3

#define RET_PLL_LOCK_FAIL		0x1000
#define RET_CLK_NOT_MATCH		0x1
#define SYS_PLL_DIV16_CNTL		(1 << 24)
#define SYS_CLK_DIV16_CNTL		(1 << 1)
#define SYS_PLL_TEST_DIV		4	/* div16 */
#define HDMI_PLL_DIV_CNTL		(1 << 18)
#define HDMI_PLL_DIV_GATE		(1 << 19)

#define PLL_DIV16_OFFSET		4	/* div2/2/4 */

#define Wr(addr, data) writel(data, addr)
#define Rd(addr) readl(addr)

static int sys_pll_init(sys_pll_set_t * sys_pll_set);
static int sys_pll_test(sys_pll_set_t * sys_pll_set);
static int sys_pll_test_all(sys_pll_cfg_t * sys_pll_cfg);
static int fix_pll_test(void);
static int ddr_pll_test(void);
static int hdmi_pll_init(hdmi_pll_set_t * hdmi_pll_set);
static int hdmi_pll_test(hdmi_pll_set_t * hdmi_pll_set);
static int hdmi_pll_test_all(hdmi_pll_cfg_t * hdmi_pll_cfg);
static int gp0_pll_test(gp0_pll_set_t * gp0_pll);
static int gp0_pll_test_all(gp0_pll_cfg_t * gp0_pll_cfg);
static int hifi_pll_test_all(hifi_pll_cfg_t * hifi_pll_cfg);

static void update_bits(size_t reg, size_t mask, unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}


gp0_pll_cfg_t gp0_pll_cfg = {
	.gp0_pll[0] = {
		.pll_clk   = 6000, /* MHz */
		.pll_cntl0 = 0x080304fa,
		.pll_cntl1 = 0x00000000,
		.pll_cntl2 = 0x00000000,
		.pll_cntl3 = 0x48681c00,
		.pll_cntl4 = 0x88770290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},
	.gp0_pll[1] = {
		.pll_clk   = 3000, /* MHz */
		.pll_cntl0 = 0X0803047d,
		.pll_cntl1 = 0x00006aab,
		.pll_cntl2 = 0x00000000,
		.pll_cntl3 = 0x6a295c00,
		.pll_cntl4 = 0x65771290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x54540000
	},
};

hdmi_pll_cfg_t hdmi_pll_cfg = {
	.hdmi_pll[0] = {
		.pll_clk   = 5940, /* MHz */
		.pll_cntl0 = 0x0b0004f7,
		.pll_cntl1 = 0x00010000,
		.pll_cntl2 = 0x00000000,
		.pll_cntl3 = 0x6a28dc00,
		.pll_cntl4 = 0x65771290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},
	.hdmi_pll[1] = {
		.pll_clk   = 2970,
		.pll_cntl0 = 0x0b00047b,
		.pll_cntl1 = 0x00018000,
		.pll_cntl2 = 0x00000000,
		.pll_cntl3 = 0x6a29dc00,
		.pll_cntl4 = 0x65771290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x54540000
	},
	.hdmi_pll[2] = {
		.pll_clk   = 5934,
		.pll_cntl0 = 0x0b0004f7,
		.pll_cntl1 = 0x00008147,
		.pll_cntl2 = 0x00000000,
		.pll_cntl3 = 0x6a685c00,
		.pll_cntl4 = 0x11551293,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},
};

uint32_t sys_pll_clk[] = {6000, 3000};
sys_pll_cfg_t sys_pll_cfg = {
	.sys_pll[0] = {
		.cpu_clk   = 6000,
		.pll_cntl  = 0X080004fa,
		.pll_cntl1 = 0x0,
		.pll_cntl2 = 0x0,
		.pll_cntl3 = 0x48681c00,
		.pll_cntl4 = 0x88770290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},

	.sys_pll[1] = {
		.cpu_clk   = 3000,
		.pll_cntl  = 0X0800047d,
		.pll_cntl1 = 0x0,
		.pll_cntl2 = 0x0,
		.pll_cntl3 = 0x48681c00,
		.pll_cntl4 = 0x88770290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},
};

uint32_t hifi_pll_clk[] = {6005, 3000};
hifi_pll_cfg_t hifi_pll_cfg = {
	.hifi_pll[0] = {
		.pll_clk   = 6005,
		.pll_cntl0 = 0X080304fa,
		.pll_cntl1 = 0X00006aab,
		.pll_cntl2 = 0x0,
		.pll_cntl3 = 0x6a285c00,
		.pll_cntl4 = 0x65771290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x56540000
	},

	.hifi_pll[1] = {
		.pll_clk   = 3000,
		.pll_cntl0 = 0X0803047d,
		.pll_cntl1 = 0X00006aab,
		.pll_cntl2 = 0x0,
		.pll_cntl3 = 0x6a295c00,
		.pll_cntl4 = 0x65771290,
		.pll_cntl5 = 0x39272000,
		.pll_cntl6 = 0x54540000
	},
};

/*PCIE clk_out = 24M*m/2^(n+1)/OD*/
static const struct pciepll_rate_table pcie_pll_rate_table[] = {
	PLL_RATE(4800, 200, 1, 12),
};

unsigned long usbphy_base_cfg[2] = {CONFIG_USB_PHY_20, CONFIG_USB_PHY_21};

static void pll_report(unsigned int flag, char * name)
{
	if (flag)
		printf("%s pll test failed!\n", name);
	else
		printf("%s pll test pass!\n", name);
	return;
}

static int clk_around(unsigned int clk, unsigned int cmp)
{
	if (cmp == 1)
		cmp += 1;
	if (((cmp-2) <= clk) && (clk <= (cmp+2)))
		return 1;
	else
		return 0;
}

static void clocks_set_sys_cpu_clk(uint32_t freq, uint32_t pclk_ratio, uint32_t aclkm_ratio, uint32_t atclk_ratio )
{
	uint32_t    control;
	uint32_t    dyn_pre_mux;
	uint32_t    dyn_post_mux;
	uint32_t    dyn_div;

	// Make sure not busy from last setting and we currently match the last setting
	do {
		control = Rd(HHI_SYS_CPU_CLK_CNTL);
	} while( (control & (1 << 28)) );

	control = control | (1 << 26);              // Enable

	// Switching to System PLL...just change the final mux
	if ( freq == 1 ) {
		// wire            cntl_final_mux_sel      = control[11];
		control = control | (1 << 11);
	} else {
		switch ( freq ) {
			case   0:       // If Crystal
				dyn_pre_mux     = 0;
				dyn_post_mux    = 0;
				dyn_div         = 0;    // divide by 1
				break;
			case 1000:      // fclk_div2
				dyn_pre_mux     = 1;
				dyn_post_mux    = 0;
				dyn_div         = 0;    // divide by 1
				break;
			case  667:      // fclk_div3
				dyn_pre_mux     = 2;
				dyn_post_mux    = 0;
				dyn_div         = 0;    // divide by 1
				break;
			case  500:      // fclk_div2/2
				dyn_pre_mux     = 1;
				dyn_post_mux    = 1;
				dyn_div         = 1;    // Divide by 2
				break;
			case  333:      // fclk_div3/2
				dyn_pre_mux     = 2;
				dyn_post_mux    = 1;
				dyn_div         = 1;    // divide by 2
				break;
			case  250:      // fclk_div2/4
				dyn_pre_mux     = 1;
				dyn_post_mux    = 1;
				dyn_div         = 3;    // divide by 4
				break;
			default:
				dyn_pre_mux     = 0;
				dyn_post_mux    = 0;
				dyn_div         = 0;    // divide by 1
				break;
		}
		if ( control & (1 << 10) ) {     // if using Dyn mux1, set dyn mux 0
			// Toggle bit[10] indicating a dynamic mux change
			control = (control & ~((1 << 10) | (0x3f << 4)  | (1 << 2)  | (0x3 << 0)))
			| ((0 << 10)
			| (dyn_div << 4)
			| (dyn_post_mux << 2)
			| (dyn_pre_mux << 0));
		} else {
			// Toggle bit[10] indicating a dynamic mux change
			control = (control & ~((1 << 10) | (0x3f << 20) | (1 << 18) | (0x3 << 16)))
			| ((1 << 10)
			| (dyn_div << 20)
			| (dyn_post_mux << 18)
			| (dyn_pre_mux << 16));
		}
		// Select Dynamic mux
		control = control & ~(1 << 11);
	}
	Wr(HHI_SYS_CPU_CLK_CNTL,control);
	//
	// Now set the divided clocks related to the System CPU
	//
	// This function changes the clock ratios for the
	// PCLK, ACLKM (AXI) and ATCLK
	//       .clk_clken0_i   ( {clk_div2_en,clk_div2}    ),
	//       .clk_clken1_i   ( {clk_div3_en,clk_div3}    ),
	//       .clk_clken2_i   ( {clk_div4_en,clk_div4}    ),
	//       .clk_clken3_i   ( {clk_div5_en,clk_div5}    ),
	//       .clk_clken4_i   ( {clk_div6_en,clk_div6}    ),
	//       .clk_clken5_i   ( {clk_div7_en,clk_div7}    ),
	//       .clk_clken6_i   ( {clk_div8_en,clk_div8}    ),

	uint32_t    control1 = Rd(HHI_SYS_CPU_CLK_CNTL1);

	//       .cntl_PCLK_mux              ( hi_sys_cpu_clk_cntl1[5:3]     ),
	if ( (pclk_ratio >= 2) && (pclk_ratio <= 8) ) {
		control1 = (control1 & ~(0x7 << 3)) | ((pclk_ratio-2) << 3);
	}
	//       .cntl_ACLKM_clk_mux         ( hi_sys_cpu_clk_cntl1[11:9]    ),  // AXI matrix
	if ( (aclkm_ratio >= 2) && (aclkm_ratio <= 8) ) {
		control1 = (control1 & ~(0x7 << 9)) | ((aclkm_ratio-2) << 9);
	}
	//       .cntl_ATCLK_clk_mux         ( hi_sys_cpu_clk_cntl1[8:6]     ),
	if ( (atclk_ratio >= 2) && (atclk_ratio <= 8) ) {
		control1 = (control1 & ~(0x7 << 6)) | ((atclk_ratio-2) << 6);
	}
	Wr( HHI_SYS_CPU_CLK_CNTL1, control1 );
}

static int sys_pll_init(sys_pll_set_t * sys_pll_set)
{
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;

	do {
		Wr(HHI_SYS_PLL_CNTL0, sys_pll_set->pll_cntl);
		Wr(HHI_SYS_PLL_CNTL0, sys_pll_set->pll_cntl | (3 << 28));
		Wr(HHI_SYS_PLL_CNTL1, sys_pll_set->pll_cntl1);
		Wr(HHI_SYS_PLL_CNTL2, sys_pll_set->pll_cntl2);
		Wr(HHI_SYS_PLL_CNTL3, sys_pll_set->pll_cntl3);
		Wr(HHI_SYS_PLL_CNTL4, sys_pll_set->pll_cntl4);
		Wr(HHI_SYS_PLL_CNTL5, sys_pll_set->pll_cntl5);
		Wr(HHI_SYS_PLL_CNTL6, sys_pll_set->pll_cntl6);
		Wr(HHI_SYS_PLL_CNTL0, ((1<<29) | Rd(HHI_SYS_PLL_CNTL0)));
		Wr(HHI_SYS_PLL_CNTL0, Rd(HHI_SYS_PLL_CNTL0)&(~(1<<29)));
		_udelay(20);
		//printf("pll lock check %d\n", lock_check);
	} while((!((readl(HHI_SYS_PLL_CNTL0)>>31)&0x1)) && --lock_check);
	if (lock_check != 0)
		return 0;
	else
		return RET_PLL_LOCK_FAIL;
}

static int sys_pll_test_all(sys_pll_cfg_t * sys_pll_cfg)
{
	unsigned int i=0;
	int ret=0;

	for (i=0; i<(sizeof(sys_pll_clk)/sizeof(uint32_t)); i++) {
		sys_pll_cfg->sys_pll[0].cpu_clk = sys_pll_clk[i];
		ret += sys_pll_test(&(sys_pll_cfg->sys_pll[i]));
	}
	return ret;
}

static int sys_pll_test(sys_pll_set_t * sys_pll_set) {
	unsigned int clk_msr_reg = 0;
	unsigned int clk_msr_val = 0;
	unsigned int sys_clk = 0;
	sys_pll_set_t sys_pll;
	int ret=0;

	/* switch sys clk to oscillator */
	clocks_set_sys_cpu_clk( 0, 0, 0, 0);

	/* store current sys pll cntl */
	sys_pll.pll_cntl = readl(HHI_SYS_PLL_CNTL0);
	sys_pll.pll_cntl1 = readl(HHI_SYS_PLL_CNTL1);
	sys_pll.pll_cntl2 = readl(HHI_SYS_PLL_CNTL2);
	sys_pll.pll_cntl3 = readl(HHI_SYS_PLL_CNTL3);
	sys_pll.pll_cntl4 = readl(HHI_SYS_PLL_CNTL4);
	sys_pll.pll_cntl5 = readl(HHI_SYS_PLL_CNTL5);
	sys_pll.pll_cntl6 = readl(HHI_SYS_PLL_CNTL6);

	if (sys_pll_set->cpu_clk == 0) {
		sys_clk = (24 / ((sys_pll_set->pll_cntl>>10)&0x1F) * (sys_pll_set->pll_cntl&0x1FF)) >> ((sys_pll_set->pll_cntl>>16)&0x3);
	} else {
		sys_clk = sys_pll_set->cpu_clk;
#if 0
		if (sys_clk <= 750) {
			/* VCO/8, OD=3 */
			sys_pll_cntl = (3<<16/* OD */) | (1<<10/* N */) | (sys_clk / 3 /*M*/);
		}
		else if  (sys_clk <= 1500) {
			/* VCO/4, OD=2 */
			sys_pll_cntl = (2<<16/* OD */) | (1<<10/* N */) | (sys_clk / 6 /*M*/);
		}
		else {
			/* VCO/2, OD=1 */
			sys_pll_cntl = (1<<16/* OD */) | (1<<10/* N */) | (sys_clk / 12 /*M*/);
		}
		sys_pll_set->pll_cntl = 0x38000000 | sys_pll_cntl;
#endif
	}

	/* store CPU clk msr reg, restore it when test done */
	clk_msr_reg = readl(HHI_SYS_CPU_CLK_CNTL1);

	/* enable CPU clk msr cntl bit */
	writel(clk_msr_reg | SYS_PLL_DIV16_CNTL | SYS_CLK_DIV16_CNTL, HHI_SYS_CPU_CLK_CNTL1);

	//printf("HHI_SYS_CPU_CLK_CNTL: 0x%x\n", readl(HHI_SYS_CPU_CLK_CNTL));
	//printf("HHI_SYS_CPU_CLK_CNTL1: 0x%x\n", readl(HHI_SYS_CPU_CLK_CNTL1));

	if (0 == sys_pll_set->pll_cntl) {
		printf("sys pll cntl equal NULL, skip\n");
		return -1;
	}

	/* test sys pll */
	if (sys_pll_set->cpu_clk)
		sys_clk = sys_pll_set->cpu_clk;

	ret = sys_pll_init(sys_pll_set);
	_udelay(100);
	if (ret) {
		printf("SYS pll lock Failed! - %4d MHz\n", sys_clk);
	} else {
		printf("SYS pll lock OK! - %4d MHz. Div16 - %4d MHz. ", sys_clk, sys_clk>>PLL_DIV16_OFFSET);
		clk_msr_val = clk_util_clk_msr(17);
		printf("CLKMSR(17) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, sys_clk>>SYS_PLL_TEST_DIV)) {
			/* sys clk/pll div16 */
			printf(": Match\n");
		} else {
			ret = RET_CLK_NOT_MATCH;
			printf(": MisMatch\n");
		}
	}

	/* restore sys pll */
	sys_pll_init(&sys_pll);
	clocks_set_sys_cpu_clk( 1, 0, 0, 0);

	/* restore clk msr reg */
	writel(clk_msr_reg, HHI_SYS_CPU_CLK_CNTL1);
	return ret;
}

static int fix_pll_test(void)
{
	return 0;
}

static int ddr_pll_test(void)
{
	return 0;
}

static int hdmi_pll_init(hdmi_pll_set_t * hdmi_pll_set)
{
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;

	do {
		Wr(P_HHI_HDMI_PLL_CNTL0, hdmi_pll_set->pll_cntl0);
		Wr(P_HHI_HDMI_PLL_CNTL0, hdmi_pll_set->pll_cntl0 | (3 << 28));
		Wr(P_HHI_HDMI_PLL_CNTL1, hdmi_pll_set->pll_cntl1);
		Wr(P_HHI_HDMI_PLL_CNTL2, hdmi_pll_set->pll_cntl2);
		Wr(P_HHI_HDMI_PLL_CNTL3, hdmi_pll_set->pll_cntl3);
		Wr(P_HHI_HDMI_PLL_CNTL4, hdmi_pll_set->pll_cntl4);
		Wr(P_HHI_HDMI_PLL_CNTL5, hdmi_pll_set->pll_cntl5);
		Wr(P_HHI_HDMI_PLL_CNTL6, hdmi_pll_set->pll_cntl6);
		Wr(P_HHI_HDMI_PLL_CNTL0, Rd(P_HHI_HDMI_PLL_CNTL0) | (1<<29));
		Wr(P_HHI_HDMI_PLL_CNTL0, Rd(P_HHI_HDMI_PLL_CNTL0) & (~(1<<29)));
		//printf("lock_check: %d\n", lock_check);
		_udelay(100);
	} while ((!(0x3==((readl(P_HHI_HDMI_PLL_CNTL0)>>30)&0x3))) && --lock_check);
	if (lock_check != 0)
		return 0;
	else
		return RET_PLL_LOCK_FAIL;
}

static int hdmi_pll_test_all(hdmi_pll_cfg_t * hdmi_pll_cfg)
{
	unsigned int i=0;
	int ret=0;

	for (i=0; i<(sizeof(hdmi_pll_cfg_t)/sizeof(hdmi_pll_set_t)); i++)
		ret += hdmi_pll_test(&(hdmi_pll_cfg->hdmi_pll[i]));
	return ret;
}

static int hdmi_pll_test(hdmi_pll_set_t * hdmi_pll_set)
{
	unsigned int pll_clk = 0;
	unsigned int pll_clk_div = 0;
	unsigned int clk_msr_val = 0;
	unsigned int clk_msr_reg = 0;
	int ret = 0;

	/* store pll div setting */
	clk_msr_reg = readl(HHI_VID_PLL_CLK_DIV);
	/* connect vid_pll_div to HDMIPLL directly */
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 1<<19);
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 1<<15);

	/* div14 */
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 1<<18);
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 0x3<<16);
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 1<<15);
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 0x7fff);
	setbits_le32(HHI_VID_PLL_CLK_DIV, 1<<16);
	setbits_le32(HHI_VID_PLL_CLK_DIV, 1<<15);
	setbits_le32(HHI_VID_PLL_CLK_DIV, 0x3f80);
	clrbits_le32(HHI_VID_PLL_CLK_DIV, 1<<15);

	setbits_le32(HHI_VID_PLL_CLK_DIV, 1<<19);

	/* test pll */
	if (hdmi_pll_set->pll_clk == 0)
		pll_clk = (((24 * (hdmi_pll_set->pll_cntl0 & 0xff)) /
			  ((hdmi_pll_set->pll_cntl0 >> 10) & 0x1f)) >>
			  ((hdmi_pll_set->pll_cntl0 >> 16) & 0xf)) >>
			  ((hdmi_pll_set->pll_cntl0 >> 20) & 0x3);
	else
		pll_clk = hdmi_pll_set->pll_clk;

	_udelay(100);
	ret = hdmi_pll_init(hdmi_pll_set);
	_udelay(100);
	if (ret) {
		printf("HDMI pll lock Failed! - %4d MHz\n", pll_clk);
	} else {
		pll_clk_div = pll_clk/14;
		printf("HDMI pll lock OK! - %4d MHz. Div14 - %4d MHz. ", pll_clk, pll_clk_div);
		/* get [  55][1485 MHz] vid_pll_div_clk_out */
		clk_msr_val = clk_util_clk_msr(55);
		printf("CLKMSR(55) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, pll_clk_div)) {
			printf(": Match\n");
		} else {
			ret = RET_CLK_NOT_MATCH;
			printf(": MisMatch\n");
		}
	}

	/* restore pll */
	/* restore div cntl bit */
	writel(clk_msr_reg, HHI_VID_PLL_CLK_DIV);

	return ret;
}

static int gp0_pll_test(gp0_pll_set_t * gp0_pll)
{
	unsigned int pll_clk = 0;
	int ret=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0, od = 0;

	od = (gp0_pll->pll_cntl0 >> 16) & 0x7;
	lock_check = PLL_LOCK_CHECK_MAX;

	if (gp0_pll->pll_clk == 0)
		//pll_clk = (24 / ((gp0_pll->pll_cntl0>>10)&0x1F) * (gp0_pll->pll_cntl0&0x1FF)) >> ((gp0_pll->pll_cntl0>>16)&0x7);
		pll_clk = (24 / ((gp0_pll->pll_cntl0>>10)&0x1F) * (gp0_pll->pll_cntl0&0x1FF));
	else
		pll_clk = gp0_pll->pll_clk;


	do {
		writel(gp0_pll->pll_cntl0, HHI_GP0_PLL_CNTL0);
		writel(gp0_pll->pll_cntl0 | (3 << 28), HHI_GP0_PLL_CNTL0);
		writel(gp0_pll->pll_cntl1, HHI_GP0_PLL_CNTL1);
		writel(gp0_pll->pll_cntl2, HHI_GP0_PLL_CNTL2);
		writel(gp0_pll->pll_cntl3, HHI_GP0_PLL_CNTL3);
		writel(gp0_pll->pll_cntl4, HHI_GP0_PLL_CNTL4);
		writel(gp0_pll->pll_cntl5, HHI_GP0_PLL_CNTL5);
		writel(gp0_pll->pll_cntl6, HHI_GP0_PLL_CNTL6);

		writel(readl(HHI_GP0_PLL_CNTL0)|(1 << 29), HHI_GP0_PLL_CNTL0);
		_udelay(10);
		writel(readl(HHI_GP0_PLL_CNTL0)&(~(1<<29)), HHI_GP0_PLL_CNTL0);
		_udelay(100);
		//printf("gp0 lock_check: %4d\n", lock_check);
	} while ((!((readl(HHI_GP0_PLL_CNTL0)>>31)&0x1)) && --lock_check);
	if (0 == lock_check) {
		printf("GP0 pll lock Failed! - %4d MHz\n", pll_clk);
		ret = RET_PLL_LOCK_FAIL;
	} else {
		printf("GP0 pll lock OK! - %4d MHz.Div8 - %4d MHz. ", pll_clk,pll_clk >> od);
		/* get gp0_pll_clk */
		clk_msr_val = clk_util_clk_msr(4);
		printf("CLKMSR(4) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, pll_clk >> od)) {
			printf(": Match\n");
		} else {
			printf(": MisMatch\n");
			ret = RET_CLK_NOT_MATCH;
		}
	}
	return ret;
}

static int gp0_pll_test_all(gp0_pll_cfg_t * gp0_pll_cfg)
{
	unsigned int i=0;
	int ret=0;
	for (i=0; i<(sizeof(gp0_pll_cfg_t)/sizeof(gp0_pll_set_t)); i++)
		ret += gp0_pll_test(&(gp0_pll_cfg->gp0_pll[i]));
	return ret;
}

static int hifi_pll_test(hifi_pll_set_t * hifi_pll)
{
	unsigned int pll_clk = 0;
	int ret=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0, od = 0;

	od = (hifi_pll->pll_cntl0 >> 16) & 0x3;

	if (hifi_pll->pll_clk == 0)
		//pll_clk = (24 / ((hifi_pll->pll_cntl0>>10)&0x1F) * (hifi_pll->pll_cntl0&0x1FF)) >> ((hifi_pll->pll_cntl0>>16)&0x3);
		pll_clk = (24 / ((hifi_pll->pll_cntl0>>10)&0x1F) * (hifi_pll->pll_cntl0&0x1FF));
	else
		pll_clk = hifi_pll->pll_clk;


	do {
		writel(hifi_pll->pll_cntl0, HHI_HIFI_PLL_CNTL0);
		writel(hifi_pll->pll_cntl0 | (3 << 28), HHI_HIFI_PLL_CNTL0);
		writel(hifi_pll->pll_cntl1, HHI_HIFI_PLL_CNTL1);
		writel(hifi_pll->pll_cntl2, HHI_HIFI_PLL_CNTL2);
		writel(hifi_pll->pll_cntl3, HHI_HIFI_PLL_CNTL3);
		writel(hifi_pll->pll_cntl4, HHI_HIFI_PLL_CNTL4);
		writel(hifi_pll->pll_cntl5, HHI_HIFI_PLL_CNTL5);
		writel(hifi_pll->pll_cntl6, HHI_HIFI_PLL_CNTL6);
		writel(readl(HHI_HIFI_PLL_CNTL0)|(1<<29), HHI_HIFI_PLL_CNTL0);
		_udelay(10);
		writel(readl(HHI_HIFI_PLL_CNTL0)&(~(1<<29)), HHI_HIFI_PLL_CNTL0);
		_udelay(100);
		//printf("hifi lock_check: %4d\n", lock_check);
	} while ((!((readl(HHI_HIFI_PLL_CNTL0)>>31)&0x1)) && --lock_check);
	if (0 == lock_check) {
		printf("HIFI pll lock Failed! - %4d MHz\n", pll_clk);
		ret = RET_PLL_LOCK_FAIL;
	} else {
		printf("HIFI pll lock OK! - %4d MHz.Div8  - %4d MHz.", pll_clk, pll_clk >> od);
		/* get hifi_pll_clk */
		clk_msr_val = clk_util_clk_msr(12);
		printf("CLKMSR(12) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, pll_clk >> od)) {
			printf(": Match\n");
		} else {
			printf(": MisMatch\n");
			ret = RET_CLK_NOT_MATCH;
		}
	}
	return ret;
}

static int hifi_pll_test_all(hifi_pll_cfg_t * hifi_pll_cfg) {
	unsigned int i=0;
	int ret=0;

	for (i=0; i<(sizeof(hifi_pll_clk)/sizeof(uint32_t)); i++)
		ret += hifi_pll_test(&(hifi_pll_cfg->hifi_pll[i]));
	return ret;
}

static int pcie_pll_test(pcie_pll_set_t * pcie_pll) {
	int ret=0;
	unsigned int i=0, pll_clk=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0;

	for (i=0; i<(sizeof(pcie_pll_rate_table)/sizeof(pcie_pll_rate_table[0])); i++) {
		if ((pcie_pll->pll_cntl0 & 0xFF) == pcie_pll_rate_table[i].m / 2)
			pll_clk = pcie_pll_rate_table[i].rate;
	}

	writel(pcie_pll->pll_cntl0, HHI_PCIE_PLL_CNTL0);
	writel(pcie_pll->pll_cntl0 | 0x30000000, HHI_PCIE_PLL_CNTL0);
	writel(pcie_pll->pll_cntl1, HHI_PCIE_PLL_CNTL1);
	writel(pcie_pll->pll_cntl2, HHI_PCIE_PLL_CNTL2);
	writel(pcie_pll->pll_cntl3, HHI_PCIE_PLL_CNTL3);
	writel(pcie_pll->pll_cntl4, HHI_PCIE_PLL_CNTL4);
	writel(pcie_pll->pll_cntl5, HHI_PCIE_PLL_CNTL5);
	writel(pcie_pll->pll_cntl5 | 0x68, HHI_PCIE_PLL_CNTL5);
	_udelay(20);
	writel(pcie_pll->pll_cntl4 | 0x00800000, HHI_PCIE_PLL_CNTL4);
	_udelay(10);
	writel(pcie_pll->pll_cntl0 | 0x34000000, HHI_PCIE_PLL_CNTL0);
	writel(((pcie_pll->pll_cntl0 & (~(1<<29)))|(1<<26))|(1<<28), HHI_PCIE_PLL_CNTL0);
	_udelay(10);
	writel(pcie_pll->pll_cntl2 & (~(1<<8)), HHI_PCIE_PLL_CNTL2);

	lock_check = PLL_LOCK_CHECK_MAX;
	do {
		update_bits(HHI_PCIE_PLL_CNTL0, 1<<29, 1 << 29);
		_udelay(10);
		update_bits(HHI_PCIE_PLL_CNTL0, 1<<29, 0);
		_udelay(100);
		//printf("pcie lock_check: %4d\n", lock_check);
	} while ((!((readl(HHI_PCIE_PLL_CNTL0)>>31)&0x1)) && --lock_check);
	if (0 == lock_check) {
		printf("PCIE pll lock Failed! - %4d MHz\n", pll_clk);
		ret = RET_PLL_LOCK_FAIL;
	} else {
		printf("PCIE pll lock OK! - %4d MHz.Div48  - %4d MHz. ", pll_clk, pll_clk / 48);
		/* get pcie_pll_clk */
		clk_msr_val = clk_util_clk_msr(29);
		printf("CLKMSR(29) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, pll_clk / 48)) {
			printf(": Match\n");
		} else {
			printf(": MisMatch\n");
			ret = RET_CLK_NOT_MATCH;
		}
	}
	return ret;
}

static int pcie_pll_test_all(void) {
	unsigned int i=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0;
	int ret=0;

	for (i=0; i<(sizeof(pcie_pll_rate_table)/sizeof(pcie_pll_rate_table[0])); i++) {
		writel(0x28060464, HHI_PCIE_PLL_CNTL0);
		writel(0x38060464, HHI_PCIE_PLL_CNTL0);
		writel(0x00000000, HHI_PCIE_PLL_CNTL1);
		writel(0x00001100, HHI_PCIE_PLL_CNTL2);
		writel(0x10058e00, HHI_PCIE_PLL_CNTL3);
		writel(0x000100c0, HHI_PCIE_PLL_CNTL4);
		writel(0x68000048, HHI_PCIE_PLL_CNTL5);
		writel(0x68000068, HHI_PCIE_PLL_CNTL5);
		_udelay(20);
		writel(0x008100c0, HHI_PCIE_PLL_CNTL4);
		_udelay(10);
		writel(0x3c060464, HHI_PCIE_PLL_CNTL0);
		writel(0x1c060464, HHI_PCIE_PLL_CNTL0);
		_udelay(10);
		writel(0x00001000, HHI_PCIE_PLL_CNTL2);

		lock_check = PLL_LOCK_CHECK_MAX;
		do {
			update_bits(HHI_PCIE_PLL_CNTL0, 1<<29, 1 << 29);
			_udelay(10);
			update_bits(HHI_PCIE_PLL_CNTL0, 1<<29, 0);
			_udelay(100);
			//printf("pcie lock_check: %4d\n", lock_check);
		} while ((!((readl(HHI_PCIE_PLL_CNTL0)>>31)&0x1)) && --lock_check);
		if (0 == lock_check) {
			printf("pcie pll lock Failed! - %4d MHz\n", pcie_pll_rate_table[i].rate);
			ret += RET_PLL_LOCK_FAIL;
		} else {
			printf("pcie pll lock OK! - %4d MHz.Div48  - %4d MHz.",
				pcie_pll_rate_table[i].rate,
				pcie_pll_rate_table[i].rate / 48);
			/* get pcie_pll_clk */
			clk_msr_val = clk_util_clk_msr(29);
			printf("CLKMSR(29) - %4d MHz ", clk_msr_val);
			if (clk_around(clk_msr_val,
				       pcie_pll_rate_table[i].rate / 48)) {
				printf(": Match\n");
			} else {
				printf(": MisMatch\n");
				ret += RET_CLK_NOT_MATCH;
			}
		}
	}

	return ret;

}

static int ethphy_pll_test(ethphy_pll_set_t * ethphy_pll) {
	int ret=0;
	unsigned int pll_clk=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0;

	if ((ethphy_pll->pll_cntl0 & 0x1FF) == 0xA) {
		pll_clk = 500;
	} else {
		printf("input frequency point is not support\n");
		return -1;
	}

	do {
		writel(ethphy_pll->pll_cntl0, ETH_PLL_CTL0);
		writel(ethphy_pll->pll_cntl1, ETH_PLL_CTL1);
		writel(ethphy_pll->pll_cntl2, ETH_PLL_CTL2);
		writel(ethphy_pll->pll_cntl3, ETH_PLL_CTL3);
		_udelay(150);
		writel(ethphy_pll->pll_cntl0 | 0x10000000, ETH_PLL_CTL0);
		_udelay(150);
	} while ((!((readl(ETH_PLL_CTL0)>>30)&0x1))&& --lock_check);

	if (0 == lock_check) {
		printf("ETHPHY pll lock Failed! - %4d MHz\n", pll_clk);
		ret = RET_PLL_LOCK_FAIL;
	} else {
		printf("ETHPHY pll lock OK! - %4d MHz. ", pll_clk);
		/* get ethphy_pll_clk */
		clk_msr_val = clk_util_clk_msr(95)<<2;
		printf("CLKMSR(95) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, pll_clk)) {
			printf(": Match\n");
		} else {
			printf(": MisMatch\n");
			ret = RET_CLK_NOT_MATCH;
		}
	}
	return ret;
}


static int ethphy_pll_test_all(void) {
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	unsigned int clk_msr_val = 0;
	int ret=0;

	do {
		writel(0x9c0040a | 0x30000000, ETH_PLL_CTL0);
		writel(0x927e0000, ETH_PLL_CTL1);
		writel(0xac5f49e5, ETH_PLL_CTL2);
		writel(0x00000000, ETH_PLL_CTL3);
		_udelay(150);
		writel(0x9c0040a | 0x10000000, ETH_PLL_CTL0);
		_udelay(150);
		} while ((!((readl(ETH_PLL_CTL0)>>30)&0x1))&& --lock_check);

	if (0 == lock_check) {
		printf("ethphy pll lock Failed! - 500MHz\n");
		ret += RET_PLL_LOCK_FAIL;
	} else {
		printf("ethphy pll lock OK! - 500MHz. ");
		/* get ethphy_pll_clk */
		clk_msr_val = clk_util_clk_msr(95)<<2;
		printf("CLKMSR(95) - %4d MHz ", clk_msr_val);
		if (clk_around(clk_msr_val, 500)) {
			printf(": Match\n");
		} else {
			printf(": MisMatch\n");
			ret += RET_CLK_NOT_MATCH;
		}
	}

	return ret;
}


static int usbphy_pll_test(usbphy_pll_set_t * usbphy_pll) {
	int ret=0;
	unsigned int i=0, pll_clk=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;

	if ((usbphy_pll->pll_cntl4 & 0x1FF) == 0x14) {
		pll_clk = 480;
	}
	else
	{
		printf("input frequency point is not support\n");
		return -1;
	}
	for (i=0; i<(sizeof(usbphy_base_cfg)/sizeof(usbphy_base_cfg[0])); i++) {
		/* TO DO set usb  PLL */
		writel(usbphy_pll->pll_cntl4, (usbphy_base_cfg[i] + 0x40));
		writel(usbphy_pll->pll_cntl5, (usbphy_base_cfg[i] + 0x44));
		writel(usbphy_pll->pll_cntl6, (usbphy_base_cfg[i] + 0x48));
		/* PHY Tune */
		writel(usbphy_pll->pll_cntl7, (usbphy_base_cfg[i] + 0x50));
		writel(usbphy_pll->pll_cntl0, (usbphy_base_cfg[i] + 0x10));
		/* Recovery analog status */
		writel(usbphy_pll->pll_cntl3, (usbphy_base_cfg[i] + 0x38));
		writel(usbphy_pll->pll_cntl2, (usbphy_base_cfg[i] + 0x34));

		writel(usbphy_pll->pll_cntl1, (usbphy_base_cfg[i] + 0xC));

		lock_check = PLL_LOCK_CHECK_MAX;
		do {
			update_bits((usbphy_base_cfg[i] + 0x40), 1<<29, 1 << 29);
			_udelay(10);
			update_bits((usbphy_base_cfg[i] + 0x40), 1<<29, 0);
			_udelay(100);
			//printf("ethphy lock_check: %4d\n", lock_check);
		} while ((!((readl(usbphy_base_cfg[i] + 0x40)>>31)&0x1)) && --lock_check);
		if (0 == lock_check) {
			printf("usbphy%d pll lock Failed! - %4d MHz\n", i+20, pll_clk);
			ret = RET_PLL_LOCK_FAIL;
		}
		else{
			printf("usbphy%d pll lock OK! - %4d MHz.\n", i+20,pll_clk);
		}

	}
	return ret;
}


static int usbphy_pll_test_all(void) {
	unsigned int i=0;
	unsigned int lock_check = PLL_LOCK_CHECK_MAX;
	int ret=0;

	for (i=0; i<(sizeof(usbphy_base_cfg)/sizeof(usbphy_base_cfg[0])); i++) {
		/* TO DO set usb  PLL */
		writel(0x09400414, (usbphy_base_cfg[i] + 0x40));
		writel(0x927E0000, (usbphy_base_cfg[i] + 0x44));
		writel(0xac5f69e5, (usbphy_base_cfg[i] + 0x48));
		/* PHY Tune */
		writel(0xfe18, (usbphy_base_cfg[i] + 0x50));
		writel(0x8000fff, (usbphy_base_cfg[i] + 0x10));
		/* Recovery analog status */
		writel(0, (usbphy_base_cfg[i] + 0x38));
		writel(0x78000, (usbphy_base_cfg[i] + 0x34));

		writel(0x34, (usbphy_base_cfg[i] + 0xC));

		do {
			update_bits((usbphy_base_cfg[i] + 0x40), 1<<29, 1 << 29);
			_udelay(10);
			update_bits((usbphy_base_cfg[i] + 0x40), 1<<29, 0);
			_udelay(100);
			//printf("ethphy lock_check: %4d\n", lock_check);
		} while ((!((readl(usbphy_base_cfg[i] + 0x40)>>31)&0x1)) && --lock_check);

		if (0 == lock_check) {
			printf("usbphy%d pll lock Failed! - 480MHz\n", i+20);
			ret += RET_PLL_LOCK_FAIL;
		} else {
			printf("usbphy%d pll lock OK! - 480MHz.\n", i+20);
		}
	}

	return ret;

}


static int pll_test_all(unsigned char * pll_list) {
	int ret = 0;
	unsigned char i=0;
	for (i=0; i<PLL_ENUM; i++) {
		switch (pll_list[i]) {
			case PLL_SYS:
				ret = sys_pll_test_all(&sys_pll_cfg);
				pll_report(ret, STR_PLL_TEST_SYS);
				break;
			case PLL_FIX:
				ret = fix_pll_test();
				pll_report(ret, STR_PLL_TEST_FIX);
				break;
			case PLL_DDR:
				ret = ddr_pll_test();
				pll_report(ret, STR_PLL_TEST_DDR);
				break;
			case PLL_HDMI:
				ret = hdmi_pll_test_all(&hdmi_pll_cfg);
				pll_report(ret, STR_PLL_TEST_HDMI);
				break;
			case PLL_GP0:
				ret = gp0_pll_test_all(&gp0_pll_cfg);
				pll_report(ret, STR_PLL_TEST_GP0);
				break;
			case PLL_HIFI:
				ret = hifi_pll_test_all(&hifi_pll_cfg);
				pll_report(ret, STR_PLL_TEST_HIFI);
				break;
			case PLL_PCIE:
				ret = pcie_pll_test_all();
				pll_report(ret, STR_PLL_TEST_PCIE);
				break;
			case PLL_ETHPHY:
				ret = ethphy_pll_test_all();
				pll_report(ret, STR_PLL_TEST_ETHPHY);
				break;
			case PLL_USBPHY:
				ret = usbphy_pll_test_all();
				pll_report(ret, STR_PLL_TEST_USBPHY);
				break;
			default:
				break;
		}
	}
	return ret;
}

int pll_test(int argc, char * const argv[])
{
	int ret = 0;
	sys_pll_set_t sys_pll_set = {0};
	hdmi_pll_set_t hdmi_pll_set = {0};
	gp0_pll_set_t gp0_pll_set = {0};
	hifi_pll_set_t hifi_pll_set = {0};
	pcie_pll_set_t pcie_pll_set = {0};
	ethphy_pll_set_t ethphy_pll_set = {0};
	usbphy_pll_set_t usbphy_pll_set = {0};

	unsigned char plls[PLL_ENUM] = {
		PLL_SYS,
		0xff,//	PLL_FIX, //0xff will skip this pll
		0xff,//	PLL_DDR,
		PLL_HDMI,
		PLL_GP0,
		PLL_HIFI,
		PLL_PCIE,
		PLL_ETHPHY,
		PLL_USBPHY
	};

	if (0 == strcmp(STR_PLL_TEST_ALL, argv[1])) {
		printf("Test all plls\n");
		pll_test_all(plls);
	} else if(0 == strcmp(STR_PLL_TEST_SYS, argv[1])) {
		if (argc == 2) {
			ret = sys_pll_test_all(&sys_pll_cfg);
			pll_report(ret, STR_PLL_TEST_SYS);
		} else if (argc != 9){
			printf("%s pll test: args error\n", STR_PLL_TEST_SYS);
			return -1;
		} else {
			sys_pll_set.pll_cntl = simple_strtoul(argv[2], NULL, 16);
			sys_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			sys_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			sys_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			sys_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			sys_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			sys_pll_set.pll_cntl6 = simple_strtoul(argv[8], NULL, 16);
			ret = sys_pll_test(&sys_pll_set);
			pll_report(ret, STR_PLL_TEST_SYS);
		}
	} else if (0 == strcmp(STR_PLL_TEST_HDMI, argv[1])) {
		if (argc == 2) {
			ret = hdmi_pll_test_all(&hdmi_pll_cfg);
			pll_report(ret, STR_PLL_TEST_HDMI);
		} else if (argc != 9){
			printf("%s pll test: args error\n", STR_PLL_TEST_HDMI);
			return -1;
		} else {
			hdmi_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			hdmi_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			hdmi_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			hdmi_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			hdmi_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			hdmi_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			hdmi_pll_set.pll_cntl6 = simple_strtoul(argv[8], NULL, 16);
			ret = hdmi_pll_test(&hdmi_pll_set);
			pll_report(ret, STR_PLL_TEST_HDMI);
		}
	} else if (0 == strcmp(STR_PLL_TEST_GP0, argv[1])) {
		if (argc == 2) {
			ret = gp0_pll_test_all(&gp0_pll_cfg);
			pll_report(ret, STR_PLL_TEST_GP0);
		} else if (argc != 9){
			printf("%s pll test: args error\n", STR_PLL_TEST_GP0);
			return -1;
		} else {
			gp0_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			gp0_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			gp0_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			gp0_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			gp0_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			gp0_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			gp0_pll_set.pll_cntl6 = simple_strtoul(argv[8], NULL, 16);
			ret = gp0_pll_test(&gp0_pll_set);
			pll_report(ret, STR_PLL_TEST_GP0);
		}
	} else if (0 == strcmp(STR_PLL_TEST_HIFI, argv[1])) {
		if (argc == 2) {
			ret = hifi_pll_test_all(&hifi_pll_cfg);
			pll_report(ret, STR_PLL_TEST_HIFI);
		} else if (argc != 9){
			printf("%s pll test: args error\n", STR_PLL_TEST_HIFI);
			return -1;
		} else {
			hifi_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			hifi_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			hifi_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			hifi_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			hifi_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			hifi_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			hifi_pll_set.pll_cntl6 = simple_strtoul(argv[8], NULL, 16);
			ret = hifi_pll_test(&hifi_pll_set);
			pll_report(ret, STR_PLL_TEST_HIFI);
		}
	} else if (0 == strcmp(STR_PLL_TEST_PCIE, argv[1])) {
		if (argc == 2) {
			ret = pcie_pll_test_all();
			pll_report(ret, STR_PLL_TEST_PCIE);
		} else if (argc != 8){
			printf("%s pll test: args error\n", STR_PLL_TEST_PCIE);
			return -1;
		} else {
			pcie_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			pcie_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			pcie_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			pcie_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			pcie_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			pcie_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			ret = pcie_pll_test(&pcie_pll_set);
			pll_report(ret, STR_PLL_TEST_PCIE);
		}
	} else if (0 == strcmp(STR_PLL_TEST_ETHPHY, argv[1])) {
		if (argc == 2) {
			ret = ethphy_pll_test_all();
			pll_report(ret, STR_PLL_TEST_ETHPHY);
		} else if (argc != 6){
			printf("%s pll test: args error\n", STR_PLL_TEST_ETHPHY);
			return -1;
		} else {
			ethphy_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			ethphy_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			ethphy_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			ethphy_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			ret = ethphy_pll_test(&ethphy_pll_set);
			pll_report(ret, STR_PLL_TEST_ETHPHY);
		}
	} else if (0 == strcmp(STR_PLL_TEST_USBPHY, argv[1])) {
		if (argc == 2) {
			ret = usbphy_pll_test_all();
			pll_report(ret, STR_PLL_TEST_USBPHY);
		} else if (argc != 10){
			printf("%s pll test: args error\n", STR_PLL_TEST_USBPHY);
			return -1;
		} else {
			usbphy_pll_set.pll_cntl0 = simple_strtoul(argv[2], NULL, 16);
			usbphy_pll_set.pll_cntl1 = simple_strtoul(argv[3], NULL, 16);
			usbphy_pll_set.pll_cntl2 = simple_strtoul(argv[4], NULL, 16);
			usbphy_pll_set.pll_cntl3 = simple_strtoul(argv[5], NULL, 16);
			usbphy_pll_set.pll_cntl4 = simple_strtoul(argv[6], NULL, 16);
			usbphy_pll_set.pll_cntl5 = simple_strtoul(argv[7], NULL, 16);
			usbphy_pll_set.pll_cntl6 = simple_strtoul(argv[8], NULL, 16);
			usbphy_pll_set.pll_cntl7 = simple_strtoul(argv[9], NULL, 16);
			ret = usbphy_pll_test(&usbphy_pll_set);
			pll_report(ret, STR_PLL_TEST_USBPHY);
		}
	} else if (0 == strcmp(STR_PLL_TEST_DDR, argv[1])) {
		printf("%s pll not support now\n", STR_PLL_TEST_DDR);
		return -1;
	} else if (0 == strcmp(STR_PLL_TEST_FIX, argv[1])) {
		printf("%s pll not support now\n", STR_PLL_TEST_FIX);
		return -1;
	}

	return 0;
}

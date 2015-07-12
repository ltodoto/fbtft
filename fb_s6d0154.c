/*
 * FB driver for the S6D0154X LCD Controller
 *
 * Copyright (C) 2015 Alter Table
 *
 * Based on driver for the ILI9320 LCD Controller
 *
 * Copyright (C) 2013 Noralf Tronnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_s6d0154"
#define WIDTH		240
#define HEIGHT		320
#define DEFAULT_GAMMA	"00 10 0 0 1 1 3 6 8 4\n" \
			            "10 00 3 3 5 6 6 4 3 3"


static unsigned read_devicecode(struct fbtft_par *par)
{
	int ret;
	u8 rxbuf[4] = {0, };

	write_reg(par, 0x0000);
	ret = par->fbtftops.read(par, rxbuf, 2);
	return (rxbuf[0] << 8) | rxbuf[1];
}

static int init_display(struct fbtft_par *par)
{
	unsigned devcode;

	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	par->fbtftops.reset(par);

	devcode = read_devicecode(par);
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "Device code: 0x%04X\n",
		devcode);
	if ((devcode != 0x0000) && (devcode != 0x0154))
		dev_warn(par->info->device,
			"Unrecognized Device code: 0x%04X (expected 0x0154)\n",
			devcode);

	/* Initialization sequence */

	/* ***********Power On sequence *************** */
	/* DC1[2:0], DC0[2:0], VC[2:0] */
	write_reg(par, 0x0011, 0x001A);

	/* VREG1OUT voltage */
	write_reg(par, 0x0012, 0x3121);

	/* VDV[4:0] for VCOM amplitude */
	write_reg(par, 0x0013, 0x006C);
    
    /* VDV[4:0] for VCOM amplitude */
	write_reg(par, 0x0014, 0x4249);

	/* SAP, AP, DSTB, STB */
	write_reg(par, 0x0010, 0x0800);

	/* R11h=0x0031 at VCI=3.3V DC1[2:0], DC0[2:0], VC[2:0] */
	write_reg(par, 0x0011, 0x011A);
	mdelay(10);

	/* R11h=0x0138 at VCI=3.3V VREG1OUT voltage */
	write_reg(par, 0x0011, 0x031A);
	mdelay(10);

	/* R11h=0x1800 at VCI=3.3V VDV[4:0] for VCOM amplitude */
	write_reg(par, 0x0011, 0x071A);
	mdelay(10);

	/* R21h=0x0008 at VCI=3.3V VCM[4:0] for VCOMH */
	write_reg(par, 0x0011, 0x0F1A);
	mdelay(10);
	
	write_reg(par, 0x0011, 0x0F3A);
	mdelay(30);

	/* set SS bit and driving lines (720x320, use 320 drive lines) */
	write_reg(par, 0x0001, 0x0128);

	/* no line inversion, FLD = 0 */
	write_reg(par, 0x0002, 0x0300);

	/* Set BGR byte order, horz inc, vert inc, AM = 0 */
	write_reg(par, 0x0003, 0x1030);
	
	/* Disable display output */
	write_reg(par, 0x0007, 0x0000);

	/* set blank period for front and back porch (8/8 raster periods) */
	write_reg(par, 0x0008, 0x0808);

	/* Frame cycle control (1/1/16 input clk) */
	write_reg(par, 0x000B, 0x1100);

	/* RGB interface setting (system interface, internal clock) */
	write_reg(par, 0x000C, 0x0000);

	/* VCI recycling setting (multiplier = 2) */
	write_reg(par, 0x0015, 0x0020);

	/* GRAM horizontal Address */
	write_reg(par, 0x0020, 0x0000);

	/* GRAM Vertical Address */
	write_reg(par, 0x0021, 0x0000);

	/* ------------------ Set GRAM area --------------- */
	/* Horizontal GRAM Start Address */
	write_reg(par, 0x0037, 0x0000);

	/* Horizontal GRAM End Address */
	write_reg(par, 0x0036, 0x00EF);

	/* Vertical GRAM Start Address */
	write_reg(par, 0x0039, 0x0000);

	/* Vertical GRAM End Address */
	write_reg(par, 0x0038, 0x013F);

	/* Start internal OSC. */
	write_reg(par, 0x000F, 0x0801);

	write_reg(par, 0x0007, 0x0016);
	write_reg(par, 0x0007, 0x0017); /* 262K color and display ON */

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par,
		"%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);

	switch (par->info->var.rotate) {
	/* R20h = Horizontal GRAM Start Address */
	/* R21h = Vertical GRAM Start Address */
	case 0:
		write_reg(par, 0x0020, xs);
		write_reg(par, 0x0021, ys);
		break;
	case 90:
		write_reg(par, 0x0020, ys);
		write_reg(par, 0x0021, HEIGHT - 1 - xs);
		break;
	case 180:
		write_reg(par, 0x0020, WIDTH - 1 - xs);
		write_reg(par, 0x0021, HEIGHT - 1 - ys);
		break;
	case 270:
		write_reg(par, 0x0020, WIDTH - 1 - ys);
		write_reg(par, 0x0021, xs);
		break;
	}
	write_reg(par, 0x0022); /* Write Data to GRAM */
}

static int set_var(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x0003, (par->bgr << 12) | 0x30);
		break;
	case 90:
		write_reg(par, 0x0003, (par->bgr << 12) | 0x18);
		break;
	case 180:
		write_reg(par, 0x0003, (par->bgr << 12) | 0x00);
		break;
	case 270:
		write_reg(par, 0x0003, (par->bgr << 12) | 0x28);
		break;
	}
	return 0;
}

/*
  Gamma string format:
    VRP0 VRP1 RP0 RP1 KP0 KP1 KP2 KP3 KP4 KP5
    VRN0 VRN1 RN0 RN1 KN0 KN1 KN2 KN3 KN4 KN5
*/
#define CURVE(num, idx)  curves[num*par->gamma.num_values + idx]
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	unsigned long mask[] = {
		0x1f, 0x1f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
		0x1f, 0x1f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	};
	int i, j;

	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	/* apply mask */
	for (i = 0; i < 2; i++)
		for (j = 0; j < 10; j++)
			CURVE(i, j) &= mask[i*par->gamma.num_values + j];

	write_reg(par, 0x0050, CURVE(0, 5) << 8 | CURVE(0, 4));
	write_reg(par, 0x0051, CURVE(0, 7) << 8 | CURVE(0, 6));
	write_reg(par, 0x0052, CURVE(0, 9) << 8 | CURVE(0, 8));
	write_reg(par, 0x0053, CURVE(0, 3) << 8 | CURVE(0, 2));
	write_reg(par, 0x0058, CURVE(0, 1) << 8 | CURVE(0, 0));

	write_reg(par, 0x0054, CURVE(1, 5) << 8 | CURVE(1, 4));
	write_reg(par, 0x0055, CURVE(1, 7) << 8 | CURVE(1, 6));
	write_reg(par, 0x0056, CURVE(1, 9) << 8 | CURVE(1, 8));
	write_reg(par, 0x0057, CURVE(1, 3) << 8 | CURVE(1, 2));
	write_reg(par, 0x0059, CURVE(1, 1) << 8 | CURVE(1, 0));

	return 0;
}
#undef CURVE


static struct fbtft_display display = {
	.regwidth = 16,
	.width = WIDTH,
	.height = HEIGHT,
	.gamma_num = 2,
	.gamma_len = 10,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};
FBTFT_REGISTER_DRIVER(DRVNAME, "samsung,s6d0154", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:s6d0154");
MODULE_ALIAS("platform:s6d0154");

MODULE_DESCRIPTION("FB driver for the S6D0154X LCD Controller");
MODULE_AUTHOR("Alter Table");
MODULE_LICENSE("GPL");

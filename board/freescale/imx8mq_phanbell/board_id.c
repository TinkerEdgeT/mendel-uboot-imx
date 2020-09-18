/*
 * Copyright 2019 Google LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "board_id.h"

#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/imx-regs.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/gpio.h>
#include <asm/gpio.h>
#include <asm/io.h>

#define BOARD_ID_GPIOS_PORT 3
#define BOARD_ID_GPIOS_BASE GPIO3_BASE_ADDR
#define BOARD_ID_GPIO1_INDEX 22
#define BOARD_ID_GPIO2_INDEX 24
#define BOARD_ID_GPIO3_INDEX 19
#define BOARD_ID_GPIO1 IMX_GPIO_NR(BOARD_ID_GPIOS_PORT, BOARD_ID_GPIO1_INDEX)
#define BOARD_ID_GPIO2 IMX_GPIO_NR(BOARD_ID_GPIOS_PORT, BOARD_ID_GPIO2_INDEX)
#define BOARD_ID_GPIO3 IMX_GPIO_NR(BOARD_ID_GPIOS_PORT, BOARD_ID_GPIO3_INDEX)

#define BASEBOARD_ID_GPIOS_PORT 3
#define BASEBOARD_ID_GPIOS_BASE GPIO3_BASE_ADDR
#define BASEBOARD_ID_GPIO1_INDEX 25
#define BASEBOARD_ID_GPIO2_INDEX 23
#define BASEBOARD_ID_GPIO3_INDEX 20
#define BASEBOARD_ID_GPIO4_INDEX 21
#define BASEBOARD_ID_GPIO1 IMX_GPIO_NR(BASEBOARD_ID_GPIOS_PORT, BASEBOARD_ID_GPIO1_INDEX)
#define BASEBOARD_ID_GPIO2 IMX_GPIO_NR(BASEBOARD_ID_GPIOS_PORT, BASEBOARD_ID_GPIO2_INDEX)
#define BASEBOARD_ID_GPIO3 IMX_GPIO_NR(BASEBOARD_ID_GPIOS_PORT, BASEBOARD_ID_GPIO3_INDEX)
#define BASEBOARD_ID_GPIO4 IMX_GPIO_NR(BASEBOARD_ID_GPIOS_PORT, BASEBOARD_ID_GPIO4_INDEX)

static iomux_v3_cfg_t const board_id_pads[] = {
	IMX8MQ_PAD_SAI5_RXD1__GPIO3_IO22 | MUX_PAD_CTRL(PAD_CTL_DSE0),
	IMX8MQ_PAD_SAI5_RXD3__GPIO3_IO24 | MUX_PAD_CTRL(PAD_CTL_DSE0),
	IMX8MQ_PAD_SAI5_RXFS__GPIO3_IO19 | MUX_PAD_CTRL(PAD_CTL_DSE0),
};

static iomux_v3_cfg_t const board_id_default_pads[] = {
	IMX8MQ_PAD_SAI5_RXD1__SAI5_RX_DATA1 | MUX_PAD_CTRL(0x1816),
	IMX8MQ_PAD_SAI5_RXD3__SAI5_RX_DATA3 | MUX_PAD_CTRL(0x16),
	IMX8MQ_PAD_SAI5_RXFS__SAI5_RX_SYNC | MUX_PAD_CTRL(0x16),
};

static iomux_v3_cfg_t const baseboard_id_pads[] = {
	IMX8MQ_PAD_SAI5_MCLK__GPIO3_IO25 | MUX_PAD_CTRL(PAD_CTL_DSE0),
	IMX8MQ_PAD_SAI5_RXD2__GPIO3_IO23 | MUX_PAD_CTRL(PAD_CTL_DSE0),
	IMX8MQ_PAD_SAI5_RXC__GPIO3_IO20 | MUX_PAD_CTRL(PAD_CTL_DSE0),
	IMX8MQ_PAD_SAI5_RXD0__GPIO3_IO21 | MUX_PAD_CTRL(PAD_CTL_DSE0),
};

static iomux_v3_cfg_t const baseboard_id_default_pads[] = {
	IMX8MQ_PAD_SAI5_MCLK__SAI5_MCLK | MUX_PAD_CTRL(0x16),
	IMX8MQ_PAD_SAI5_RXD2__SAI5_RX_DATA2 | MUX_PAD_CTRL(0x16),
	IMX8MQ_PAD_SAI5_RXC__SAI5_RX_BCLK | MUX_PAD_CTRL(0x16),
	IMX8MQ_PAD_SAI5_RXD0__SAI5_RX_DATA0 | MUX_PAD_CTRL(0x16),
};

int get_board_id() {
	int board_id = 0;

	imx_iomux_v3_setup_multiple_pads(board_id_pads,
		ARRAY_SIZE(board_id_pads));

#ifdef CONFIG_SPL_BUILD
	gpio_request(BOARD_ID_GPIO1, "board_id_1");
	gpio_direction_input(BOARD_ID_GPIO1);
	gpio_request(BOARD_ID_GPIO2, "board_id_2");
	gpio_direction_input(BOARD_ID_GPIO2);
	gpio_request(BOARD_ID_GPIO3, "board_id_3");
	gpio_direction_input(BOARD_ID_GPIO3);

	board_id = gpio_get_value(BOARD_ID_GPIO1);
	board_id |= gpio_get_value(BOARD_ID_GPIO2) << 1;
	board_id |= gpio_get_value(BOARD_ID_GPIO3) << 2;

	gpio_free(BOARD_ID_GPIO1);
	gpio_free(BOARD_ID_GPIO2);
	gpio_free(BOARD_ID_GPIO3);
#else
	struct gpio_regs *regs = (struct gpio_regs *)BOARD_ID_GPIOS_BASE;
	board_id = (readl(&regs->gpio_dr) >> BOARD_ID_GPIO1_INDEX) & 0x01;
	board_id |= (readl(&regs->gpio_dr) >> (BOARD_ID_GPIO2_INDEX - 1)) & 0x02;
	board_id |= (readl(&regs->gpio_dr) >> (BOARD_ID_GPIO3_INDEX - 2)) & 0x04;
#endif

	imx_iomux_v3_setup_multiple_pads(board_id_default_pads,
		ARRAY_SIZE(board_id_default_pads));

	return board_id;
}

int get_baseboard_id() {
	int baseboard_id = -1;

	imx_iomux_v3_setup_multiple_pads(baseboard_id_pads, ARRAY_SIZE(baseboard_id_pads));

#ifdef CONFIG_SPL_BUILD
	gpio_request(BASEBOARD_ID_GPIO1, "baseboard_id_1");
	gpio_direction_input(BASEBOARD_ID_GPIO1);
	gpio_request(BASEBOARD_ID_GPIO2, "baseboard_id_2");
	gpio_direction_input(BASEBOARD_ID_GPIO2);
	gpio_request(BASEBOARD_ID_GPIO3, "baseboard_id_3");
	gpio_direction_input(BASEBOARD_ID_GPIO3);

	baseboard_id = gpio_get_value(BASEBOARD_ID_GPIO1);
	baseboard_id |= gpio_get_value(BASEBOARD_ID_GPIO2) << 1;
	baseboard_id |= gpio_get_value(BASEBOARD_ID_GPIO3) << 2;

	gpio_free(BASEBOARD_ID_GPIO1);
	gpio_free(BASEBOARD_ID_GPIO2);
	gpio_free(BASEBOARD_ID_GPIO3);
#else

	struct gpio_regs *regs = (struct gpio_regs *)BASEBOARD_ID_GPIOS_BASE;
	baseboard_id = (readl(&regs->gpio_dr) >> BASEBOARD_ID_GPIO1_INDEX) & 0x01;
	baseboard_id |= (readl(&regs->gpio_dr) >> (BASEBOARD_ID_GPIO2_INDEX - 1)) & 0x02;
	baseboard_id |= (readl(&regs->gpio_dr) >> (BASEBOARD_ID_GPIO3_INDEX - 2)) & 0x04;
#endif

	imx_iomux_v3_setup_multiple_pads(baseboard_id_default_pads,
		ARRAY_SIZE(baseboard_id_default_pads));

	return baseboard_id;
}

phys_size_t get_ddr_size(void) {
	const phys_size_t k1Gb = 0x40000000;
	const phys_size_t k2Gb = 0x80000000;
	const phys_size_t k3Gb = 0xC0000000;
	const phys_size_t k4Gb = 0x100000000;
	phys_size_t ram_size = 0;
	const int board_id = get_board_id();

	switch (board_id) {
		case 0:
		// Todo(pnordstrom) Board id 0 should be 4 Gb but currently
		// only 3Gb works due to 32 bit checks all over u-boot and SDRAM
		// starting at 1 Gb
			ram_size = k4Gb;
			break;
		case 1:
		case 2:
		case 5:
		case 6:
			ram_size = k1Gb;
			break;
		case 3:
			ram_size = k2Gb;
			break;
		case 4:
		case 7:
			ram_size = k3Gb;
			break;
		default:
			break;
	}

	return ram_size;
}

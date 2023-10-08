// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */
 
#ifndef _PINCTRL_HAILO15_H
#define _PINCTRL_HAILO15_H

struct pinctrl_pin_desc
{
	unsigned number;
	const char *name;
	void *drv_data;
};

#define PINCTRL_PIN(a, b)      \
	{                          \
		.number = a, .name = b \
	}

#define H15_PINMUX_PIN_COUNT (32)
#define H15_GENERAL_PIN_COUNT (44)
#define H15_PINCTRL_PIN_COUNT (H15_PINMUX_PIN_COUNT + H15_GENERAL_PIN_COUNT)

#define GPIO_PADS_CONFIG__GPIO_PE (0x0)
#define GPIO_PADS_CONFIG__GPIO_PS (0x4)

#define GENERAL_PADS_CONFIG__DS_SHIFT (11)
#define GENERAL_PADS_CONFIG__DS_MASK (0xF << GENERAL_PADS_CONFIG__DS_SHIFT)
#define GENERAL_PADS_CONFIG__DS_MODIFY(src, dst)    \
	(dst = ((dst & ~GENERAL_PADS_CONFIG__DS_MASK) | \
			(src) << GENERAL_PADS_CONFIG__DS_SHIFT))
#define GENERAL_PADS_CONFIG__OFFSET (4)

#endif /* _PINCTRL_HAILO15_H */
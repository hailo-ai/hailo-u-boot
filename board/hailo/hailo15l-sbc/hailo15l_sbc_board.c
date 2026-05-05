// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 *
 * H15L SBC board-specific initialization.
 * Sensor detection is performed in board_late_init() using DM I2C / GPIO.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <i2c.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <linux/sizes.h>

/* IMX662 sensor on H15L SBC (CSI0, i2c bus 0, addr 0x10) */
#define IMX662_SENSOR_RESET_GPIO  "cmsdk-gpio-0-4"
#define IMX662_GPIO_SETTLE_MS     10          /* ms after GPIO release */
#define IMX662_I2C_BUS            0
#define IMX662_I2C_ADDR           0x10
#define IMX662_CHIPID_REG         0x3CB6      /* 16-bit register address */
#define IMX662_CHIPID_VAL         0xF3        /* expected chip-ID byte */
#define IMX662_OVERLAY_CONFIG     "#conf-hailo_hailo15l-sbc-sensor0-imx662.dtbo"

static bool detect_imx662(void)
{
	struct gpio_desc reset_gpio;
	struct udevice *bus, *dev;
	u8 chip_id;
	bool detected = false;
	int ret;

	/* Release sensor from reset (GPIO0[4] HIGH = out of reset) */
	ret = dm_gpio_lookup_name(IMX662_SENSOR_RESET_GPIO, &reset_gpio);
	if (ret) {
		debug("IMX662: GPIO lookup failed: %d\n", ret);
		return false;
	}

	ret = dm_gpio_request(&reset_gpio, "sensor-reset");
	if (ret) {
		debug("IMX662: GPIO request failed: %d\n", ret);
		return false;
	}

	ret = dm_gpio_set_dir_flags(&reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		debug("IMX662: GPIO set dir failed: %d\n", ret);
		goto release_gpio;
	}

	dm_gpio_set_value(&reset_gpio, 1);
	mdelay(IMX662_GPIO_SETTLE_MS);

	ret = uclass_get_device_by_seq(UCLASS_I2C, IMX662_I2C_BUS, &bus);
	if (ret)
		goto deassert_gpio;

	ret = dm_i2c_probe(bus, IMX662_I2C_ADDR, 0, &dev);
	if (ret)
		goto deassert_gpio;

	/* IMX662 uses 16-bit register addresses */
	i2c_set_chip_offset_len(dev, 2);

	ret = dm_i2c_read(dev, IMX662_CHIPID_REG, &chip_id, 1);
	if (ret)
		goto deassert_gpio;

	detected = (chip_id == IMX662_CHIPID_VAL);

deassert_gpio:
	dm_gpio_set_value(&reset_gpio, 0);
release_gpio:
	dm_gpio_free(NULL, &reset_gpio);
	return detected;
}

int board_late_init(void)
{
	const char *cur;
	char buf[256];

	/* Select DTB variant based on DRAM size (board_dtb defaults to board) */
	if (gd->ram_size <= SZ_1G)
		env_set("board_dtb", "hailo15l-sbc-1gb");

	if (!detect_imx662())
		return 0;

	cur = env_get("dtb_overlays");
	snprintf(buf, sizeof(buf), "%s%s", IMX662_OVERLAY_CONFIG,
		 cur ? cur : "");

	env_set("dtb_overlays", buf);
	printf("Sensor: IMX662 detected, loading DT overlay\n");
	return 0;
}

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
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/sizes.h>

/* Pinmux + pad-config for i2c_2 on H15L SBC.
 *
 * Bus 2's SCL/SDA come out of general-purpose pads, not dedicated i2c
 * pads (unlike bus 0). Making them act as i2c needs two register writes:
 *
 *   1) Switch the pad from GPIO to i2c mode (function-select).
 *   2) Crank the pad's output drive strong enough to pull the bus low
 *      through its loading (drive-strength).
 *
 * GPIO pads reset with drive-strength 0 — too weak to pull the bus low,
 * so no slave ACKs. Linux's kernel pinctrl driver writes both registers
 * from the DT `pinctrl_gpio_16` / `pinctrl_gpio_17` nodes
 * (`drive-strength = <8>`); the same effect is reproduced here.
 *
 *   IO_PAD_76 = GPIO_16 = i2c2_scl  (mode 1 → group i2c2_scl_4)
 *   IO_PAD_77 = GPIO_17 = i2c2_sda  (mode 1 → group i2c2_sda_3)
 *
 *   Function-select:
 *     general_pads_config @ 0x7c291000 + 0xA4 + mux_index*4
 *   Drive-strength (4 bits per pad, one bit per word at the same bit
 *   position; strength 8 = 0b1000 → bit set in word 3 only):
 *     gpio_pads_config @ 0x7c292000 + 0xC + i*4   (i in 0..3)
 */
#define HAILO15L_GENERAL_PADS_CONFIG_BASE  0x7c291000
#define HAILO15L_GPIO_PADS_CONFIG_BASE     0x7c292000
#define HAILO15L_PADS_PINMUX_OFFSET        0xA4
#define HAILO15L_GPIO_PADS_DS_OFFSET       0xC
#define HAILO15L_GPIO_PADS_DS_NUM_WORDS    4
#define IO_PAD_76_I2C2_SCL                 76
#define IO_PAD_77_I2C2_SDA                 77
#define GPIO_PAD_INDEX_PIN_76              16
#define GPIO_PAD_INDEX_PIN_77              17
#define I2C2_PINMUX_MODE                   1
#define I2C2_DRIVE_STRENGTH                8       /* matches Linux DT */

static void hailo15l_sbc_set_gpio_pad_drive_strength(unsigned pad_index, unsigned strength)
{
	void __iomem *ds_base = (void __iomem *)(uintptr_t)
		(HAILO15L_GPIO_PADS_CONFIG_BASE + HAILO15L_GPIO_PADS_DS_OFFSET);
	int i;

	/* Drive-strength is 4 bits, one bit per word at the same pad_index */
	for (i = 0; i < HAILO15L_GPIO_PADS_DS_NUM_WORDS; i++) {
		void __iomem *reg = ds_base + i * sizeof(uint32_t);
		u32 v = readl(reg);
		v &= ~BIT(pad_index);
		v |= (((strength >> i) & 1) << pad_index);
		writel(v, reg);
	}
}

static void hailo15l_sbc_mux_i2c2_pins(void)
{
	void __iomem *mux_base = (void __iomem *)(uintptr_t)
		(HAILO15L_GENERAL_PADS_CONFIG_BASE + HAILO15L_PADS_PINMUX_OFFSET);

	/* Function-select: switch pads from GPIO to i2c mode. */
	writel(I2C2_PINMUX_MODE, mux_base + IO_PAD_76_I2C2_SCL * sizeof(uint32_t));
	writel(I2C2_PINMUX_MODE, mux_base + IO_PAD_77_I2C2_SDA * sizeof(uint32_t));

	/* Drive-strength: bring the pads up to a level the bus can actually use. */
	hailo15l_sbc_set_gpio_pad_drive_strength(GPIO_PAD_INDEX_PIN_76, I2C2_DRIVE_STRENGTH);
	hailo15l_sbc_set_gpio_pad_drive_strength(GPIO_PAD_INDEX_PIN_77, I2C2_DRIVE_STRENGTH);
}

/* IMX662 sensor 0 on H15L SBC: CSI0, i2c bus 0, addr 0x10, reset on GPIO0[4] */
#define IMX662_S0_RESET_GPIO      "cmsdk-gpio-0-4"
#define IMX662_S0_I2C_BUS         0

/* IMX662 sensor 1 on H15L SBC: CSI1, i2c bus 2, addr 0x10, reset on GPIO0[8] */
#define IMX662_S1_RESET_GPIO      "cmsdk-gpio-0-8"
#define IMX662_S1_I2C_BUS         2

#define IMX662_GPIO_SETTLE_MS     10
#define IMX662_I2C_ADDR           0x10
#define IMX662_CHIPID_REG         0x3CB6      /* 16-bit register address */
#define IMX662_CHIPID_VAL         0xF3        /* expected chip-ID byte */

#define IMX662_OVERLAY_S0         "#conf-hailo_hailo15l-sbc-sensor0-imx662.dtbo"
#define IMX662_OVERLAY_S1         "#conf-hailo_hailo15l-sbc-sensor1-imx662.dtbo"
#define IMX662_OVERLAY_DUAL       "#conf-hailo_hailo15l-sbc-dual-imx662.dtbo"

static bool detect_imx662(int i2c_bus, const char *reset_gpio_name)
{
	struct gpio_desc reset_gpio;
	struct udevice *bus, *dev;
	u8 chip_id;
	bool detected = false;
	int ret;

	ret = dm_gpio_lookup_name(reset_gpio_name, &reset_gpio);
	if (ret) {
		printf("IMX662: GPIO %s lookup failed: %d\n", reset_gpio_name, ret);
		return false;
	}

	ret = dm_gpio_request(&reset_gpio, "sensor-reset");
	if (ret) {
		printf("IMX662: GPIO %s request failed: %d\n", reset_gpio_name, ret);
		return false;
	}

	ret = dm_gpio_set_dir_flags(&reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("IMX662: GPIO %s set dir failed: %d\n", reset_gpio_name, ret);
		goto release_gpio;
	}

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("IMX662: i2c bus %d not present (%d)\n", i2c_bus, ret);
		goto release_gpio;
	}

	/* IMX662 power-on per datasheet: XCLR low >=500ns, then high, then >=20us
	 * before first i2c access. mdelay margins are generous; the sensor only
	 * needs a clean rising edge while INCK (a fixed-clock) is already running.
	 */
	dm_gpio_set_value(&reset_gpio, 0);
	mdelay(2);
	dm_gpio_set_value(&reset_gpio, 1);
	mdelay(20);

	ret = dm_i2c_probe(bus, IMX662_I2C_ADDR, 0, &dev);
	if (ret) {
		debug("IMX662: probe on bus %d @ 0x%02x failed (%d)\n",
		      i2c_bus, IMX662_I2C_ADDR, ret);
		goto deassert_gpio;
	}

	/* IMX662 uses 16-bit register addresses */
	i2c_set_chip_offset_len(dev, 2);

	ret = dm_i2c_read(dev, IMX662_CHIPID_REG, &chip_id, 1);
	if (ret) {
		debug("IMX662: chip-id read on bus %d failed (%d)\n",
		      i2c_bus, ret);
		goto deassert_gpio;
	}

	detected = (chip_id == IMX662_CHIPID_VAL);

deassert_gpio:
	dm_gpio_set_value(&reset_gpio, 0);
release_gpio:
	dm_gpio_free(NULL, &reset_gpio);
	return detected;
}

int board_late_init(void)
{
	bool s0_present, s1_present;
	const char *overlay;
	const char *cur;
	char buf[256];

	/* Select DTB variant based on DRAM size (board_dtb defaults to board) */
	if (gd->ram_size <= SZ_1G)
		env_set("board_dtb", "hailo15l-sbc-1gb");

	/* Detected sensor overlay is prepended to dtb_overlays so any
	 * non-sensor overlays already set by the user (e.g. a gyro) are
	 * preserved as the suffix.
	 */
	cur = env_get("dtb_overlays");

	s0_present = detect_imx662(IMX662_S0_I2C_BUS, IMX662_S0_RESET_GPIO);
	hailo15l_sbc_mux_i2c2_pins();
	s1_present = detect_imx662(IMX662_S1_I2C_BUS, IMX662_S1_RESET_GPIO);

	if (s0_present && s1_present) {
		overlay = IMX662_OVERLAY_DUAL;
		printf("Sensor: dual IMX662, loading DT overlay\n");
	} else if (s0_present) {
		overlay = IMX662_OVERLAY_S0;
		printf("Sensor: IMX662 detected on CSI0, loading DT overlay\n");
	} else if (s1_present) {
		overlay = IMX662_OVERLAY_S1;
		printf("Sensor: IMX662 detected on CSI1, loading DT overlay\n");
	} else {
		printf("Sensor: no IMX662 detected on either CSI bus; "
		       "booting without sensor DT overlay\n");
		return 0;
	}

	snprintf(buf, sizeof(buf), "%s%s", overlay, cur ? cur : "");
	env_set("dtb_overlays", buf);
	return 0;
}

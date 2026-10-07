// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * The camera ports of every Hailo15/15L board that detects its sensor at
 * boot, and the board_late_init() that drives detection on them. Selected by
 * CONFIG_HAILO_SENSOR_DETECT; the detection logic itself is in
 * hailo15_sensor_detect.c.
 *
 * One port table for all of them. The boards differ in three things: which
 * GPIO0 pin straps each sensor's reset, whether a second port exists, and --
 * for the H15L SBC alone -- how its second bus's pads are muxed and the fact
 * that its single D-PHY is split 2+2 across the bridges.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <init.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/sizes.h>

#include "hailo15_board.h"
#include "hailo15_sensor_detect.h"

/*
 * For the PADS_PINMUX and pad-bypass register offsets, so they are not
 * duplicated here. The pinctrl driver owns them; the bases below are not in
 * that header because the driver takes them from its DT reg.
 */
#include "../../../drivers/pinctrl/hailo/pinctrl-hailo15.h"

/*
 * The H15L SBC has two U-Boot targets: the plain one and the NAND-boot one,
 * whose DT is hailo15l-sbc.dts plus a flash node. Same board, so all the
 * camera wiring below is the same; only where it boots from differs.
 */
#if defined(CONFIG_TARGET_HAILO15L_SBC) || defined(CONFIG_TARGET_HAILO15L_SBC_NAND)
#define HAILO15L_SBC_BOARD
#endif

/*
 * Reset straps, and which boards have a second camera port. Every board that
 * can enable detection straps port 0's reset to GPIO0[4]; they differ in
 * whether there is a second port and which pin resets it.
 */
#if defined(CONFIG_TARGET_HAILO15_SBC)
#define SENSOR0_RESET_GPIO	"cmsdk-gpio-0-4"
#define DETECT_SENSOR1
#define SENSOR1_RESET_GPIO	"cmsdk-gpio-0-5"
#elif defined(HAILO15L_SBC_BOARD)
#define SENSOR0_RESET_GPIO	"cmsdk-gpio-0-4"
#define DETECT_SENSOR1
#define SENSOR1_RESET_GPIO	"cmsdk-gpio-0-8"
#elif defined(CONFIG_TARGET_HAILO15_SBC_REV3) || \
      defined(CONFIG_TARGET_HAILO15_SBC_REV3_1)
/*
 * Port 0 only, going by the kernel device trees: rev3 declares no second
 * camera node, and rev3-1 declares one on i2c_1 while the port-1 entry below
 * probes bus 2. Neither has been tried on hardware, so port 1 is left exactly
 * as the board's own DT has it -- as on develop -- rather than detected on an
 * assumption.
 */
#define SENSOR0_RESET_GPIO	"cmsdk-gpio-0-4"
#else
/*
 * Deliberately no default. A board whose ports are not described here would
 * otherwise inherit some other board's reset pins and bus numbers and detect
 * against them, which fails quietly -- a camera missed, or one brought up on
 * wiring it does not have. Describing the board is the cheaper half of
 * enabling this, so it is required rather than guessed.
 */
#error "CONFIG_HAILO_SENSOR_DETECT: this board's camera ports are not \
described in hailo15_sensor_boards.c - add a case to the #if above, saying \
which GPIO0 pin resets each port and whether port 1 exists on i2c_2"
#endif

#ifdef DETECT_SENSOR1

/*
 * The pad-config bases, from the pinctrl node's two named regs -- the same
 * ones pinctrl-hailo15.c takes with dev_read_resource_byname(). The driver
 * keeps them in its private struct hailo_priv, so read the node directly.
 */
static int hailo15_pad_config_bases(void __iomem **general, void __iomem **gpio)
{
	struct resource res;
	ofnode node;

	node = ofnode_by_compatible(ofnode_null(), "hailo15,pinctrl");
	if (!ofnode_valid(node))
		return -ENODEV;
	if (ofnode_read_resource_byname(node, "general_pads_config_base", &res))
		return -EINVAL;
	*general = (void __iomem *)(uintptr_t)res.start;
	if (ofnode_read_resource_byname(node, "gpio_pads_config_base", &res))
		return -EINVAL;
	*gpio = (void __iomem *)(uintptr_t)res.start;

	return 0;
}

#if defined(HAILO15L_SBC_BOARD)

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
 *     general_pads_config + 0xA4 + mux_index*4
 *   Drive-strength (4 bits per pad, one bit per word at the same bit
 *   position; strength 8 = 0b1000 → bit set in word 3 only):
 *     gpio_pads_config + 0xC + i*4   (i in 0..3)
 *
 * These offsets are H15L's own; pinctrl-hailo15.h describes H15's different
 * layout (PADS_PINMUX at 0xb8, drive-strength as a DS_SHIFT bitfield).
 */
#define HAILO15L_PADS_PINMUX_OFFSET        0xA4
#define HAILO15L_GPIO_PADS_DS_OFFSET       0xC
#define HAILO15L_GPIO_PADS_DS_NUM_WORDS    4
#define IO_PAD_76_I2C2_SCL                 76
#define IO_PAD_77_I2C2_SDA                 77
#define GPIO_PAD_INDEX_PIN_76              16
#define GPIO_PAD_INDEX_PIN_77              17
#define I2C2_PINMUX_MODE                   1
#define I2C2_DRIVE_STRENGTH                8       /* matches Linux DT */

static void hailo15l_sbc_set_gpio_pad_drive_strength(void __iomem *gpio_base,
						     unsigned pad_index,
						     unsigned strength)
{
	void __iomem *ds_base = gpio_base + HAILO15L_GPIO_PADS_DS_OFFSET;
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

static unsigned hailo15l_sbc_get_gpio_pad_drive_strength(void __iomem *gpio_base,
							 unsigned pad_index)
{
	void __iomem *ds_base = gpio_base + HAILO15L_GPIO_PADS_DS_OFFSET;
	unsigned strength = 0;
	int i;

	for (i = 0; i < HAILO15L_GPIO_PADS_DS_NUM_WORDS; i++) {
		u32 v = readl(ds_base + i * sizeof(uint32_t));

		strength |= ((v >> pad_index) & 1) << i;
	}

	return strength;
}

/*
 * What hailo_camera_bus_prepare() found, so an empty port can be handed
 * back unchanged. The function-select words are per-pad, so they are saved
 * whole; drive-strength is one bit per pad spread across four words, so it is
 * saved as the assembled 4-bit value.
 */
static struct {
	bool valid;
	u32 mux_scl;
	u32 mux_sda;
	unsigned ds_scl;
	unsigned ds_sda;
} i2c2_pad_state;

static void hailo_camera_bus_prepare(void)
{
	void __iomem *general_base, *gpio_base, *mux_base;
	int ret;

	ret = hailo15_pad_config_bases(&general_base, &gpio_base);
	if (ret) {
		printf("sensor: no pinctrl node for the pad bases (%d)\n", ret);
		return;
	}
	mux_base = general_base + HAILO15L_PADS_PINMUX_OFFSET;

	i2c2_pad_state.mux_scl =
		readl(mux_base + IO_PAD_76_I2C2_SCL * sizeof(uint32_t));
	i2c2_pad_state.mux_sda =
		readl(mux_base + IO_PAD_77_I2C2_SDA * sizeof(uint32_t));
	i2c2_pad_state.ds_scl =
		hailo15l_sbc_get_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_76);
	i2c2_pad_state.ds_sda =
		hailo15l_sbc_get_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_77);
	i2c2_pad_state.valid = true;

	/* Function-select: switch pads from GPIO to i2c mode. */
	writel(I2C2_PINMUX_MODE, mux_base + IO_PAD_76_I2C2_SCL * sizeof(uint32_t));
	writel(I2C2_PINMUX_MODE, mux_base + IO_PAD_77_I2C2_SDA * sizeof(uint32_t));

	/* Drive-strength: bring the pads up to a level the bus can actually use. */
	hailo15l_sbc_set_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_76,
						 I2C2_DRIVE_STRENGTH);
	hailo15l_sbc_set_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_77,
						 I2C2_DRIVE_STRENGTH);
}

static void hailo_camera_bus_restore(void)
{
	void __iomem *general_base, *gpio_base, *mux_base;

	if (!i2c2_pad_state.valid)
		return;

	/* prepare() resolved these already, so this cannot fail here. */
	if (hailo15_pad_config_bases(&general_base, &gpio_base))
		return;
	mux_base = general_base + HAILO15L_PADS_PINMUX_OFFSET;

	/* Drive-strength first, so the pads weaken before they change role. */
	hailo15l_sbc_set_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_76,
						 i2c2_pad_state.ds_scl);
	hailo15l_sbc_set_gpio_pad_drive_strength(gpio_base, GPIO_PAD_INDEX_PIN_77,
						 i2c2_pad_state.ds_sda);

	writel(i2c2_pad_state.mux_scl,
	       mux_base + IO_PAD_76_I2C2_SCL * sizeof(uint32_t));
	writel(i2c2_pad_state.mux_sda,
	       mux_base + IO_PAD_77_I2C2_SDA * sizeof(uint32_t));

	i2c2_pad_state.valid = false;
}

#else

/*
 * i2c_2 needs two things done for it here. U-Boot does have the pinctrl
 * driver (CONFIG_PINCTRL_HAILO15), but nothing in its DT gives i2c_2 a pinmux
 * state to select, so there is no one to ask -- see the note at the end.
 *
 * 1. Mux. One shared field -- PADS_PINMUX bits [22:20] -- selects the
 *    function for the whole pins 16..31 set, and a group is reachable only at
 *    the values it declares: i2c2_1 (pins 22, 23) at 3 or 5,
 *    i2c2_current_src_en_out_3 (pin 24) at 3 only. So Linux settles on 3 and
 *    this writes the same. Unlike H15L these are general pads, so no
 *    drive-strength write is needed.
 *
 * 2. Un-bypass. SCU-FW leaves every pad's output- and input-enable under
 *    bypass, which keeps the muxed function from driving or sensing the pin;
 *    Linux clears the pin's bit in both bypass registers when a driver claims
 *    it. Without this the mux is set, the bus enumerates, and nothing ever
 *    ACKs -- measured on an H15 SBC, where clearing these three bits made
 *    0x1a answer immediately.
 *
 * i2c_0 needs neither: its SCL/SDA are dedicated i2c pads.
 */
#define PINS_31_16_MODE_SHIFT		20
#define PINS_31_16_MODE_MASK		0x7
#define PINS_31_16_MODE_I2C2		3
#define I2C2_PADS			(BIT(22) | BIT(23) | BIT(24))

/*
 * What hailo_camera_bus_prepare() found, so an empty port can be handed back
 * unchanged. Only the fields actually written are saved: the mux is a shared
 * 3-bit field and the bypasses are shared words, so restoring whole registers
 * would hand back bits this code never touched.
 */
static struct {
	bool valid;
	u32 pinmux_mode;	/* PADS_PINMUX[22:20] */
	u32 input_bypass;	/* I2C2_PADS bits of the input-enable bypass */
	u32 output_bypass;	/* I2C2_PADS bits of the output-enable bypass */
} i2c2_pad_state;

static void __iomem *hailo15_gpio_pads_reg(void __iomem *gpio_base,
					   unsigned int offset)
{
	return gpio_base + offset;
}

static void __iomem *hailo15_pinmux_reg(void __iomem *general_base)
{
	return general_base + GENERAL_PADS_CONFIG__PADS_PINMUX;
}

static void hailo15_write_pad_bypass(void __iomem *gpio_base,
				     unsigned int offset, u32 pads, u32 value)
{
	void __iomem *reg = hailo15_gpio_pads_reg(gpio_base, offset);

	writel((readl(reg) & ~pads) | (value & pads), reg);
}

static void hailo_camera_bus_prepare(void)
{
	void __iomem *general_base, *gpio_base, *reg;
	u32 v;
	int ret;

	ret = hailo15_pad_config_bases(&general_base, &gpio_base);
	if (ret) {
		printf("sensor: no pinctrl node for the pad bases (%d)\n", ret);
		return;
	}
	reg = hailo15_pinmux_reg(general_base);
	v = readl(reg);

	i2c2_pad_state.pinmux_mode =
		(v >> PINS_31_16_MODE_SHIFT) & PINS_31_16_MODE_MASK;
	i2c2_pad_state.input_bypass =
		readl(hailo15_gpio_pads_reg(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN)) &
		I2C2_PADS;
	i2c2_pad_state.output_bypass =
		readl(hailo15_gpio_pads_reg(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN)) &
		I2C2_PADS;
	i2c2_pad_state.valid = true;

	v &= ~(PINS_31_16_MODE_MASK << PINS_31_16_MODE_SHIFT);
	v |= PINS_31_16_MODE_I2C2 << PINS_31_16_MODE_SHIFT;
	writel(v, reg);

	hailo15_write_pad_bypass(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN,
				 I2C2_PADS, 0);
	hailo15_write_pad_bypass(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN,
				 I2C2_PADS, 0);
}

static void hailo_camera_bus_restore(void)
{
	void __iomem *general_base, *gpio_base, *reg;
	u32 v;

	if (!i2c2_pad_state.valid)
		return;

	/* prepare() resolved these already, so this cannot fail here. */
	if (hailo15_pad_config_bases(&general_base, &gpio_base))
		return;
	reg = hailo15_pinmux_reg(general_base);

	/* Bypass first: stop the pads driving before the function changes. */
	hailo15_write_pad_bypass(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN,
				 I2C2_PADS, i2c2_pad_state.output_bypass);
	hailo15_write_pad_bypass(gpio_base, GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN,
				 I2C2_PADS, i2c2_pad_state.input_bypass);

	v = readl(reg);
	v &= ~(PINS_31_16_MODE_MASK << PINS_31_16_MODE_SHIFT);
	v |= i2c2_pad_state.pinmux_mode << PINS_31_16_MODE_SHIFT;
	writel(v, reg);

	i2c2_pad_state.valid = false;
}

#endif /* HAILO15L_SBC_BOARD */
#endif /* DETECT_SENSOR1 */

/*
 * Every port uses the generic overlay families. Those overlays describe only
 * which sensor is fitted and where it answers -- never how many lanes it runs
 * on, which is the board's business and is described by the wiring overlays
 * below.
 */
static const struct hailo_sensor_port hailo_sensor_ports[] = {
	{
		.name = "CSI0",
		.i2c_bus = 0,
		.reset_gpio = SENSOR0_RESET_GPIO,
		.overlay_stem = "hailo15-sensor0",
	},
#ifdef DETECT_SENSOR1
	{
		.name = "CSI1",
		.i2c_bus = 2,
		.reset_gpio = SENSOR1_RESET_GPIO,
		.prepare = hailo_camera_bus_prepare,
		.restore = hailo_camera_bus_restore,
		.overlay_stem = "hailo15-sensor1",
	},
#endif
};

static const struct hailo_sensor_board hailo_sensor_board = {
	.ports = hailo_sensor_ports,
	.num_ports = ARRAY_SIZE(hailo_sensor_ports),
#if defined(HAILO15L_SBC_BOARD)
	/*
	 * Pluto's single D-PHY is split 2+2 across the two bridges, so a
	 * camera here runs on two lanes whether or not a second one is
	 * fitted; the dual-link overlay additionally puts both bridges under
	 * that one D-PHY, which is only right with both fitted.
	 */
	.link_2lane_overlay = "#conf-hailo_hailo15l-sbc-2lane.dtbo",
	.dual_link_overlay = "#conf-hailo_hailo15l-sbc-dual-link.dtbo",
#endif
	/*
	 * Every board here includes hailo15-vision.dtsi, so port 0's node was
	 * the same candidate list before detection existed. One overlay
	 * restores exactly what develop had.
	 */
	.legacy_overlay = "#conf-hailo_hailo15-sensor0-legacy.dtbo",
};

int board_late_init(void)
{
	/*
	 * Not sensor-related: it lives in hailo15_board.c so it still runs when
	 * detection is switched off and this file is not compiled.
	 */
	hailo15_select_board_dtb();

	hailo_sensor_detect_and_select(&hailo_sensor_board);

	return 0;
}

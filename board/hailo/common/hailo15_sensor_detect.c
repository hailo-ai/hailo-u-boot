// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * Boot-time camera sensor detection via the Sony Type-ID registers.
 * See hailo15_sensor_detect.h for the board-facing interface.
 *
 * Per port: release reset, cancel standby, read the Type-ID, re-assert reset.
 *
 * Re-asserting reset before handing over to Linux is not optional: the kernel
 * driver's first act is to drive the reset line high, so if this code leaves
 * it high as well the line never changes, the sensor is never reset, and it
 * starts life with whatever register state this code left behind.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <i2c.h>
#include <vsprintf.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>

#include "hailo15_sensor_detect.h"

#define SENSOR_REG_STANDBY		0x3000	/* write 0 to leave standby */
#define SENSOR_STANDBY_CANCEL		0x00
#define SENSOR_REG_ADDR_BYTES		2	/* 16-bit register addresses */

#define SENSOR_RESET_ASSERT_MS		2	/* XCLR low, needs >= 500ns */
#define SENSOR_RESET_RELEASE_MS		2	/* then >= 20us before i2c */

/*
 * Poll rather than wait a fixed worst case. The time from standby cancel to a
 * readable Type-ID varies by an order of magnitude between parts. At this
 * point we do not know which part we are talking to. Polling costs the minimal delay.
 */
#define SENSOR_TYPEID_POLL_MS		2
#define SENSOR_TYPEID_TIMEOUT_MS	80

#define SENSOR_OVERLAY_ENV		"dtb_overlays"
#define SENSOR_OVERRIDE_ENV		"sensor_override"
#define SENSOR_OVERRIDE_NONE		"none"
#define SENSOR_OVERRIDE_LEGACY		"legacy"

/*
 * ${sensor_link}: the lanes each port is wired with, for boards that
 * describe wiring
 * overlays. "auto" (the default) infers it from what was detected.
 */
#define SENSOR_LINK_ENV			"sensor_link"
#define SENSOR_LINK_AUTO		"auto"
#define SENSOR_LINK_2LANE		"2lane"
#define SENSOR_LINK_4LANE		"4lane"
#define SENSOR_ENV_BUF_SIZE		512

#define SENSOR_ADDR_MAX_CANDIDATES	4
#define SENSOR_MAX_CSI_PORTS		2

/* A pair of registers to read, named for the boot log. */
struct sensor_regs {
	const char *name;
	u16 reg_lo;
	u16 reg_hi;
};

/*
 * The two register windows the Type-ID is known to live in. Both are read on
 * every port: which one a part uses is not something we can know in advance.
 */
static const struct sensor_regs sensor_typeid_regs[] = {
	{ "4D12h", 0x4D12, 0x4D13 },	/* imx662/664/675/678/838 */
	{ "3F12h", 0x3F12, 0x3F13 },	/* imx715 */
};

/*
 * imx334: 0x3015 is shared with imx715 (both read 0x00), so the 0x300A tiebreak is what
 * separates them. imx715 has a Type-ID and is identified long before this runs,
 * but the tiebreak means even a missed Type-ID cannot turn an imx715 into an
 * imx334.
 */
struct sensor_legacy_rule {
	struct sensor_regs regs;
	u8 val_lo;
	u8 val_hi;
	u16 model;
};

/* imx307 uses its own pair of registers rather than the shared 0x3015. */
static const struct sensor_legacy_rule sensor_legacy_rules[] = {
	{ { "3015h/300Ah", 0x3015, 0x300A }, 0x00, 0x0A, 334 },
	{ { "301Eh/301Fh", 0x301E, 0x301F }, 0xB2, 0x01, 307 },
};

/*
 * Sensors the fitImage ships an overlay for, by exact name: the overlay is
 * <stem>-<name>.dtbo, so supporting a new sensor, Sony or otherwise, is
 * this list plus the kernel-side overlay. A Type-ID decode is
 * only trusted if its name is in this list. That also stops the poll loop
 * from accepting a register read taken before the sensor was ready, which
 * can happen to look like valid BCD.
 *
 * ${sensor0_models_extra} extends the list at runtime. It never replaces
 * it, so a saved value cannot mask models a newer bootloader knows. Enabling
 * a new sensor on a already deployed bootloader, once its overlay ships in the
 * fitImage, is:  setenv sensor0_models_extra "imx828"; saveenv
 */
static const char sensor_models_builtin[] =
	"imx307 imx334 imx662 imx664 imx675 imx678 imx715";
#define SENSOR_MODELS_EXTRA_ENV		"sensor0_models_extra"

/*
 * The i2c addresses to probe: the ones camera modules are wired to use.
 * ${sensor0_addrs_extra} (hex, e.g. "36 21") adds to this list. It never
 * replaces it.
 */
static const u8 sensor_addrs_builtin[] = { 0x1a, 0x10 };
#define SENSOR_ADDRS_EXTRA_ENV		"sensor0_addrs_extra"

#define SENSOR_OVERLAY_NAME_LEN		64
#define SENSOR_NAME_LEN			16	/* "imx678", "ov2775", ... */

struct sensor_candidate {
	struct udevice *dev;
	u8 addr;
	/* Last raw bytes seen, per window, reported if nothing ever decodes,
	 * so an unknown sensor is diagnosable from a single boot log.
	 */
	u8 raw_lo[ARRAY_SIZE(sensor_typeid_regs)];
	u8 raw_hi[ARRAY_SIZE(sensor_typeid_regs)];
	bool raw_valid[ARRAY_SIZE(sensor_typeid_regs)];
};

/* Is @name a space-separated token of @list? NULL @list is empty. */
static bool sensor_list_has(const char *list, const char *name)
{
	size_t len = strlen(name);

	while (list && *list) {
		if (*list == ' ') {
			list++;
			continue;
		}

		if (!strncmp(list, name, len) &&
		    (list[len] == ' ' || list[len] == '\0'))
			return true;

		while (*list && *list != ' ')
			list++;
	}

	return false;
}

static bool sensor_name_supported(const char *name)
{
	return sensor_list_has(sensor_models_builtin, name) ||
	       sensor_list_has(env_get(SENSOR_MODELS_EXTRA_ENV), name);
}

/*
 * The overlay names, constructed from the port's stem. NULL if the sensor
 * name is not one we support, or if the composed name would not fit @buf; the
 * stem itself is guaranteed by the check in hailo_sensor_detect_and_select().
 */
static const char *sensor_name_overlay(const struct hailo_sensor_port *port,
				       const char *name, char *buf, size_t len)
{
	int n;

	if (!sensor_name_supported(name))
		return NULL;

	n = snprintf(buf, len, "#conf-hailo_%s-%s.dtbo",
		     port->overlay_stem, name);
	if (n < 0 || (size_t)n >= len)
		return NULL;

	return buf;
}

/*
 * Nothing validates @addr: need to ship a matching -addrXX.dtbo for any addr
 * added. NULL if the name would not fit @buf -- a truncated config name would
 * reach bootm and fail there instead.
 */
static const char *sensor_module_overlay(const struct hailo_sensor_port *port,
					 u8 addr, char *buf, size_t len)
{
	int n = snprintf(buf, len, "#conf-hailo_%s-addr%02x.dtbo",
			 port->overlay_stem, addr);

	if (n < 0 || (size_t)n >= len)
		return NULL;

	return buf;
}

/*
 * Decode the Type-ID pair. The model number is three BCD digits spread over two
 * bytes: the low byte carries the hundreds digit in its low nibble and the tens
 * digit in its high nibble, the high byte carries the units digit in its low
 * nibble. imx678 reads 76 08 -> 6, 7, 8.
 *
 * Returns 0 if the bytes cannot be a real model number. The nibble checks are
 * needed: an unknown device on one camera bus answers 0xf0 0xf1 0xf2 0xf3 to
 * the first four registers, and without the checks that gives a wrong model
 * number that still looks right.
 */
static u16 sensor_typeid_decode(u8 lo, u8 hi)
{
	u8 hundreds = lo & 0xF;
	u8 tens = lo >> 4;
	u8 units = hi & 0xF;

	if (hundreds > 9 || tens > 9 || units > 9)
		return 0;

	/* The nibble above the units digit is not part of the model number
	 * and is not validated: a real imx664 reads 66 74 (top nibble 7).
	 */
	return hundreds * 100 + tens * 10 + units;
}

/* Read one identification register pair */
static bool sensor_read_ident(struct udevice *dev,
			      const struct sensor_regs *regs,
			      u8 *lo, u8 *hi)
{
	if (dm_i2c_read(dev, regs->reg_lo, lo, 1))
		return false;

	if (dm_i2c_read(dev, regs->reg_hi, hi, 1))
		return false;

	return true;
}

/*
 * Poll every candidate's every window until one decodes to a supported name;
 * the first that does wins. A sensor's other window reads 00 00 (measured on
 * imx675), which decodes to nothing, so there is no competing decode to
 * arbitrate.
 */
static u16 sensor_poll_typeid(struct sensor_candidate *cands,
			      unsigned int num_cands, u8 *addr_out,
			      bool *no_fallback)
{
	unsigned int elapsed, i, w;
	char name[SENSOR_NAME_LEN];
	u16 unlisted = 0;
	u8 unlisted_addr = 0;
	u16 model;
	u8 lo, hi;

	for (elapsed = 0; ; elapsed += SENSOR_TYPEID_POLL_MS) {
		for (i = 0; i < num_cands; i++) {
			for (w = 0; w < ARRAY_SIZE(sensor_typeid_regs); w++) {
				if (!sensor_read_ident(cands[i].dev,
						       &sensor_typeid_regs[w],
						       &lo, &hi))
					continue;

				cands[i].raw_lo[w] = lo;
				cands[i].raw_hi[w] = hi;
				cands[i].raw_valid[w] = true;

				model = sensor_typeid_decode(lo, hi);
				if (!model)
					continue;

				snprintf(name, sizeof(name), "imx%u", model); /* imx specific */
				if (!sensor_name_supported(name)) {
					unlisted = model;
					unlisted_addr = cands[i].addr;
					continue;
				}

				*addr_out = cands[i].addr;
				return model;
			}
		}

		if (elapsed >= SENSOR_TYPEID_TIMEOUT_MS)
			break;
		mdelay(SENSOR_TYPEID_POLL_MS);
	}

	/*
	 * A part identified itself but this build has no overlay for it. The
	 * fingerprints are only for parts with no Type-ID, so skip them.
	 */
	if (unlisted) {
		printf("sensor: 0x%02x is imx%u, which is not in the supported model list; once its overlay is in the fitImage, enable it with: setenv %s \"imx%u\"; saveenv\n",
		       unlisted_addr, unlisted, SENSOR_MODELS_EXTRA_ENV,
		       unlisted);
		*no_fallback = true;
		return 0;
	}

	/* Nothing decoded. Dump what we did see, so that a module this code
	 * does not know about can be identified from one boot log.
	 */
	for (i = 0; i < num_cands; i++) {
		for (w = 0; w < ARRAY_SIZE(sensor_typeid_regs); w++) {
			if (!cands[i].raw_valid[w])
				continue;

			printf("sensor: 0x%02x %s reads %02x %02x (not a known Type-ID)\n",
			       cands[i].addr, sensor_typeid_regs[w].name,
			       cands[i].raw_lo[w], cands[i].raw_hi[w]);
		}
	}

	return 0;
}

/*
 * Last resort for the parts with no Type-ID. Runs only after the
 * poll timed out, so the sensor is long since awake and a single read of each
 * rule is enough.
 */
static u16 sensor_try_legacy(struct sensor_candidate *cands,
			     unsigned int num_cands, u8 *addr_out)
{
	unsigned int i, r;
	u8 lo, hi;

	for (i = 0; i < num_cands; i++) {
		for (r = 0; r < ARRAY_SIZE(sensor_legacy_rules); r++) {
			const struct sensor_legacy_rule *rule =
				&sensor_legacy_rules[r];

			if (!sensor_read_ident(cands[i].dev, &rule->regs,
					       &lo, &hi))
				continue;

			if (lo != rule->val_lo || hi != rule->val_hi)
				continue;

			*addr_out = cands[i].addr;
			return rule->model;
		}
	}

	return 0;
}

/*
 * Add @addr to the probe set if there is room. Say so when there is not:
 * ${sensor0_addrs_extra} is added last, so its addresses are the ones
 * dropped, and the user would otherwise see them ignored for no clear reason.
 */
static void sensor_add_addr(u8 *addrs, unsigned int *num, unsigned int max,
			    u8 addr)
{
	unsigned int i;

	for (i = 0; i < *num; i++)
		if (addrs[i] == addr)
			return;

	if (*num >= max) {
		printf("sensor: probe set full (%u); 0x%02x not probed\n",
		       max, addr);
		return;
	}

	addrs[(*num)++] = addr;
}

/*
 * Build the list of i2c addresses to probe on @port and return how many.
 * Duplicates are dropped.
 */
static unsigned int sensor_port_addrs(const struct hailo_sensor_port *port,
				      u8 *addrs, unsigned int max)
{
	unsigned int num = 0, i;
	const char *extra;

	for (i = 0; i < ARRAY_SIZE(sensor_addrs_builtin); i++)
		sensor_add_addr(addrs, &num, max, sensor_addrs_builtin[i]);

	for (extra = env_get(SENSOR_ADDRS_EXTRA_ENV); extra && *extra;) {
		unsigned long addr;
		char *end;

		if (*extra == ' ') {
			extra++;
			continue;
		}

		addr = simple_strtoul(extra, &end, 16);
		if (end == extra)
			break;
		if (addr && addr <= 0x7f)
			sensor_add_addr(addrs, &num, max, (u8)addr);
		extra = end;
	}

	return num;
}

/*
 * Bring the port's sensor out of reset and standby and read its Type-ID.
 *
 * Returns the decoded model and the address it answered on, or 0 if the port is
 * empty or holds something unrecognised. Reset is always left asserted.
 *
 * On success the sensor's name ("imx678") is written into @name_out
 * (SENSOR_NAME_LEN bytes).
 *
 * A NULL @name_out skips identification: the port is only probed for which
 * address answers, nothing is written to the device, and the probe succeeds
 * if exactly one address answered.
 */
static bool sensor_detect_port(const struct hailo_sensor_port *port,
			       u8 *addr_out, char *name_out)
{
	struct sensor_candidate cands[SENSOR_ADDR_MAX_CANDIDATES] = { };
	u8 addrs[SENSOR_ADDR_MAX_CANDIDATES];
	unsigned int num_addrs, num_cands = 0;
	struct gpio_desc reset_gpio;
	struct udevice *bus, *dev;
	bool no_fallback = false;
	unsigned int i;
	u16 model = 0;
	int ret;

	num_addrs = sensor_port_addrs(port, addrs, ARRAY_SIZE(addrs));
	if (!num_addrs)
		return 0;

	if (port->prepare)
		port->prepare();

	ret = dm_gpio_lookup_name(port->reset_gpio, &reset_gpio);
	if (ret) {
		printf("sensor: %s: GPIO %s lookup failed (%d)\n",
		       port->name, port->reset_gpio, ret);
		goto restore_pads;
	}

	ret = dm_gpio_request(&reset_gpio, "sensor-reset");
	if (ret) {
		printf("sensor: %s: GPIO %s request failed (%d)\n",
		       port->name, port->reset_gpio, ret);
		goto restore_pads;
	}

	ret = dm_gpio_set_dir_flags(&reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("sensor: %s: GPIO %s direction failed (%d)\n",
		       port->name, port->reset_gpio, ret);
		goto free_gpio;
	}

	ret = uclass_get_device_by_seq(UCLASS_I2C, port->i2c_bus, &bus);
	if (ret) {
		printf("sensor: %s: i2c bus %d not present (%d)\n",
		       port->name, port->i2c_bus, ret);
		goto free_gpio;
	}

	/* Pull XCLR low, hold it there, then release it and let it settle,
	 * with INCK -- a fixed clock -- already running.
	 */
	dm_gpio_set_value(&reset_gpio, 0);
	mdelay(SENSOR_RESET_ASSERT_MS);
	dm_gpio_set_value(&reset_gpio, 1);
	mdelay(SENSOR_RESET_RELEASE_MS);

	/*
	 * Probe only the port's address set. Whatever answers is then taken
	 * out of standby, which means writing to a device before knowing what
	 * it is. That is the reason ${sensor_override}=none exists: it
	 * suppresses all of this, so a board carrying a customer's own device
	 * on the camera bus can opt out.
	 */
	for (i = 0; i < num_addrs && num_cands < ARRAY_SIZE(cands); i++) {
		u8 addr = addrs[i];

		if (dm_i2c_probe(bus, addr, 0, &dev))
			continue;

		if (!name_out) {
			cands[num_cands++].addr = addr;
			continue;
		}

		ret = i2c_set_chip_offset_len(dev, SENSOR_REG_ADDR_BYTES);
		if (ret) {
			printf("sensor: %s: 0x%02x offset length failed (%d)\n",
			       port->name, addr, ret);
			continue;
		}

		/* Before standby is cancelled the Type-ID reads back as zero
		 * rather than failing, so this write is what makes the
		 * registers meaningful at all.
		 */
		ret = dm_i2c_reg_write(dev, SENSOR_REG_STANDBY,
				       SENSOR_STANDBY_CANCEL);
		if (ret) {
			printf("sensor: %s: 0x%02x standby cancel failed (%d)\n",
			       port->name, addr, ret);
			continue;
		}

		cands[num_cands].dev = dev;
		cands[num_cands].addr = addr;
		num_cands++;
	}

	if (!name_out) {
		if (num_cands == 1) {
			*addr_out = cands[0].addr;
			model = 1;	/* exactly one address answered */
		} else if (num_cands) {
			printf("sensor: %s: %u addresses answer; cannot tell which carries the forced sensor\n",
			       port->name, num_cands);
		}
	} else if (num_cands) {
		model = sensor_poll_typeid(cands, num_cands, addr_out,
					   &no_fallback);
		if (!model && !no_fallback)
			model = sensor_try_legacy(cands, num_cands, addr_out);
		if (model)
			snprintf(name_out, SENSOR_NAME_LEN, "imx%u", model);
	}

free_gpio:
	/* Always leave the sensor in reset. The kernel driver starts by
	 * driving this line high, and that only resets the part if we left it
	 * low. The returns above happen before reset is released, so they
	 * leave it alone.
	 */
	dm_gpio_set_value(&reset_gpio, 0);
	dm_gpio_free(NULL, &reset_gpio);

restore_pads:
	/*
	 * Nothing was found, so put back whatever ->prepare() changed. On a
	 * port that did answer the pads stay as they are: the kernel is about
	 * to claim them for the same bus anyway.
	 */
	if (!model && port->restore)
		port->restore();

	return model != 0;
}

/*
 * Overlays describing @model at @addr on @port, appended to @out.
 *
 * That is the module overlay (addr) plus the sensor overlay -- always both.
 *
 * Returns the number appended, or 0 if this board cannot describe the pair, in
 * which case nothing is appended and the port is reported as unsupported
 * rather than half-configured.
 */
static unsigned int sensor_collect_overlays(const struct hailo_sensor_port *port,
					    const char *name, u8 addr,
					    const char **out, unsigned int room,
					    char bufs[2][SENSOR_OVERLAY_NAME_LEN])
{
	const char *module, *sensor;

	module = sensor_module_overlay(port, addr, bufs[0],
				       SENSOR_OVERLAY_NAME_LEN);
	sensor = sensor_name_overlay(port, name, bufs[1],
				     SENSOR_OVERLAY_NAME_LEN);

	/*
	 * Either can fail: the sensor name if the Type-ID decoded to a model
	 * we ship no overlay for (the address is always one we chose to probe,
	 * so it needs no such list check), and either if the composed name
	 * would not fit its buffer.
	 *
	 * Neither list is checked against the fitImage; adding to either means
	 * shipping the matching overlay too.
	 *
	 * Both or neither: applying one axis without the other would leave the
	 * node half-described, which fails far more confusingly than simply
	 * having no camera.
	 */
	if (!module || !sensor || room < 2)
		return 0;

	out[0] = module;
	out[1] = sensor;
	return 2;
}

/* Length of the board's own overlay token at @p, or 0 if there is not one. */
static size_t sensor_token_match(const char *p, const char *overlay)
{
	size_t len = strlen(overlay);

	return strncmp(p, overlay, len) ? 0 : len;
}

/*
 * Length of the token at @p if it is one this code applied on some earlier
 * boot, 0 otherwise. Recognising it is what drops it: sensor_apply_overlays()
 * steps over anything this reports and rebuilds ${dtb_overlays} without it,
 * copying through everything it does not recognise.
 */
static size_t sensor_own_overlay_at(const struct hailo_sensor_board *board,
				    const char *p)
{
	unsigned int i;
	size_t len;

	for (i = 0; i < board->num_ports; i++) {
		const struct hailo_sensor_port *port = &board->ports[i];
		char prefix[SENSOR_OVERLAY_NAME_LEN];

		/* Anything constructed from the port's stem is ours --
		 * addrXX and imxNNN alike.
		 */
		snprintf(prefix, sizeof(prefix), "#conf-hailo_%s-",
			 port->overlay_stem);
		len = sensor_token_match(p, prefix);
		if (len) {
			while (p[len] && p[len] != '#')
				len++;
			return len;
		}
	}

	/* The wiring overlays are ours too. They have to be recognised here or
	 * they would be mistaken for someone else's overlay and copied
	 * through, so a board that once booted dual would keep a 2-lane
	 * overlay for good.
	 */
	if (board->link_2lane_overlay) {
		len = sensor_token_match(p, board->link_2lane_overlay);
		if (len)
			return len;
	}

	if (board->dual_link_overlay) {
		len = sensor_token_match(p, board->dual_link_overlay);
		if (len)
			return len;
	}

	/* Named, so it need not share a stem with anything above. */
	if (board->legacy_overlay) {
		len = sensor_token_match(p, board->legacy_overlay);
		if (len)
			return len;
	}

	return 0;
}

/*
 * Point ${dtb_overlays} at @overlays, keeping any overlay in there that is not
 * one of ours.
 *
 * ${dtb_overlays} is a run of "#conf-....dtbo" tokens with no separator. This
 * code only sets it, never saves it: detection runs every boot, and its answer
 * should not outlive the hardware it describes.
 *
 * The variable is persistent though, and someone else may have saved a sensor
 * overlay into it -- "fw_setenv dtb_overlays ..." was the old way to enable a
 * sensor by hand, and an environment can also come from another board. Kept
 * tokens go first and the detected overlays last: overlays are applied left to
 * right and the later one wins, so fresh detection beats anything stale.
 * Pinning a sensor by hand is what ${sensor_override} is for.
 */
static void sensor_apply_overlays(const struct hailo_sensor_board *board,
				  const char *const *overlays,
				  unsigned int count)
{
	char buf[SENSOR_ENV_BUF_SIZE];
	const char *cur;
	size_t used = 0;
	unsigned int i;
	size_t skip;

	cur = env_get(SENSOR_OVERLAY_ENV);
	while (cur && *cur) {
		size_t len;

		/* Keep whole "#conf-*" tokens and nothing else. The factory
		 * default is a single space, and any space carried into
		 * ${dtb_overlays} splits the bootm argument in two: the
		 * overlays land in the initrd slot and the base DTB boots
		 * with no diagnostic.
		 */
		if (*cur != '#') {
			cur++;
			continue;
		}

		skip = sensor_own_overlay_at(board, cur);
		if (skip) {
			cur += skip;
			continue;
		}

		len = 1 + strcspn(cur + 1, "#");
		if (used + len + 1 > sizeof(buf)) {
			printf("sensor: overlay list too long; booting without a sensor overlay\n");
			return;
		}

		memcpy(buf + used, cur, len);
		used += len;
		cur += len;
	}

	for (i = 0; i < count; i++) {
		size_t len = strlen(overlays[i]);

		if (used + len + 1 > sizeof(buf)) {
			printf("sensor: overlay list too long; booting without a sensor overlay\n");
			return;
		}

		memcpy(buf + used, overlays[i], len);
		used += len;
	}

	buf[used] = '\0';

	if (env_set(SENSOR_OVERLAY_ENV, buf))
		printf("sensor: failed to set %s\n", SENSOR_OVERLAY_ENV);
}

/*
 * Copy the name @override forces on port @index into @out. The value is a
 * comma-separated list, one entry per port in order, so a plain "imx678"
 * names port 0 only. Returns false if this port has no name in the list.
 */
static bool sensor_override_name(const char *override, unsigned int index,
				 char *out, size_t len)
{
	const char *entry = override;
	unsigned int i;
	size_t n;

	for (i = 0; i < index; i++) {
		entry = strchr(entry, ',');
		if (!entry)
			return false;
		entry++;
	}

	n = strcspn(entry, ",");
	if (!n || n >= len)
		return false;

	memcpy(out, entry, n);
	out[n] = '\0';
	return true;
}

void hailo_sensor_detect_and_select(const struct hailo_sensor_board *board)
{
	/*
	 * Two overlays per port -- module axis plus sensor axis -- and up to
	 * two board wiring overlays after them.
	 */
	const char *overlays[SENSOR_MAX_CSI_PORTS * 2 + 2];
	char bufs[SENSOR_MAX_CSI_PORTS][2][SENSOR_OVERLAY_NAME_LEN];
	char names[SENSOR_MAX_CSI_PORTS][SENSOR_NAME_LEN] = { };
	u8 addrs[SENSOR_MAX_CSI_PORTS] = { };
	unsigned int num_overlays = 0;
	const char *override_names = NULL;
	const char *override;
	unsigned int i;

	if (!board || !board->num_ports)
		return;

	if (board->num_ports > SENSOR_MAX_CSI_PORTS) {
		printf("sensor: too many camera ports described\n");
		return;
	}

	/*
	 * Every overlay name is built from the port's stem, so a port without
	 * one cannot be described at all. Say so rather than quietly never
	 * probing it.
	 */
	for (i = 0; i < board->num_ports; i++) {
		if (!board->ports[i].overlay_stem) {
			printf("sensor: %s: no overlay stem described\n",
			       board->ports[i].name);
			return;
		}
	}

	override = env_get(SENSOR_OVERRIDE_ENV);
	if (override && *override) {
		/* The escape hatch. "none" must touch nothing at all:
		 * no GPIO, no i2c traffic, on the assumption that whatever is
		 * on that bus is none of our business.
		 */
		if (!strcmp(override, SENSOR_OVERRIDE_NONE)) {
			printf("sensor: detection disabled by %s=%s\n",
			       SENSOR_OVERRIDE_ENV, override);
			return;
		}

		/* The pre-detection candidate-list DT, by explicit request.
		 * No probing: the point is the old behaviour.
		 */
		if (!strcmp(override, SENSOR_OVERRIDE_LEGACY)) {
			if (!board->legacy_overlay) {
				printf("sensor: %s=legacy: no such overlay\n",
				       SENSOR_OVERRIDE_ENV);
				return;
			}

			printf("sensor: legacy candidate-list overlay forced by %s\n",
			       SENSOR_OVERRIDE_ENV);
			sensor_apply_overlays(board, &board->legacy_overlay, 1);
			return;
		}

		/*
		 * Anything else names sensors, verbatim -- "imx678", one day
		 * "ov2775" -- one entry per port. The name dictates the
		 * sensor, not the address: probe which of the board's
		 * addresses the module answers on, so the module overlay
		 * describes the fitted hardware. Nothing is written to the
		 * device. Ports the list does not name are detected as usual.
		 */
		override_names = override;
	}

	for (i = 0; i < board->num_ports; i++) {
		const struct hailo_sensor_port *p = &board->ports[i];
		bool have_addr;

		/* detect */
		if (!override_names ||
		    !sensor_override_name(override_names, i, names[i],
					  SENSOR_NAME_LEN)) {
			if (!sensor_detect_port(p, &addrs[i], names[i]))
				names[i][0] = '\0';
			continue;
		}

		/* forced (sensor name is already in names[i]) */
		have_addr = sensor_detect_port(p, &addrs[i], NULL);

		/* Forcing a name says which sensor, never where: without a
		 * module answering somewhere there is no address to build the
		 * module overlay from, so the port stays unconfigured.
		 */
		if (!have_addr) {
			printf("sensor: %s: no address for \"%s\"\n",
			       p->name, names[i]);
			names[i][0] = '\0';
			continue;
		}

		printf("sensor: %s: %s at 0x%02x forced by %s\n",
		       p->name, names[i], addrs[i], SENSOR_OVERRIDE_ENV);
	}

	for (i = 0; i < board->num_ports; i++) {
		unsigned int n;

		if (!names[i][0]) {
			printf("sensor: %s: none detected\n",
			       board->ports[i].name);
			continue;
		}

		n = sensor_collect_overlays(&board->ports[i], names[i],
					    addrs[i], overlays + num_overlays,
					    ARRAY_SIZE(overlays) - num_overlays,
					    bufs[i]);
		if (!n) {
			printf("sensor: %s: %s at 0x%02x has no overlay on this board\n",
			       board->ports[i].name, names[i], addrs[i]);
			continue;
		}

		printf("sensor: %s: %s at 0x%02x\n", board->ports[i].name,
		       names[i], addrs[i]);
		num_overlays += n;
	}

	if (!num_overlays) {
		/*
		 * Supported outcome, not a failure: boards ship without a
		 * camera and must boot exactly as before.
		 *
		 * Still rewrite ${dtb_overlays}, with nothing to add. Finding
		 * no camera is an answer and has to be recorded: the variable
		 * is persistent, so if anyone ever ran saveenv while a sensor
		 * was fitted, yesterday's overlay pair is still in flash and
		 * returning here would boot it against an empty port -- the
		 * driver would then talk to nothing and fail with -EREMOTEIO.
		 * This strips ours and keeps everyone else's.
		 */
		printf("sensor: booting without a sensor overlay\n");
		sensor_apply_overlays(board, NULL, 0);
		return;
	}

	/*
	 * The board's link wiring, last so it overrides the per-port
	 * properties. See struct hailo_sensor_board for what each describes.
	 */
	if (board->link_2lane_overlay) {
		const char *link = env_get(SENSOR_LINK_ENV); /* NULL if unset */
		unsigned int found = 0;
		bool found_beyond_port0 = false;
		bool split_wiring_active;

		/*
		 * Two separate questions, and neither answers the other. Any
		 * sensor beyond port 0 proves the split is wired -- one on
		 * CSI1 alone is enough. Dual-link mode instead needs a
		 * sensor on both ports. There is one sensor per port, so
		 * found > 1 implies found_beyond_port0, but not the reverse,
		 * and the reverse is the case that matters.
		 */
		for (i = 0; i < board->num_ports; i++) {
			if (!names[i][0])  /* nothing on this port */
				continue;
			found++; /* ports with a sensor */
			if (i)
				found_beyond_port0 = true; /* ...that aren't port 0 */
		}

		/*
		 * Decide if the split wiring is active: one branch per value
		 * ${sensor_link} can take, in the order they are documented.
		 *
		 * The inferred case is the default. Only boards that declare
		 * wiring overlays reach it, and on those a port above 0 is
		 * routed only when the split is wired, so finding a sensor
		 * there proves it. (Boards with independent per-port links
		 * have no wiring overlays and never get here.) A wired board
		 * with only port 0 fitted cannot be told apart, and needs
		 * sensor_link=2lane.
		 */
		if (!link || !*link || !strcmp(link, SENSOR_LINK_AUTO)) {
			/* unset, empty or "auto": infer from what was found */
			split_wiring_active = found_beyond_port0;
		} else if (!strcmp(link, SENSOR_LINK_2LANE)) {
			/* split wiring, even with a single camera fitted */
			split_wiring_active = true;
			printf("sensor: link wiring forced by %s\n",
			       SENSOR_LINK_ENV);
		} else if (!strcmp(link, SENSOR_LINK_4LANE)) {
			/* no split (regular board), even with two cameras fitted */
			split_wiring_active = false;
			printf("sensor: link wiring refused by %s\n",
			       SENSOR_LINK_ENV);
		} else {
			/* a typo, most likely: say so and infer anyway */
			printf("sensor: %s=%s not understood, inferring\n",
			       SENSOR_LINK_ENV, link);
			split_wiring_active = found_beyond_port0;
		}

		if (split_wiring_active) {
			overlays[num_overlays++] = board->link_2lane_overlay;
			printf("sensor: 2-lane link wiring\n");

			/*
			 * Only correct with a sensor on both ports: it puts
			 * both bridges under one D-PHY.
			 */
			/* found > 1 == a sensor on both ports */
			if (board->dual_link_overlay && found > 1) {
				overlays[num_overlays++] =
					board->dual_link_overlay;
				printf("sensor: dual-link\n");
			}
		}
	}

	sensor_apply_overlays(board, overlays, num_overlays);
}

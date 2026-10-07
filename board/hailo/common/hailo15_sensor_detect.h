/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * Boot-time camera sensor detection.
 *
 * Sony image sensors carry their model number in a pair of read-only "Type-ID"
 * registers, in BCD. Reading them tells us exactly which module is fitted, so
 * U-Boot can select the matching per-sensor DT overlay and the kernel is handed
 * a device tree that names one sensor instead of a list of candidates to try.
 *
 * A board opts in by describing its camera ports in a struct
 * hailo_sensor_board and calling hailo_sensor_detect_and_select() from
 * board_late_init().
 */

#ifndef __HAILO15_SENSOR_DETECT_H__
#define __HAILO15_SENSOR_DETECT_H__

#include <linux/types.h>

/*
 * In the common case a camera on a port is described by TWO overlays, applied
 * together and constructed by name from what detection found:
 *
 *   #conf-<vendor>_<stem>-addr<xx>.dtbo  the i2c address it answered on
 *   #conf-<vendor>_<stem>-<name>.dtbo    compatible, INCK, link-frequencies
 *
 * Two is the rule: every camera on every port is described this way. A board
 * whose links need describing as well adds a wiring overlay after them; see
 * struct hailo_sensor_board.
 *
 * The sensor name is an exact, vendor-qualified string -- "imx678" today,
 * "ov2775" someday -- matching the overlay's file name; nothing parses it.
 * Supporting a new sensor needs no U-Boot code: ship the overlay in the
 * fitImage and list the name (built-in list, or ${sensor0_models_extra} on a
 * deployed bootloader). ${sensor0_addrs_extra} likewise extends the probed
 * address set. Both env variables extend and never replace, so a saved value
 * cannot mask what a newer bootloader knows.
 *
 * <stem>-legacy.dtbo is a further constructed name but never an additional
 * applied overlay: it is the pre-detection candidate-list DT, used alone and
 * only on ${sensor_override}=legacy.
 */

/*
 * A board whose wiring differs from the generic case says so with the 
 * wiring overlays in struct hailo_sensor_board. Which overlays a camera gets
 * therefore depends on the board. Taking imx678 as
 * the example -- every supported model behaves the same way:
 *   H15 SBC, CSI0 only    hailo15-sensor0-addr1a + hailo15-sensor0-imx678
 *   H15 SBC, both ports   those two plus the matching hailo15-sensor1-* pair
 *   H15L SBC, both ports  the same four, plus that board's wiring overlays
 *
 * So two imx662 on an H15 SBC get no wiring overlay, while two imx678 on an
 * H15L SBC do. Nothing keys an overlay off a model name.
 */

/*
 * One camera port. Its overlays are constructed from @overlay_stem, which is
 * required -- detection refuses to run on a board that omits it.
 */
struct hailo_sensor_port {
	const char *name;			/* for messages, e.g. "CSI0" */
	int i2c_bus;
	const char *reset_gpio;			/* dm_gpio_lookup_name() name */
	void (*prepare)(void);			/* optional: pinmux the bus */
	/*
	 * Optional counterpart to @prepare, called only when the port turned
	 * up empty. @prepare runs before anything is known about the port, so
	 * on a board where nothing is fitted it would otherwise leave pads
	 * muxed and driving for a bus with no device on it, for the rest of
	 * the boot. Ports whose @prepare has nothing to undo leave this NULL.
	 */
	void (*restore)(void);
	const char *overlay_stem;		/* e.g. "hailo15-sensor0" */
};

struct hailo_sensor_board {
	const struct hailo_sensor_port *ports;
	unsigned int num_ports;
	/*
	 * Board link wiring, applied after the per-port overlays so their
	 * properties win. Both NULL on a board whose ports have independent
	 * links (an rxwrapper each), which needs neither.
	 *
	 * @link_2lane_overlay describes a board whose single D-PHY is split
	 * 2+2 across the bridges -- the H15L SBC (Pluto) dual kit. It is a
	 * property of the WIRING, not of what is fitted: a lone sensor on
	 * such a board still runs on two lanes, so this is applied whenever
	 * the wiring is active, even for one camera.
	 *
	 * @dual_link_overlay enables dual-link mode: one D-PHY driving both
	 * bridges at once (2-clock-lane, plus rxwrapper and pixel-mux
	 * phandles).
	 * Unlike the lane split it is only correct when both ports really
	 * have a sensor, so it is applied only then.
	 *
	 * On a board that declares these -- today only the H15L SBC -- whether
	 * the wiring is active is decided by ${sensor_link}: "2lane" forces it
	 * on, "4lane" off, and unset/"auto" -- the default -- infers it. The
	 * inference holds because on such a board a port above 0 is routed only
	 * when the split is wired, so finding a sensor there proves it. It says
	 * nothing about boards like the H15 SBC, which have a port 1 on an
	 * independent link: they leave both fields NULL and never get here, and
	 * ${sensor_link} does nothing for them.
	 *
	 * The one case that MUST be set by hand: a board that IS wired for the
	 * split but has only port 0 fitted. Nothing on the bus tells it apart
	 * from an unwired board, so "auto" leaves the split off and the camera
	 * is described as 4-lane on 2-lane wiring -- wrong, and silent. It
	 * cannot be inferred; the board has to be told:
	 *     setenv sensor_link 2lane; saveenv
	 * or from Linux:
	 *     fw_setenv sensor_link 2lane
	 */
	const char *link_2lane_overlay;
	const char *dual_link_overlay;
	/*
	 * The overlay ${sensor_override}=legacy applies, named in full. A
	 * board that has none leaves this NULL and the override is refused.
	 *
	 * Named rather than built from a port's overlay_stem: the stem is for
	 * the constructed per-sensor names, and a board that gives its port 0
	 * its own stem would silently produce a legacy name no fitImage ships.
	 */
	const char *legacy_overlay;
};

/*
 * Detect the sensor on each port and point ${dtb_overlays} at the matching
 * overlays. Never fails the boot: a board with no camera, an unrecognised
 * module, or a bus that does not answer all leave the connector disabled and
 * boot on with a message.
 *
 * ${sensor_link} selects how many lanes each port is wired with: "auto" (or
 * unset) infers it, "2lane" forces the split, "4lane" refuses it. It only
 * means anything on a
 * board that describes wiring overlays.
 *
 * ${sensor_override} unset or empty means detect. "none" skips detection and
 * touches no bus or GPIO; "legacy" applies the pre-detection candidate-list
 * overlay, also touching nothing. Anything else names sensors, one entry per
 * port in order: "imx678" names port 0, ",imx662" names port 1, and
 * "imx678,imx662" names both. Ports the list does not name are detected as
 * usual. Addresses are still found by probing and nothing is written to the
 * device.
 */
void hailo_sensor_detect_and_select(const struct hailo_sensor_board *board);

#endif /* __HAILO15_SENSOR_DETECT_H__ */

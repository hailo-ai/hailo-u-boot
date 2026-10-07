// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the Hailo15 pinctrl
 *
 */

#include "pinctrl-hailo15.h"
#include "pinctrl-hailo15-descriptions.h"
#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <log.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/gpio.h>

struct hailo_priv {
    void __iomem *general_pads_config_base;
    void __iomem *gpio_pads_config_base;
    struct h15_pin_set_runtime_data pin_set_runtime_data[H15_PIN_SET_VALUE__COUNT];
    const struct h15_pin_function *current_functions[H15_PINMUX_PIN_COUNT]; 
};

static const unsigned char drive_strength_lookup[16] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

static const struct pinconf_param hailo_conf_params[] = {
    { "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 2 },
    { "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 0 },
    { "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 0 },
    { "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
};

static const struct h15_pin_function unknown_function = {
    .name = "unknown",
    .name_size = sizeof("unknown"),
    .num_groups = 0,
    .groups = NULL,
};

static int hailo15_get_pins_count(struct udevice *dev)
{
    return ARRAY_SIZE(hailo15_pins);
}

/*
* The mode PADS_PINMUX actually holds for a pin set, read back from hardware.
*/
static uint32_t hailo15_get_pads_pinmux_mode(struct hailo_priv *priv,
                                             enum h15_pin_set_value set_number)
{
    uint32_t data_reg = readl(priv->general_pads_config_base +
                              GENERAL_PADS_CONFIG__PADS_PINMUX);

    return (data_reg >> pads_pinmux_mode_shift_value[set_number]) &
           pads_pinmux_mode_reg_mask_value[set_number];
}

static int hailo15_pinctrl_probe(struct udevice *dev)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    int return_value;
    struct resource res_regs;
    int i;

    return_value = dev_read_resource_byname(dev, "general_pads_config_base",
                        &res_regs);
    if (return_value) {
        log_err("Error getting resource\n");
        return -ENODEV;
    }

    priv->general_pads_config_base =
        devm_ioremap(dev, res_regs.start, resource_size(&res_regs));

    return_value = dev_read_resource_byname(dev, "gpio_pads_config_base",
                        &res_regs);
    if (return_value) {
        log_err("Error getting resource\n");
        return -ENODEV;
    }

    priv->gpio_pads_config_base =
        devm_ioremap(dev, res_regs.start, resource_size(&res_regs));

    for (i = 0; i < H15_PIN_SET_VALUE__COUNT; i++) {
        /*
        * Create a contiguous bitmask starting at bit position h15_pin_sets[i].modes_count and ending at position 0.
        */
        priv->pin_set_runtime_data[i].possible_modes = GENMASK(h15_pin_sets[i].modes_count, 0);
        /*
        * Start from what the hardware holds -- the boot ROM or SCU firmware
        * may have left a set in another mode. current_mode is only a record
        * for get_pin_muxing(); set_mux() reads the register itself.
        */
        priv->pin_set_runtime_data[i].current_mode =
            hailo15_get_pads_pinmux_mode(priv, i);
    }

    for (i = 0; i < H15_PINCTRL_PIN_COUNT; i++) {
        priv->current_functions[i] = &unknown_function;
    }

    log_info("hailo15 pinctrl registered\n");
    return 0;
}

static const char *hailo15_get_pin_name(struct udevice *dev,
                    unsigned int selector)
{
    if (selector >= ARRAY_SIZE(hailo15_pins)) {
        return NULL;
    }
    return hailo15_pins[selector].name;
}

static void hailo15_set_pads_pinmux_mode(struct hailo_priv *priv,
                                        enum h15_pin_set_value set_number,
                                        uint32_t pads_pinmux_value)
{
    uint32_t data_reg;
    uint32_t data_reg_mask;

    data_reg = readl(priv->general_pads_config_base +
                     GENERAL_PADS_CONFIG__PADS_PINMUX);
    /*
    * Turn off the bits of the relevant pins set in data_reg.
    */
    data_reg_mask = pads_pinmux_mode_reg_mask_value[set_number];
    data_reg_mask =
        (~(data_reg_mask << pads_pinmux_mode_shift_value[set_number]));
    data_reg = data_reg & data_reg_mask;

    /*
    * Set data_reg in the bits of the relevant pins set to the selected mode.
    */
    data_reg = data_reg | (pads_pinmux_value
               << (pads_pinmux_mode_shift_value[set_number]));
    writel(data_reg, (priv->general_pads_config_base +
           GENERAL_PADS_CONFIG__PADS_PINMUX));

    pr_debug("data_reg: %d\n", data_reg);
}

static int hailo15_general_pad_set_strength(struct udevice *dev,
                        unsigned general_pad_index,
                        unsigned strength_value)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    uint32_t data_reg;

    data_reg = readl(priv->general_pads_config_base +
             general_pad_index * GENERAL_PADS_CONFIG__OFFSET);

    /*  NOTICE!!!
      DS bits are written in reverse, meaning that the LSB is the fourth bit,
     and the MSB is the first bit. it's important to acknowledge that if wanting
     to change the DS, having to high value might burn the board.
  */

    GENERAL_PADS_CONFIG__DS_MODIFY(drive_strength_lookup[strength_value],
                       data_reg);

    writel(data_reg,
           priv->general_pads_config_base +
               (general_pad_index * GENERAL_PADS_CONFIG__OFFSET));

    pr_debug(
        "general_pad_index:%u, set drive strength:%d mA, data_reg %d\n",
        general_pad_index, strength_value, data_reg);
    return 0;
}

static int hailo15_pin_set_strength(struct udevice *dev, unsigned pin,
                    unsigned strength_value)
{
    /* make sure drive strength is supported */
    if (strength_value > 16) {
        pr_err("Error: make sure drive strength is supported");
        return -ENOTSUPP;
    }

    if (pin < H15_PINMUX_PIN_COUNT) {
        pr_err("Error: drive strength for pinmux pins is currently not supported");
        return -ENOTSUPP;
    } else {
        return hailo15_general_pad_set_strength(
            dev, pin - H15_PINMUX_PIN_COUNT, strength_value);
    }
}

static int hailo15_gpio_pin_set_pull(struct udevice *dev,
                     unsigned gpio_pad_index, unsigned config)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    uint32_t data_reg;

    data_reg =
        readl(priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS);

    data_reg &= (~(1 << gpio_pad_index));

    if (config == PIN_CONFIG_BIAS_PULL_UP) {
        data_reg |= (1 << gpio_pad_index);
    }

    writel(data_reg,
           (priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS));

    data_reg =
        readl(priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE);

    data_reg |= (1 << gpio_pad_index);
    writel(data_reg,
           (priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

    pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
         config == PIN_CONFIG_BIAS_PULL_UP ? "PULL_UP" : "PULL_DOWN",
         data_reg);
    return 0;
}

static int hailo15_gpio_pin_bias_disable(struct udevice *dev,
                     unsigned gpio_pad_index)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    uint32_t data_reg;

    data_reg =
        readl(priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE);

    data_reg &= (~(1 << gpio_pad_index));
    writel(data_reg,
           (priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

    pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
         "BIAS DISABLED", data_reg);
    return 0;
}

static int hailo15_pin_set_pull(struct udevice *dev, unsigned pin,
                unsigned config)
{
    if (pin < H15_PINMUX_PIN_COUNT) {
        return hailo15_gpio_pin_set_pull(dev, pin, config);
    } else {
        pr_err("Error: pull for un-muxable pins is currently not supported");
        return -ENOTSUPP;
    }
}

static int hailo15_pin_bias_disable(struct udevice *dev, unsigned pin)
{
	if (pin < H15_PINMUX_PIN_COUNT) {
		return hailo15_gpio_pin_bias_disable(dev, pin);
	} else {
		pr_err("Error: bias disable for un-muxable pins is currently not supported");
		return -ENOTSUPP;
	}
}

static int hailo15_pin_config_set(struct udevice *dev, unsigned pin_selector,
                  unsigned param, unsigned argument)
{
    int ret = 0;
    switch (param) {
    case PIN_CONFIG_DRIVE_STRENGTH:
        ret = hailo15_pin_set_strength(dev, pin_selector, argument);
        if (ret) {
            return ret;
        }
        break;
    case PIN_CONFIG_BIAS_PULL_UP:
    case PIN_CONFIG_BIAS_PULL_DOWN:
        ret = hailo15_pin_set_pull(dev, pin_selector, param);
        if (ret) {
            return ret;
        }
        break;
    case PIN_CONFIG_BIAS_DISABLE:
        ret = hailo15_pin_bias_disable(dev, pin_selector);
        if (ret) {
            return ret;
        }
        break;
    default:
        pr_err("Error: unsupported operation for pin config");
        return -ENOTSUPP;
    }

    return 0;
}

static int hailo15_get_functions_count(struct udevice *dev)
{
    return ARRAY_SIZE(h15_pin_functions);
}

static const char *hailo15_get_function_name(struct udevice *dev,
                                             unsigned int selector)
{
    if (selector >= ARRAY_SIZE(h15_pin_functions)) {
        return NULL;
    }
    return h15_pin_functions[selector].name;
}

static void hailo15_enable_pin(struct hailo_priv *priv, unsigned int pin_number)
{
    uint32_t data_reg;

    data_reg = readl(priv->gpio_pads_config_base +
                     GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN);
    data_reg &= (~(1 << pin_number));
    writel(data_reg,
           priv->gpio_pads_config_base +
           GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN);
    
    data_reg = readl(priv->gpio_pads_config_base +
                     GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN);
    data_reg &= (~(1 << pin_number));
    writel(data_reg,
           priv->gpio_pads_config_base +
           GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN);
}

static int hailo15_pinmux_group_set(struct udevice *dev,
                                    unsigned group_selector,
                                    unsigned function_selector)
{
    struct hailo_priv *priv = dev_get_priv(dev);

    const struct h15_pin_function *func = &h15_pin_functions[function_selector];
    const struct h15_pin_group *grp = &h15_pin_groups[group_selector];

    uint32_t new_mode = 0;
    bool is_function_supported = false;
    int i;

    /*
     * Check if group supports the given function.
     */
    for (i = 0; i < func->num_groups; i++) {
        is_function_supported |= strcmp(func->groups[i], grp->name) == 0;
    }

    if (!is_function_supported) {
        pr_err("Error: function %s is not supported for group %s\n",
               func->name, grp->name);
        return -ENOSYS;
    }

    /*
    * For every pin set in the pins group, find the permitted pinmux mode.
    */
    for (i = 0; i < grp->num_set_modes; i++) {
        struct h15_pin_set_runtime_data *pin_set_runtime_data =
            &priv->pin_set_runtime_data[grp->set_modes[i].set];

        /*
        * For every pin set, check if there is at least one mode that also exists in the possible_modes
        * (that established so far) and in the permitted modes in the new group.
        */
        if ((pin_set_runtime_data->possible_modes &
             grp->set_modes[i].modes) == 0) {
            pr_err("Error for (function name: %s, group name: %s) there is no correct mode that settles with the others selected function so far\n",
                   func->name, grp->name);
            return -EIO;
        }
    }

    /*
     * Since the driver "decodes" what is the correct mode for each pin group
     * by the functions requested so far, it can happen that it chooses a mode
     * that is correct for the functions selected so far, but it is not the "correct" mode for this board.
     * This temporary mode can set wrong functionality to some pins.
     * The "correct" mode will be selected when all the functions defined in the device tree will be processed.
     * So the driver start all 32 gpio pads in disable state to prevent set wrong functionality to some pins,
     * and the driver enable only the pins that is requested by a specific driver in hailo15_set_mux function.
    */
    for (i = 0; i < grp->num_pins; i++) {
        hailo15_enable_pin(priv, grp->pins[i]);
        priv->current_functions[grp->pins[i]] = &h15_pin_functions[function_selector];
    }

    for (i = 0; i < grp->num_set_modes; i++) {
        struct h15_pin_set_runtime_data *pin_set_runtime_data =
            &priv->pin_set_runtime_data[grp->set_modes[i].set];

        pin_set_runtime_data->possible_modes &= grp->set_modes[i].modes;

        /*
        * Find the first set bit in pin_set_runtime_data->possible_modes to determine the selected mode.
        */
        new_mode = __builtin_ctz(pin_set_runtime_data->possible_modes);

        /*
        * Update the pinmux mode for that set unless the hardware already
        * holds it. Compared against the register rather than against what
        * this driver last wrote: PADS_PINMUX is shared, and board code that
        * muxes a pad directly -- as the camera code does for i2c_2, having no
        * pinmux path in U-Boot's DT -- would otherwise make a stale shadow
        * skip a write that is genuinely needed, leaving the pad on someone
        * else's function with nothing reported.
        */
        if (new_mode != hailo15_get_pads_pinmux_mode(priv,
                                                     grp->set_modes[i].set)) {
            hailo15_set_pads_pinmux_mode(priv, grp->set_modes[i].set,
                                         new_mode);
        }
        pin_set_runtime_data->current_mode = new_mode;
    }

    return 0;
}

enum h15_pin_set_value hailo15_get_pin_set(unsigned int pin_number)
{
	enum h15_pin_set_value set;
	if (pin_number <= 1) {
		set = H15_PIN_SET_VALUE__1_0;
	} else if (pin_number <= 3) {
		set = H15_PIN_SET_VALUE__3_2;
	} else if (pin_number <= 5) {
		set = H15_PIN_SET_VALUE__5_4;
	} else if (pin_number <= 7) {
		set = H15_PIN_SET_VALUE__7_6;
	} else if (pin_number <= 15) {
		set = H15_PIN_SET_VALUE__15_8;
	} else if (pin_number <= 31) {
		set = H15_PIN_SET_VALUE__31_16;
	} else {
		return -EIO;
	}
	return set;
}

static int hailo15_gpio_request_enable(struct udevice *dev, unsigned int selector)
{
    return hailo15_pinmux_group_set(dev, selector, H15_PINMUX_GPIO_FUNCTION_SELECTOR);
}

static int hailo15_get_pin_muxing(struct udevice *dev, unsigned int selector,
                          char *buf, int size)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    const struct h15_pin_function *func;
    enum h15_pin_set_value set;
    struct h15_pin_set_runtime_data *pin_set_runtime_data;

    if (selector >= H15_PINMUX_PIN_COUNT) {
        snprintf(buf, size, "is non-muxable pin");
        return 0;
    }

    /* Muxable pin (GPIO0-GPIO31) */
    func = priv->current_functions[selector];
    set = hailo15_get_pin_set(selector);
    pin_set_runtime_data = &priv->pin_set_runtime_data[set];
    
    snprintf(
        buf,
        size,
        "set: %d, modes mask: 0x%x, mode: %d, function: %s",
        (int)set,
        pin_set_runtime_data->possible_modes,
        pin_set_runtime_data->current_mode,
        func->name);

    return 0;
}

static int hailo15_get_groups_count(struct udevice *dev)
{
    return ARRAY_SIZE(h15_pin_groups);
}

static const char *hailo15_get_group_name(struct udevice *dev,
                                          unsigned int selector)
{
    if (selector >= ARRAY_SIZE(h15_pin_groups)) {
        return NULL;
    }
    return h15_pin_groups[selector].name;
}

static int hailo15_get_gpio_mux(struct udevice *dev, int banknum, int index)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    const struct h15_pin_function *func;
    enum h15_pin_set_value set;

    int pin = banknum * H15_PINMUX_PIN_IN_BANK_COUNT + index;
    if (pin >= H15_PINMUX_PIN_COUNT) {
        return -EINVAL;
    }

    /* Muxable pin (GPIO0-GPIO31) */
    set = hailo15_get_pin_set(pin);
    func = priv->current_functions[pin];
    if (func == &unknown_function) {
        return GPIOF_UNKNOWN;
    } else if (func != &h15_pin_functions[H15_PINMUX_GPIO_FUNCTION_SELECTOR]) {
        return GPIOF_FUNC;
    }

    /* 
     * We do not mind whether it's an input or an output,
     * as long as it's a GPIO pin.
     */
    return GPIOF_INPUT;
}

static struct pinctrl_ops hailo_pinctrl_ops = {
    .get_pins_count = hailo15_get_pins_count,
    .get_pin_name = hailo15_get_pin_name,
    .set_state = pinctrl_generic_set_state,
    .pinconf_num_params = ARRAY_SIZE(hailo_conf_params),
    .pinconf_params = hailo_conf_params,
    .pinconf_set = hailo15_pin_config_set,
    .get_functions_count = hailo15_get_functions_count,
    .get_function_name = hailo15_get_function_name,
    .pinmux_group_set = hailo15_pinmux_group_set,
    .gpio_request_enable = hailo15_gpio_request_enable,
    .get_pin_muxing = hailo15_get_pin_muxing,
    .set_state = pinctrl_generic_set_state,
    .get_groups_count = hailo15_get_groups_count,
    .get_group_name = hailo15_get_group_name,
    .get_gpio_mux = hailo15_get_gpio_mux
};

static const struct udevice_id hailo_pinctrl_ids[] = {
    { .compatible = "hailo15,pinctrl" },
    {}
};

U_BOOT_DRIVER(pinctrl_hailo15) = {
    .name = "pinctrl_hailo15",
    .id = UCLASS_PINCTRL,
    .of_match = hailo_pinctrl_ids,
    .priv_auto = sizeof(struct hailo_priv),
    .ops = &hailo_pinctrl_ops,
    .probe = hailo15_pinctrl_probe,
};

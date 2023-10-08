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

struct hailo_priv {
    void __iomem *general_pads_config_base;
    void __iomem *gpio_pads_config_base;
};

static const unsigned char drive_strength_lookup[16] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

static const struct pinconf_param hailo_conf_params[] = {
    { "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 2 },
    { "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 0 },
    { "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 0 },
};

static int hailo15_get_pins_count(struct udevice *dev)
{
    return ARRAY_SIZE(hailo15_pins);
}

static int hailo15_pinctrl_probe(struct udevice *dev)
{
    struct hailo_priv *priv = dev_get_priv(dev);
    int return_value;
    struct resource res_regs;

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

    log_info("hailo15 pinctrl registered\n");
    return 0;
}

static const char *hailo15_get_pin_name(struct udevice *dev,
                    unsigned int selector)
{
    return hailo15_pins[selector].name;
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
        readl(priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE);

    data_reg |= (1 << gpio_pad_index);
    writel(data_reg,
           (priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

    data_reg =
        readl(priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS);

    data_reg &= (~(1 << gpio_pad_index));

    if (config == PIN_CONFIG_BIAS_PULL_UP) {
        data_reg |= (1 << gpio_pad_index);
    }

    writel(data_reg,
           (priv->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS));

    pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
         config == PIN_CONFIG_BIAS_PULL_UP ? "PULL_UP" : "PULL_DOWN",
         data_reg);
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
    default:
        pr_err("Error: unsupported operation for pin config");
        return -ENOTSUPP;
    }

    return 0;
}

static struct pinctrl_ops hailo_pinctrl_ops = {
    .get_pins_count = hailo15_get_pins_count,
    .get_pin_name = hailo15_get_pin_name,
    .set_state = pinctrl_generic_set_state,
    .pinconf_num_params = ARRAY_SIZE(hailo_conf_params),
    .pinconf_params = hailo_conf_params,
    .pinconf_set = hailo15_pin_config_set,
};

static const struct udevice_id hailo_pinctrl_ids[] = {
    { .compatible = "hailo15,pinctrl" },
    {}
};

U_BOOT_DRIVER(pinctrl_snapdraon) = {
    .name = "pinctrl_hailo",
    .id = UCLASS_PINCTRL,
    .of_match = hailo_pinctrl_ids,
    .priv_auto = sizeof(struct hailo_priv),
    .ops = &hailo_pinctrl_ops,
    .probe = hailo15_pinctrl_probe,
};

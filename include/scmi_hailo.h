/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 *
 * SCMI Base protocol implementation
 */
#ifndef SCMI_HAILO_H
#define SCMI_HAILO_H

#include <scmi_protocols.h>
#include <asm/types.h>

#include <scmi_hailo_protocol.h>

/**
 *
 * @dev:	SCMI agent
 * @implementation_version:	output implementation version
 * @return 0 for successful status and a negative errno otherwise
 */
#if IS_ENABLED(CONFIG_SCMI_HAILO)
int scmi_hailo_configure_ethernet_delay(
    struct udevice *dev, uint8_t tx_bypass_clock_delay, uint8_t tx_clock_inversion,
    uint8_t tx_clock_delay, uint8_t rx_bypass_clock_delay,
    uint8_t rx_clock_inversion, uint8_t rx_clock_delay);

int scmi_hailo_set_eth_rmii(struct udevice *dev);

int scmi_hailo_get_boot_info(struct udevice *dev, struct scmi_hailo_get_boot_info_p2a *boot_info);

#else
int scmi_hailo_configure_ethernet_delay(
    struct udevice *dev, uint8_t tx_bypass_clock_delay, uint8_t tx_clock_inversion,
    uint8_t tx_clock_delay, uint8_t rx_bypass_clock_delay,
    uint8_t rx_clock_inversion, uint8_t rx_clock_delay)
{
    return SCMI_NOT_SUPPORTED;
}

int scmi_hailo_set_eth_rmii(struct udevice *dev);
{
    return SCMI_NOT_SUPPORTED;
}

int scmi_hailo_get_boot_info(struct udevice *dev, struct scmi_hailo_get_boot_info_p2a *boot_info)
{
    return SCMI_NOT_SUPPORTED;
}
#endif /* IS_ENABLED(CONFIG_SCMI_HAILO) */

#endif /* SCMI_HAILO_H */

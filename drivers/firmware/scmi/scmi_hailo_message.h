#ifndef SCMI_HAILO_MESSAGE_H
#define SCMI_HAILO_MESSAGE_H

#include <asm/types.h>

#define SCMI_HAILO_CONFIGURE_ETHERNET_DELAY (4)

/**
 * struct scmi_hailo_configure_ethernet_delay_in - Message payload for Configure
 * Ethernet Delay command
 */
struct __attribute__((__packed__)) scmi_hailo_configure_ethernet_delay_in {
  u8 tx_bypass_clock_delay;
  u8 tx_clock_inversion;
  u8 tx_clock_delay;
  u8 rx_bypass_clock_delay;
  u8 rx_clock_inversion;
  u8 rx_clock_delay;
};

/**
 * struct scmi_hailo_configure_ethernet_delay_out - Response payload for
 * Configure Ethernet Delay command
 * @status:	SCMI command status
 */
struct scmi_hailo_configure_ethernet_delay_out {
  u32 status;
};

#endif /* SCMI_HAILO_MESSAGE_H */
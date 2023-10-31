#include <common.h>
#include <dm/device.h>
#include <scmi_agent.h>
#include <scmi_protocols.h>

#include <scmi_hailo.h>
#include "scmi_hailo_message.h"

int scmi_hailo_configure_ethernet_delay(
    struct udevice *dev, u8 tx_bypass_clock_delay, uint8_t tx_clock_inversion,
    uint8_t tx_clock_delay, uint8_t rx_bypass_clock_delay,
    uint8_t rx_clock_inversion, uint8_t rx_clock_delay)
{
  int ret;
  struct scmi_hailo_configure_ethernet_delay_in in = {
    .tx_bypass_clock_delay =  rx_bypass_clock_delay,
    .tx_clock_inversion = tx_clock_inversion,
    .tx_clock_delay = tx_clock_delay,
    .rx_bypass_clock_delay = rx_bypass_clock_delay,
    .rx_clock_inversion = rx_clock_inversion,
    .rx_clock_delay = rx_clock_delay,
  };
  struct scmi_hailo_configure_ethernet_delay_out out;
  struct scmi_msg msg =
      SCMI_MSG_IN(SCMI_PROTOCOL_ID_HAILO,
                  SCMI_HAILO_CONFIGURE_ETHERNET_DELAY, in, out);


  ret = devm_scmi_process_msg(dev, &msg);
  if (ret)
    return ret;

  if (out.status)
    return scmi_to_linux_errno(out.status);

  return 0;
}
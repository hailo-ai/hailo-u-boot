// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 */
 
#include <common.h>
#include <scmi_agent.h>
#include <scmi_protocols.h>
#include <dm/device.h>

int scmi_base_discover_implementation_version(struct udevice *dev, u32 *implementation_version)
{
	struct scmi_base_discover_implementation_version_in in = {};
	struct scmi_base_discover_implementation_version_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_BASE,
					  SCMI_BASE_DISCOVER_IMPLEMENTATION_VERSION,
					  in, out);
	int ret;

	ret = devm_scmi_process_msg(dev, &msg);
	if (ret)
		return ret;

	if (out.status)
		return scmi_to_linux_errno(out.status);

	*implementation_version = out.implementation_version;
	return 0;
}

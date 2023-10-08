/* SPDX-License-Identifier: GPL-2.0 */
 /*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * SCMI Base protocol implementation
 *
 */
 
#ifndef SCMI_BASE_H
#define SCMI_BASE_H

#include <asm/types.h>
/**
 *
 * @dev:	SCMI agent
 * @implementation_version:	output implementation version
 * @return 0 for successful status and a negative errno otherwise
 */
int scmi_base_discover_implementation_version(struct udevice *dev, u32 *implementation_version);

#endif /* SCMI_H */

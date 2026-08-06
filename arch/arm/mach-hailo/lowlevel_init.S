/* SPDX-License-Identifier: GPL-2.0+ */

#include <asm-offsets.h>
#include <asm/macro.h>
#include <config.h>
#include <linux/linkage.h>

ENTRY(lowlevel_init)
	/* lowlevel init is handled by TrustedFirmware-A.
	   we specifically do not move secondary cores to EL1/2 here, in order for TF-A to be running
	   in EL3 in the secondary cores. */
	ret
ENDPROC(lowlevel_init)

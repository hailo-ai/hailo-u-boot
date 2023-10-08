/* SPDX-License-Identifier: GPL-2.0+ */

#include <asm-offsets.h>
#include <asm/macro.h>
#include <config.h>
#include <linux/linkage.h>

ENTRY(lowlevel_init)
	/* lowlevel init is handled by TrustedFirmware-A.
	   we specifically do not move secondary cores to EL1/2 here, in order for TF-A to be running
	   in EL3 in the secondary cores.
	   The only thing we do is clear the CPU_RELEASE_ADDRESS */
#ifdef CONFIG_SPL_BUILD
	branch_if_slave x0, 1f
	mov	x0, #0
	ldr	x1, =CPU_RELEASE_ADDR
	str	x0, [x1]
1:
#endif
	ret
ENDPROC(lowlevel_init)

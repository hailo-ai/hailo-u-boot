#ifndef __HAILO15_DDR_CONFIG_H
#define __HAILO15_DDR_CONFIG_H

#define DDR_WORKING_MODE_NORMAL (0)
#define DDR_WORKING_MODE_DDRAPP (1)
#define DDR_WORKING_MODE_INTEGRATION (2)


//ddr modes
#define DDR0_DDR1_LEGACY_LINEAR_MODE (0)
#define DDR0_DDR1_INTERLEAVING_512B_MODE (1)
#define DDR0_DDR1_INTERLEAVING_4KB_MODE (2)
#define DDR0_DDR1_INTERLEAVING_256B_MODE (3)
#define ONLY_DDR0_ENABLED_MODE (4)
#define ONLY_DDR1_ENABLED_MODE (5)

#define DDR_ELEMENT_DISABLED (0)
#define DDR_ELEMENT_ENABLED (1)

#define DDR_BIST_MODE_PARTIAL_ADDRESS_SPACE (1)
#define DDR_BIST_MODE_FULL_ADDRESS_SPACE (2)


#define DDR_CTRL_ECC_MODE_DISABLED (0)
#define DDR_CTRL_ECC_MODE_ENABLED (1) /* ECC enabled, detection disabled, correction disabled */
#define DDR_CTRL_ECC_MODE_DETECTION (2) /* ECC enabled, detection enabled, correction disabled */
#define DDR_CTRL_ECC_MODE_CORRECTION (3)

#define DRAM_AUTO_LOW_POWER_MODE_DISABLED (0) /* DRAM Auto Low power mode disabled */
#define DRAM_AUTO_LOW_POWER_MODE_DYNAMIC_PCPCS_INTERFACE (1) /* (default) Dynamic Power Control Per Chip Select */
#define DRAM_AUTO_LOW_POWER_MODE_AUTOMATIC_INTERFACE (2) /* Automatic entry and exit for all low power states */

#define DDR_PHY_LOW_POWER_MODE_DISBALED (0)
#define DDR_PHY_LOW_POWER_MODE_LIGHT_SLEEP (1) /* (default) */
#define DDR_PHY_LOW_POWER_MODE_DEEP_SLEEP (2)

#endif /* __HAILO15_DDR_CONFIG_H */
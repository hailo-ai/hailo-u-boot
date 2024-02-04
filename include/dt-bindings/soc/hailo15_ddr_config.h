#ifndef __HAILO15_DDR_CONFIG_H
#define __HAILO15_DDR_CONFIG_H

#define DDR_WORKING_MODE_NORMAL (0)
#define DDR_WORKING_MODE_DDRAPP (1)
#define DDR_WORKING_MODE_INTEGRATION (2)

#define DDR_CTRL_ECC_MODE_DISABLED (0)
#define DDR_CTRL_ECC_MODE_ENABLED (1) /* ECC enabled, detection disabled, correction disabled */
#define DDR_CTRL_ECC_MODE_DETECTION (2) /* ECC enabled, detection enabled, correction disabled */
#define DDR_CTRL_ECC_MODE_CORRECTION (3)

#endif /* __HAILO15_DDR_CONFIG_H */
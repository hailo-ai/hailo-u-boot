#ifndef _HAILO15_BOARD_H
#define _HAILO15_BOARD_H

int hailo15_scmi_init(void);
int hailo15_scmi_check_version_match(void);
int hailo15_mmc_boot_partition(void);

#endif /* _HAILO15_BOARD_H */
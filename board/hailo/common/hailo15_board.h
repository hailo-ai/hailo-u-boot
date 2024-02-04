#ifndef _HAILO15_BOARD_H
#define _HAILO15_BOARD_H

int hailo15_scmi_init(void);
int hailo15_scmi_check_version_match(void);
extern ulong qspi_flash_ab_offset;

#endif /* _HAILO15_BOARD_H */
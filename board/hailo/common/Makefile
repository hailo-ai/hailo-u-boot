obj-y := hailo15_board.o
obj-y += hailo15_mem_map.o
obj-$(CONFIG_SPL_BUILD) += hailo15_spl.o
ifneq ($(CONFIG_SPL_BUILD),y)
obj-$(CONFIG_HAILO_SENSOR_DETECT) += hailo15_sensor_detect.o
obj-$(CONFIG_HAILO_SENSOR_DETECT) += hailo15_sensor_boards.o
endif

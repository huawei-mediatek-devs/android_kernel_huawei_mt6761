#
# Makefile for MMC/SD card drivers
#
ifeq ($(TARGET_VERSION_MODE),factory)
KBUILD_CFLAGS += -DEMMC_FMD_FACTORY_MODE
endif

obj-$(CONFIG_MMC_BLOCK)		+= mmc_block.o
mmc_block-objs			:= block.o queue.o
obj-$(CONFIG_MMC_TEST)		+= mmc_test.o

obj-$(CONFIG_SDIO_UART)		+= sdio_uart.o
obj-$(CONFIG_MMC_FFU)		+= ffu.o
obj-$(CONFIG_HW_SD_HEALTH_DETECT)	+= mmc_health_diag.o
obj-$(CONFIG_MMC_BLOCK_IO_LOG)	+= mtk_mmc_block.o

obj-$(CONFIG_MTK_MMC_PWR_WP) += mtk_emmc_write_protect.o
obj-$(CONFIG_HUAWEI_EMMC_DSM) += dsm_emmc.o
obj-$(CONFIG_HUAWEI_SDCARD_DSM) += dsm_sdcard.o

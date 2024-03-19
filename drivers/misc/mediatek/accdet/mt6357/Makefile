ifneq ($(TARGET_BUILD_VARIANT),user)
    EXTRA_CFLAGS += -DCONFIG_HUAWEI_HEADSET_DEBUG
    EXTRA_CFLAGS += -Idrivers/devkit/audiokit/headset_debug/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/fmradio/inc

obj-$(CONFIG_MTK_ACCDET) := accdet.o


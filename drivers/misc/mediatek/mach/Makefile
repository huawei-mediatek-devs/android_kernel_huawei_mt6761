#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#


# Makefile for the linux kernel.
#
# Only aarch32 need to build core/ (need machine descriptor)

#ontim add
obj-y += efusestate_info.o

subdir-ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module


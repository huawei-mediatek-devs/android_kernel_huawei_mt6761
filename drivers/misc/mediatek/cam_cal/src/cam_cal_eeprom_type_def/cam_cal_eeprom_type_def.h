/*
*  Copyright (C) 2018 Huawei Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef __CAM_CAL_EEPROM_TYPE_DEF_H
#define __CAM_CAL_EEPROM_TYPE_DEF_H

#define EEPROM_MAX_BLOCK_INFO  10
typedef struct
{
	unsigned int AddrOffset;
	unsigned int datalen;
} SENSOR_E2PROM_BLOCK_INFO_STRUCT;

typedef struct stCAM_CAL_EEPROM_TYPE_STRUCT {
	unsigned int sensorID;
	unsigned int e2prom_block_num;
	unsigned int e2prom_total_size;
	SENSOR_E2PROM_BLOCK_INFO_STRUCT e2prom_block_info[EEPROM_MAX_BLOCK_INFO];
}stCAM_CAL_EEPROM_TYPE_STRUCT_ST;

static stCAM_CAL_EEPROM_TYPE_STRUCT_ST g_camCalE2promMapInfo[] =
{
    #include "gc8034_foxconn_eeprom_map.h"
    #include "hi846_byd_eeprom_map.h"
    #include "hi846_truly_eeprom_map.h"
    #include "hi1333_qtech_eeprom_map.h"
    #include "imx258_sunny_eeprom_map.h"
    #include "gc5025_holi_eeprom_map.h"
    #include "imx258_zet_eeprom_map.h"
    #include "ov13855_ofilm_eeprom_map.h"
    #include "ov13855_ofilm_tdk_eeprom_map.h"
    #include "s5k3l6_liteon_eeprom_map.h"
    #include "s5k4h7_byd_eeprom_map.h"
    #include "s5k4h7_ofilm_eeprom_map.h"
    #include "gc5025_king_eeprom_map.h"
};

#endif

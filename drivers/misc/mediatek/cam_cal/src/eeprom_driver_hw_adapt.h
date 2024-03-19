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

#ifndef __EEPROM_DRIVER_HW_ADAPT_H
#define __EEPROM_DRIVER_HW_ADAPT_H
#include <linux/i2c.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "cam_cal_eeprom_type_def/cam_cal_eeprom_type_def.h"
#include "cam_cal_otp_page_type_def/cam_cal_otp_page_type_def.h"

int camcal_read_eeprom_func(struct i2c_client *current_Client, unsigned int sensorID, unsigned char **eeprombuff, unsigned int *eepromsize);
int camcal_read_otp_page_func(struct i2c_client *current_Client, unsigned int sensorID, unsigned char **eeprombuff, unsigned int *eepromsize);
unsigned int Common_read_region(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size);
unsigned int cam_cal_get_sensor_list(struct stCAM_CAL_LIST_STRUCT **ppCamcalList);
struct i2c_client *  EEPROM_get_i2c_client(unsigned int deviceID);

#define EEPROM_READ_DATA_SIZE_8                   8
#define EEPROM_READ_FAIL_RETRY_TIMES              3

/*Note: Must Mapping to IHalSensor.h*/
enum {
	SENSOR_DEV_NONE = 0x00,
	SENSOR_DEV_MAIN = 0x01,
	SENSOR_DEV_SUB = 0x02,
	SENSOR_DEV_PIP = 0x03,
	SENSOR_DEV_MAIN_2 = 0x04,
	SENSOR_DEV_MAIN_3D = 0x05,
	SENSOR_DEV_SUB_2 = 0x08,
	SENSOR_DEV_MAX = 0x50
};

/***********************************************************
 *
 ***********************************************************/
typedef int (*camcal_read_func_type)(struct i2c_client *, unsigned int, unsigned char **, unsigned int *);

typedef struct stCAM_CAL_MTK_MAP_STRUCT {
	unsigned int sensorID;
	unsigned int deviceID;
	unsigned int i2c_addr;
	unsigned char * eeprombuff;
	unsigned int buffsize;
	camcal_read_func_type camcal_read_func;
	bool   activeflag;
}CAM_CAL_MTK_MAP_STRUCT_ST;

static CAM_CAL_MTK_MAP_STRUCT_ST g_camCalMtkMapInfo[] =
{
	{IMX258_SENSOR_ID,             SENSOR_DEV_MAIN, 0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{IMX258_ZET_SENSOR_ID,         SENSOR_DEV_MAIN, 0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{OV13855_OFILM_SENSOR_ID,      SENSOR_DEV_MAIN, 0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{OV13855_OFILM_TDK_SENSOR_ID,  SENSOR_DEV_MAIN, 0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{S5K3L6_LITEON_SENSOR_ID,      SENSOR_DEV_MAIN, 0xA2, NULL, 0, camcal_read_eeprom_func,   false},
	{HI1333_QTECH_SENSOR_ID,       SENSOR_DEV_MAIN, 0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{GC8034_FOXCONN_SENSOR_ID,     SENSOR_DEV_SUB,  0xA4, NULL, 0, camcal_read_eeprom_func,   false},
	{HI846_TRULY_SENSOR_ID,        SENSOR_DEV_SUB,  0xA4, NULL, 0, camcal_read_eeprom_func,   false},
	{HI846_BYD_SENSOR_ID,          SENSOR_DEV_SUB,  0xA4, NULL, 0, camcal_read_eeprom_func,   false},
	{OV8856_SENSOR_ID,             SENSOR_DEV_SUB,  0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{S5K4H7_BYD_SENSOR_ID,         SENSOR_DEV_SUB,  0xA4, NULL, 0, camcal_read_eeprom_func,   false},
	{S5K4H7_OFILM_SENSOR_ID,       SENSOR_DEV_SUB,  0xA4, NULL, 0, camcal_read_eeprom_func,   false},
	{ HI556_LITEARY_SENSOR_ID,     SENSOR_DEV_SUB,  0x40, NULL, 0, camcal_read_otp_page_func,   false },
	{HI556_HOLI_SENSOR_ID,         SENSOR_DEV_SUB,  0x50, NULL, 0, camcal_read_otp_page_func,   false},
	{GC5025_HOLI_SENSOR_ID,        SENSOR_DEV_SUB,  0xA0, NULL, 0, camcal_read_eeprom_func,   false},
	{GC5025_KING_SENSOR_ID,        SENSOR_DEV_SUB,  0xA0, NULL, 0, camcal_read_eeprom_func,   false},
};

#endif

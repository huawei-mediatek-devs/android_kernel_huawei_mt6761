/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include "kd_imgsensor.h"

#include "regulator/regulator.h"
#include "gpio/gpio.h"
/*#include "mt6306/mt6306.h"*/
#include "mclk/mclk.h"



#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};

struct IMGSENSOR_HW_CFG imgsensor_custom_config[IMGSENSOR_SENSOR_IDX_NONE] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};

struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		IMGSENSOR_SENSOR_IDX_NAME_SUB,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		}
	},
	{
		IMGSENSOR_SENSOR_IDX_NAME_MAIN2,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		}
	},
#endif

	{NULL}
};

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(HI1333_QTECH)
	{
		SENSOR_DRVNAME_HI1333_QTECH,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 0},
			{SensorMCLK, Vol_High, 3},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV13855_OFILM_TDK)
	{
		SENSOR_DRVNAME_OV13855_OFILM_TDK,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1250, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 1},
			{RST, Vol_High, 6},
			{SensorMCLK, Vol_High, 5},
		},
	},
#endif
#if defined(OV13855_OFILM)
	{
		SENSOR_DRVNAME_OV13855_OFILM,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1250, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 1},
			{RST, Vol_High, 6},
			{SensorMCLK, Vol_High, 5},
		},
	},
#endif
#if defined(IMX258_SUNNY_ZET)
	{
		SENSOR_DRVNAME_IMX258_SUNNY_ZET,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 1},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX258_SUNNY)
	{
		SENSOR_DRVNAME_IMX258_SUNNY,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 1},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K3L6_LITEON)
	{
		SENSOR_DRVNAME_S5K3L6_LITEON,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1050, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_3000, 0},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 5},
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 3},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
#if defined(HI846_TRULY)
	{SENSOR_DRVNAME_HI846_TRULY,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(HI846_BYD)
	{SENSOR_DRVNAME_HI846_BYD,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(GC8034_FOXCONN)
	{SENSOR_DRVNAME_GC8034_FOXCONN,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1250, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2},
		},
	},
#endif
#if defined(S5K4H7_BYD)
	{
		SENSOR_DRVNAME_S5K4H7_BYD,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
#if defined(S5K4H7_OFILM)
	{
		SENSOR_DRVNAME_S5K4H7_OFILM,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
	/* add new sensor before this line */
	{NULL,},
};

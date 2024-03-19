/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This should be the same as
 *     mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR] = {
#if defined(IMX258_SUNNY_ZET)
	{IMX258_ZET_SENSOR_ID,
	SENSOR_DRVNAME_IMX258_SUNNY_ZET,
	IMX258_SUNNY_ZET_SensorInit},
#endif
#if defined(IMX258_SUNNY)
	{IMX258_SENSOR_ID,
	SENSOR_DRVNAME_IMX258_SUNNY,
	IMX258_SUNNY_SensorInit},
#endif
#if defined(OV13855_OFILM_TDK)
	{OV13855_OFILM_TDK_SENSOR_ID,
	SENSOR_DRVNAME_OV13855_OFILM_TDK,
	OV13855_OFILM_TDK_SensorInit},
#endif
#if defined(OV13855_OFILM)
	{OV13855_OFILM_SENSOR_ID,
	SENSOR_DRVNAME_OV13855_OFILM,
	OV13855_OFILM_SensorInit},
#endif
#if defined(OV8856_MIPI_RAW)
	{OV8856_SENSOR_ID,
	SENSOR_DRVNAME_OV8856_MIPI_RAW,
	OV8856_MIPI_RAW_SensorInit},
#endif
#if defined(S5K3L6_LITEON)
	{S5K3L6_LITEON_SENSOR_ID,
	SENSOR_DRVNAME_S5K3L6_LITEON,
	S5K3L6_LITEON_SensorInit},
#endif
#if defined(S5K4H7_BYD)
	{S5K4H7_BYD_SENSOR_ID,
	SENSOR_DRVNAME_S5K4H7_BYD,
	S5K4H7_BYD_SensorInit},
#endif
#if defined(S5K4H7_OFILM)
	{S5K4H7_OFILM_SENSOR_ID,
	SENSOR_DRVNAME_S5K4H7_OFILM,
	S5K4H7_OFILM_SensorInit},
#endif
#if defined(HI1333_QTECH)
	{HI1333_QTECH_SENSOR_ID,
	SENSOR_DRVNAME_HI1333_QTECH,
	HI1333_QTECH_SensorInit},
#endif
#if defined(HI846_TRULY)
	{HI846_TRULY_SENSOR_ID,
	SENSOR_DRVNAME_HI846_TRULY,
	HI846_TRULY_SensorInit},
#endif
#if defined(HI846_BYD)
	{HI846_BYD_SENSOR_ID,
	SENSOR_DRVNAME_HI846_BYD,
	HI846_BYD_SensorInit},
#endif
#if defined(HI556_LITEARY)
	{HI556_LITEARY_SENSOR_ID,
	SENSOR_DRVNAME_HI556_LITEARY,
	HI556_LITEARY_SensorInit},
#endif
#if defined(GC8034_FOXCONN)
	{GC8034_FOXCONN_SENSOR_ID,
	SENSOR_DRVNAME_GC8034_FOXCONN,
	GC8034_FOXCONN_SensorInit},
#endif
#if defined(GC5025_HOLI)
	{GC5025_HOLI_SENSOR_ID,
	SENSOR_DRVNAME_GC5025_HOLI,
	GC5025_HOLI_SensorInit},
#endif
#if defined(HI556_HOLI)
	{HI556_HOLI_SENSOR_ID,
	SENSOR_DRVNAME_HI556_HOLI,
	HI556_HOLI_SensorInit},
#endif
#if defined(GC5025_KING)
	{GC5025_KING_SENSOR_ID,
	SENSOR_DRVNAME_GC5025_KING,
	GC5025_KING_SensorInit},
#endif
	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */


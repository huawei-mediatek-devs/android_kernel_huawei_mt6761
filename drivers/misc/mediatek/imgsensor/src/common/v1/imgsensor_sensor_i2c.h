#ifndef _IMGSENSOR_SENSOR_I2C_H
#define _IMGSENSOR_SENSOR_I2C_H

#include "imgsensor_sensor_common.h"

kal_int32 imgsensor_sensor_i2c_read(imgsensor_t *sensor,
	kal_uint32 addr, kal_uint16 *data,
	enum imgsensor_i2c_data_type data_type);

kal_int32 imgsensor_sensor_i2c_write(imgsensor_t *sensor,
	kal_uint32 addr, kal_uint16 data,
	enum imgsensor_i2c_data_type data_type);

kal_int32 imgsensor_sensor_i2c_poll(imgsensor_t *sensor,
	kal_uint32 addr, kal_uint16 data,
	enum imgsensor_i2c_data_type data_type, kal_uint16 delay);

kal_int32 imgsensor_sensor_write_table(imgsensor_t *sensor,
	struct imgsensor_i2c_reg *setting, kal_uint32 size,
	enum imgsensor_i2c_data_type data_type);

kal_int32 imgsensor_sensor_write_setting(imgsensor_t *sensor,
	imgsensor_i2c_reg_setting_t *settings);

kal_int32 imgsensor_sensor_i2c_process(imgsensor_t *sensor,
	imgsensor_i2c_reg_table_array_t *settings);

#endif

#ifndef __IMGSENSOR_DT_UTIL_H__
#define __IMGSENSOR_DT_UTIL_H__

#define IMGSENSOR_PROPERTY_MAXSIZE 32
#define IMGSENSOR_STRING_MAXSIZE 32

#define IMGSENSOR_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct IMGSENSOR_HW_DT_POWER_INFO{
	char id[IMGSENSOR_STRING_MAXSIZE];
	char pin[IMGSENSOR_STRING_MAXSIZE];
};

struct IMGSENSOR_DT_HW_CFG{
	enum   IMGSENSOR_SENSOR_IDX           sensor_idx;
	enum   IMGSENSOR_I2C_DEV              i2c_dev;
	struct IMGSENSOR_HW_DT_POWER_INFO
	    pwr_info[IMGSENSOR_HW_POWER_INFO_MAX];
};

struct IMGSENSOR_STRING_TO_ENUM{
	const char *string;
	int enum_val;
};

#define STRING_TO_ENUM(_string, _enum) \
{                               \
  .string = _string, \
  .enum_val = _enum,  \
}

int imgsensor_dt_hw_config_init(struct IMGSENSOR_HW_CFG *pwr_cfg, unsigned int pwr_cfg_size);

#endif /*__IMGSENSOR_DT_UTIL_H__*/


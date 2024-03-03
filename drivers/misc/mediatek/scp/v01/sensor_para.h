#ifndef __SENSOR_PARA_H
#define __SENSOR_PARA_H

struct als_para_t{
	uint8_t tp_type;
	uint8_t tp_color;
	int cali_target[2];
	uint8_t als_extend_para[50];
	uint8_t product_name;
};

struct ps_para_t{
	uint8_t threshold_h;
	uint8_t threshold_l;
	uint8_t min_prox;
	uint8_t pwave;
	uint8_t pwindow;
	uint8_t product_name;
};

struct acc_para_t{
 	uint8_t axis_map_x;
 	uint8_t axis_map_y;
 	uint8_t axis_map_z;
 	uint8_t negate_x;
 	uint8_t negate_y;
 	uint8_t negate_z;
};

struct sar_para_t{
	uint8_t reg[10];
	uint8_t reg_val[10];
	uint32_t product_name;
};

struct mag_para_t{
 	uint8_t axis_map_x;
 	uint8_t axis_map_y;
 	uint8_t axis_map_z;
 	uint8_t negate_x;
 	uint8_t negate_y;
 	uint8_t negate_z;
};
struct sensor_para_t{
	struct als_para_t als_para;
	struct ps_para_t ps_para;
	struct acc_para_t acc_para;
	struct sar_para_t sar_para;
	struct mag_para_t mag_para;
};
extern struct sensor_para_t *sensor_para;
#endif

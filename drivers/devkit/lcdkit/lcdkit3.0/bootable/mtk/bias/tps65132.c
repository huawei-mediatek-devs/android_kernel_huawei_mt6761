/*
 * drivers/huawei/drivers/tps65132.c
 *
 * tps65132 driver reffer to lcd
 *
 * Copyright (C) 2012-2015 HUAWEI, Inc.
 * Author: HUAWEI, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lcd_kit_common.h"
#include "lcd_kit_bias.h"
#include "tps65132.h"
#include <libfdt.h>
#include <platform/mt_i2c.h>

#define TPS65132_SLAV_ADDR 0x3E

extern void *g_fdt;
static unsigned int i2c_bus_id = 0;

static int tps65132_i2c_read_u8(unsigned char addr, unsigned char *dataBuffer)
{
	int ret = 0;
	unsigned char len = 0;
	struct mt_i2c_t tps65132_i2c = {0};
	*dataBuffer = addr;

	tps65132_i2c.id = i2c_bus_id;
	tps65132_i2c.addr = TPS65132_SLAV_ADDR;
	tps65132_i2c.mode = ST_MODE;
	tps65132_i2c.speed = 100;
	tps65132_i2c.st_rs = I2C_TRANS_REPEATED_START;
	len = 1;

	ret = i2c_write_read(&tps65132_i2c, dataBuffer, len, len);
	if (0 != ret)
	{
		LCD_KIT_ERR("%s: i2c_read  failed! reg is 0x%x ret: %d\n", __func__, addr, ret);
    }
	return ret;
}

static int tps65132_i2c_write_u8(unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};
	unsigned char len = 0;
	struct mt_i2c_t tps65132_i2c = {0};

	write_data[0] = addr;
	write_data[1] = value;

	tps65132_i2c.id = i2c_bus_id;
	tps65132_i2c.addr = TPS65132_SLAV_ADDR;
	tps65132_i2c.mode = ST_MODE;
	tps65132_i2c.speed = 100;
	len = 2;

	ret = i2c_write(&tps65132_i2c, write_data, len);
	if (0 != ret)
	{
		LCD_KIT_ERR("%s: i2c_write  failed! reg is  0x%x ret: %d\n", __func__, addr, ret);
    }
	return ret;
}

static int tps65132_reg_inited(unsigned char vpos_target_cmd, unsigned char vneg_target_cmd)
{
	unsigned char vpos = 0;
	unsigned char vneg = 0;
	int ret = 0;

	ret = tps65132_i2c_read_u8(TPS65132_REG_VPOS, &vpos);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read vpos voltage failed\n",__FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_read_u8(TPS65132_REG_VNEG, &vneg);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read vneg voltage failed\n",__FUNCTION__);
		goto exit;
	}

	LCD_KIT_INFO("vpos : 0x%x, vneg: 0x%x\n", vpos, vpos);

	if(((vpos & TPS65132_REG_VOL_MASK) == vpos_target_cmd)
		&& ((vneg & TPS65132_REG_VOL_MASK) == vneg_target_cmd))
		ret = 1;
	else
		ret = 0;
exit:
	return ret;
}

static int tps65132_reg_init(unsigned char vpos_cmd, unsigned char vneg_cmd)
{
	unsigned char vpos = 0;
	unsigned char vneg = 0;
	unsigned char  app_dis = 0;
	unsigned char  ctl = 0;
	int ret = 0;

	ret = tps65132_i2c_read_u8(TPS65132_REG_VPOS, &vpos);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read vpos voltage failed\n", __FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_read_u8(TPS65132_REG_VNEG, &vneg);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read vneg voltage failed\n", __FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_read_u8(TPS65132_REG_APP_DIS, &app_dis);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read app_dis failed\n", __FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_read_u8(TPS65132_REG_CTL,  &ctl);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:read ctl failed\n", __FUNCTION__);
		goto exit;
	}

	vpos = (vpos&(~TPS65132_REG_VOL_MASK)) | vpos_cmd;
	vneg = (vneg&(~TPS65132_REG_VOL_MASK)) | vneg_cmd;

	app_dis = app_dis | TPS65312_APPS_BIT | TPS65132_DISP_BIT | TPS65132_DISN_BIT;
	ctl = ctl | TPS65132_WED_BIT;

	ret = tps65132_i2c_write_u8(TPS65132_REG_VPOS, vpos);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:write vpos failed\n", __FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_write_u8(TPS65132_REG_VNEG, vneg);
	if (ret != 0) {
		LCD_KIT_ERR("[%s]:write vneg failed\n", __FUNCTION__);
		goto exit;
	}

	ret = tps65132_i2c_write_u8(TPS65132_REG_APP_DIS, app_dis);
	if (ret != 0) {
		LCD_KIT_ERR("%s write app_dis failed\n");
		goto exit;
	}

	ret = tps65132_i2c_write_u8(TPS65132_REG_CTL, ctl);
	if (ret != 0) {
		LCD_KIT_ERR("%s write ctl failed\n");
		goto exit;
	}

	mdelay(60);

exit:
	return ret;
}

static int tps65132_device_verify(void)
{
	int ret = 0;
	unsigned char app_dis = 0;

	ret = tps65132_i2c_read_u8(TPS65132_REG_APP_DIS, &app_dis);
	if (ret < 0) {
		LCD_KIT_ERR("no tps65132 device, read app_dis failed\n");
		return -1;
	}
	LCD_KIT_INFO("tps65132 verify ok, app_dis = 0x%x\n", app_dis);

	return 0;
}

static void tps65132_get_target_voltage(int *vpos_target, int *vneg_target)

{
	*vpos_target = TPS65132_VOL_55;
	*vneg_target = TPS65132_VOL_55;
	return ;
}

void tps65132_set_voltage(int vpos, int vneg)
{
	int ret = 0;

	if (vpos < TPS65132_VOL_40 || vpos >= TPS65132_VOL_MAX) {
		LCD_KIT_ERR("set vpos error, vpos = %d is out of range", vpos);
		return;
	}

	if (vneg < TPS65132_VOL_40 || vneg >= TPS65132_VOL_MAX) {
		LCD_KIT_ERR("set vneg error, vneg = %d is out of range", vneg);
		return;
	}

	ret = tps65132_reg_inited(vpos, vneg);
	if (ret > 0) {
		LCD_KIT_ERR("tps65132 inited needn't reset value\n");
	} else if (ret < 0) {
		LCD_KIT_ERR("tps65132 I2C read not success\n");
	} else {
		ret = tps65132_reg_init(vpos, vneg);
		if (ret) {
			LCD_KIT_ERR("tps65132_reg_init not success\n");
		}
		LCD_KIT_ERR("tps65132 inited succeed\n");
	}
}

static int tps65132_set_bias(int vpos, int vneg)
{
	int i = 0;

	for(i = 0;i < sizeof(voltage_table) / sizeof(struct tps65132_voltage);i++) {
		if(voltage_table[i].voltage == vpos) {
			LCD_KIT_INFO("tps65132 vsp voltage:0x%x\n",
				voltage_table[i].value);
			vpos = voltage_table[i].value;
			break;
		}
	}
	if (i >= sizeof(voltage_table) / sizeof(struct tps65132_voltage)) {
		LCD_KIT_ERR("not found vsp voltage, use default voltage:TPS65132_VOL_55\n");
		vpos = TPS65132_VOL_55;
	}
	for(i = 0;i < sizeof(voltage_table) / sizeof(struct tps65132_voltage);i++) {
		if(voltage_table[i].voltage == vneg) {
			LCD_KIT_INFO("tps65132 vsn voltage:0x%x\n",
				voltage_table[i].value);
			vneg = voltage_table[i].value;
			break;
		}
	}

	if (i >= sizeof(voltage_table) / sizeof(struct tps65132_voltage)) {
		LCD_KIT_ERR("not found vsn voltage, use default voltage:TPS65132_VOL_55\n");
		vneg = TPS65132_VOL_55;
	}
	LCD_KIT_INFO("vpos = 0x%x, vneg = 0x%x\n", vpos, vneg);
	tps65132_set_voltage(vpos, vneg);
	return 0;
}
static void tps65132_set_bias_status (char *compname, char *status)
{
	int ret = 0;
	int offset = 0;

	if (!compname) {
		LCD_KIT_ERR("type is NULL!\n");
		return;
	}

	offset = fdt_node_offset_by_compatible(g_fdt, 0, compname);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find panel node, change fb dts failed\n");
		return;
	}

	ret = fdt_setprop_string(g_fdt, offset, (const char*)"status", (const void*)status);
	if (ret) {
		LCD_KIT_ERR("Cannot update lcd panel type(errno=%d)!\n", ret);
		return;
	}

	LCD_KIT_INFO("lcdkit_set_lcd_panel_status OK!\n");
	return;
}

static struct lcd_kit_bias_ops bias_ops = {
	.set_bias_voltage = tps65132_set_bias,
};

int tps65132_bias_recognize(void)
{
	int ret = 0;
	ret = tps65132_device_verify();
	if (ret < 0) {
		tps65132_set_bias_status(DTS_COMP_TPS65132,"disabled");
		LCD_KIT_ERR("tps65132 is not right bias\n");
		return ret;
	}
	else{
		lcd_kit_bias_register(&bias_ops);
		tps65132_set_bias_status(DTS_COMP_TPS65132,"okay");
		LCD_KIT_INFO("tps65132 is right bias\n");
		return ret;
	}
}

int tps65132_init(void)
{
	int ret = 0;
	unsigned int support = 0;

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_TPS65132, TPS65132_SUPPORT, &support, 0);
	if (ret < 0 || !support) {
		LCD_KIT_ERR("not support tps65132!\n");
		return 0;
	}
	tps65132_set_bias_status(DTS_COMP_TPS65132,"disabled");
	/*register bias ops*/
	lcd_kit_bias_recognize_register(tps65132_bias_recognize);
	ret = lcd_kit_parse_get_u32_default(DTS_COMP_TPS65132, TPS65132_I2C_BUS_ID, &i2c_bus_id, 0);
	if (ret < 0) {
		i2c_bus_id = 0;
		return 0;
	}
	LCD_KIT_INFO("[%s]:tps65132 is support!\n", __FUNCTION__);
	return 0;
}


/*
 * Copyright 2013 HUAWEI Tech. Co., Ltd.
 */


#ifndef __TPS65132_H
#define __TPS65132_H

enum {
	TPS65132_VOL_40   =  (0x00),     //4.0V
	TPS65132_VOL_41   =  (0x01),     //4.1V
	TPS65132_VOL_42   =  (0x02),     //4.2V
	TPS65132_VOL_43   =  (0x03),     //4.3V
	TPS65132_VOL_44   =  (0x04),     //4.4V
	TPS65132_VOL_45   =  (0x05),     //4.5V
	TPS65132_VOL_46   =  (0x06),     //4.6V
	TPS65132_VOL_47   =  (0x07),     //4.7V
	TPS65132_VOL_48   =  (0x08),     //4.8V
	TPS65132_VOL_49   =  (0x09),     //4.9V
	TPS65132_VOL_50   =  (0x0A),     //5.0V
	TPS65132_VOL_51   =  (0x0B),     //5.1V
	TPS65132_VOL_52   =  (0x0C),     //5.2V
	TPS65132_VOL_53   =  (0x0D),     //5.3V
	TPS65132_VOL_54   =  (0x0E),     //5.4V
	TPS65132_VOL_55   =  (0x0F),     //5.5V
	TPS65132_VOL_56   =  (0x10),     //5.6V
	TPS65132_VOL_57   =  (0x11),     //5.7V
	TPS65132_VOL_58   =  (0x12),     //5.8V
	TPS65132_VOL_59   =  (0x13),     //5.9V
	TPS65132_VOL_60   =  (0x14),     //6.0V
	TPS65132_VOL_MAX
};

struct tps65132_voltage {
	u32 voltage;
	int value;
};

static struct tps65132_voltage voltage_table[] = {
	{4000000,TPS65132_VOL_40},
	{4100000,TPS65132_VOL_41},
	{4200000,TPS65132_VOL_42},
	{4300000,TPS65132_VOL_43},
	{4400000,TPS65132_VOL_44},
	{4500000,TPS65132_VOL_45},
	{4600000,TPS65132_VOL_46},
	{4700000,TPS65132_VOL_47},
	{4800000,TPS65132_VOL_48},
	{4900000,TPS65132_VOL_49},
	{5000000,TPS65132_VOL_50},
	{5100000,TPS65132_VOL_51},
	{5200000,TPS65132_VOL_52},
	{5300000,TPS65132_VOL_53},
	{5400000,TPS65132_VOL_54},
	{5500000,TPS65132_VOL_55},
	{5600000,TPS65132_VOL_56},
	{5700000,TPS65132_VOL_57},
	{5800000,TPS65132_VOL_58},
	{5900000,TPS65132_VOL_59},
	{6000000,TPS65132_VOL_60},
};
#define TPS65132_MODULE_NAME_STR     "tps65132"
#define TPS65132_MODULE_NAME          tps65132
#define DTS_COMP_TPS65132 "ti,tps65132"
#define TPS65132_SUPPORT "tps65132_support"
#define DTS_TPS65132 "ti,tps65132"
#define TPS65132_I2C_BUS_ID "tps65132_i2c_bus_id"
#define TPS65132_REG_VPOS   (0x00)
#define TPS65132_REG_VNEG   (0x01)
#define TPS65132_REG_APP_DIS   (0x03)
#define TPS65132_REG_CTL    (0xFF)

#define TPS65132_REG_VOL_MASK  (0x1F)
#define TPS65312_APPS_BIT   (1<<6)
#define TPS65132_DISP_BIT   (1<<1)
#define TPS65132_DISN_BIT   (1<<0)
#define TPS65132_WED_BIT    (1<<7)   //write enable bit

void tps65132_set_voltage(int vpos, int vneg);
#endif

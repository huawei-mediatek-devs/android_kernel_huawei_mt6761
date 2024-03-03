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
#ifndef _GC5025_LCEO_SENSOR_H
#define _GC5025_LCEO_SENSOR_H

#define ANALOG_GAIN_1X 			0x40   /* 1.00x */
#define ANALOG_GAIN_2X 			0x5B   /* 1.42x */
#define ANALOG_GAIN_3X 			0x7F  /* 1.99x */
#define SENSOR_MAX_ALL_GAIN		1024

#define SENSOR_CISCTRL_CAPT_VB_REG_H	0x07
#define SENSOR_CISCTRL_CAPT_VB_REG_L	0x08
#define SENSOR_PAGE_SELECT_REG		0xfe
#define SENSOR_CAL_SHUTTER_REG_H	0xd9
#define SENSOR_CAL_SHUTTER_REG_L	0xb0
#define SENSOR_SET_GAIN_REG		0xb6
#define SENSOR_SET_GAIN_REG_H		0xb1
#define SENSOR_SET_GAIN_REG_L		0xb2
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_H	0x03
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_L	0x04
#define SENSOR_SET_PATTERN_MODE		0x8c

#define DD_PARAM_QTY				200
#define WINDOW_WIDTH				0x0a30 /* 2608 max effective pixels */
#define WINDOW_HEIGHT				0x079c /* 1948 */
#define REG_ROM_START				0x62
#define OTP_BYTE_MODE				0x20
#define OTP_CONTINUE_MODE			0x88
#define FLAG_DD					0x00
#define SENSOR_CHIPVERSION_FLAG			0x7F
#define MAX_DD_NUM          			50
#define DD_INFO     				4
#define DD_VALID     				0x03
#define OTP_FLAG_CHIPVERSION    		0x03
#define DD_START_PIXEL				0x0C
#define PAGE0_DD_SIZE				31

static struct imgsensor_i2c_reg stream_on[] = {
	{0xfe, 0x00, 0x00},
	{0x3f, 0x91, 0x00},
	{0xfe, 0x00, 0x00},
};

static struct imgsensor_i2c_reg stream_off[] = {
	{0xfe, 0x00, 0x00},
	{0x3f, 0x00, 0x00},
	{0xfe, 0x00, 0x00},
};

static struct imgsensor_i2c_reg init_setting[] = {
	{0xfe, 0x00, 0x00},
	{0xfe, 0x00, 0x00},
	{0xfe, 0x00, 0x00},
	{0xf7, 0x01, 0x00},
	{0xf8, 0x11, 0x00},
	{0xf9, 0x00, 0x00},
	{0xfa, 0xa0, 0x00},
	{0xfc, 0x2a, 0x00},
	{0xfe, 0x03, 0x00},
	{0x01, 0x07, 0x00},
	{0xfc, 0x2e, 0x00},
	{0xfe, 0x00, 0x00},
	{0x88, 0x03, 0x00},
	{0x03, 0x07, 0x00},
	{0x04, 0x08, 0x00},
	{0x05, 0x02, 0x00},
	{0x06, 0x58, 0x00},
	{0x08, 0x20, 0x00},
	{0x0a, 0x1c, 0x00},
	{0x0c, 0x04, 0x00},
	{0x0d, 0x07, 0x00},
	{0x0e, 0x9c, 0x00},
	{0x0f, 0x0a, 0x00},
	{0x10, 0x30, 0x00},
	{0x17, 0xc0, 0x00},
	{0x18, 0x02, 0x00},
	{0x19, 0x17, 0x00},
	{0x1a, 0x0a, 0x00},
	{0x1c, 0x2c, 0x00},
	{0x1d, 0x00, 0x00},
	{0x1e, 0xa0, 0x00},
	{0x1f, 0xb0, 0x00},
	{0x20, 0x22, 0x00},
	{0x21, 0x22, 0x00},
	{0x26, 0x22, 0x00},
	{0x25, 0xc1, 0x00},
	{0x27, 0x64, 0x00},
	{0x28, 0x00, 0x00},
	{0x29, 0x44, 0x00},
	{0x2b, 0x80, 0x00},
	{0x2f, 0x4d, 0x00},
	{0x30, 0x11, 0x00},
	{0x31, 0x20, 0x00},
	{0x32, 0xc0, 0x00},
	{0x33, 0x00, 0x00},
	{0x34, 0x60, 0x00},
	{0x38, 0x04, 0x00},
	{0x39, 0x02, 0x00},
	{0x3a, 0x00, 0x00},
	{0x3b, 0x00, 0x00},
	{0x3c, 0x08, 0x00},
	{0x3d, 0x0f, 0x00},
	{0x81, 0x50, 0x00},
	{0xcb, 0x02, 0x00},
	{0xcd, 0x4d, 0x00},
	{0xcf, 0x50, 0x00},
	{0xd0, 0xb3, 0x00},
	{0xd1, 0x19, 0x00},
	{0xd3, 0xc4, 0x00},
	{0xd9, 0xaa, 0x00},
	{0xdc, 0x03, 0x00},
	{0xdd, 0xc8, 0x00},
	{0xe0, 0x00, 0x00},
	{0xe1, 0x1c, 0x00},
	{0xe3, 0x2a, 0x00},
	{0xe4, 0xc0, 0x00},
	{0xe5, 0x06, 0x00},
	{0xe6, 0x10, 0x00},
	{0xe7, 0xc3, 0x00},
	{0xfe, 0x10, 0x00},
	{0xfe, 0x00, 0x00},
	{0xfe, 0x10, 0x00},
	{0xfe, 0x00, 0x00},
	{0x80, 0x10, 0x00},
	{0x89, 0x03, 0x00},
	{0x8b, 0x31, 0x00},
	{0xfe, 0x01, 0x00},
	{0x88, 0x00, 0x00},
	{0x8a, 0x03, 0x00},
	{0x8e, 0xc7, 0x00},
	{0xfe, 0x00, 0x00},
	{0x40, 0x22, 0x00},
	{0x41, 0x28, 0x00},
	{0x42, 0x04, 0x00},
	{0x43, 0x08, 0x00},
	{0x4e, 0x0f, 0x00},
	{0x4f, 0xf0, 0x00},
	{0x67, 0x0c, 0x00},
	{0xae, 0x40, 0x00},
	{0xaf, 0x04, 0x00},
	{0x60, 0x00, 0x00},
	{0x61, 0x80, 0x00},
	{0xb0, 0x4d, 0x00},
	{0xb1, 0x01, 0x00},
	{0xb2, 0x00, 0x00},
	{0xb6, 0x00, 0x00},
	{0x91, 0x00, 0x00},
	{0x92, 0x02, 0x00},
	{0x94, 0x03, 0x00},
	{0xfe, 0x03, 0x00},
	{0x02, 0x03, 0x00},
	{0x03, 0x8e, 0x00},
	{0x06, 0x80, 0x00},
	{0x15, 0x00, 0x00},
	{0x16, 0x09, 0x00},
	{0x18, 0x0a, 0x00},
	{0x21, 0x10, 0x00},
	{0x22, 0x05, 0x00},
	{0x23, 0x20, 0x00},
	{0x24, 0x02, 0x00},
	{0x25, 0x20, 0x00},
	{0x26, 0x08, 0x00},
	{0x29, 0x06, 0x00},
	{0x2a, 0x0a, 0x00},
	{0x2b, 0x08, 0x00},
	{0xfe, 0x00, 0x00},
};

static struct imgsensor_i2c_reg preview_setting[] = {
};

static struct imgsensor_i2c_reg capture_setting[] = {
};

static struct imgsensor_i2c_reg video_setting[] = {
};

static struct imgsensor_i2c_reg_table dump_setting[] = {
	{0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0},
	{0x00, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0},
};

static imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0xf0,
	.sensor_id = GC5025_LCEO_SENSOR_ID,      //Sensor ID Value: 0x8044//record sensor id defined in Kd_imgsensor.h
	.checksum_value = 0xf7375923,       //checksum value for Camera Auto Test

	.pre = {
		.pclk = 288000000,              //record different mode's pclk
		.linelength  = 4800,            //record different mode's linelength
		.framelength = 2000,            //record different mode's framelength
		.startx= 0,                     //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 2592,      //record different mode's width of grabwindow
		.grabwindow_height = 1944,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 299,
		.mipi_pixel_rate = 172800000,
	},

	.cap = {
		.pclk = 288000000,              //record different mode's pclk
		.linelength  = 4800,            //record different mode's linelength
		.framelength = 2000,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 2592,      //record different mode's width of grabwindow
		.grabwindow_height = 1944,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 299,
		.mipi_pixel_rate = 172800000,
	},

	.normal_video = {
		.pclk = 288000000,              //record different mode's pclk
		.linelength  = 4800,            //record different mode's linelength
		.framelength = 2000,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 2592,      //record different mode's width of grabwindow
		.grabwindow_height = 1944,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 299,
		.mipi_pixel_rate = 172800000,
	},

	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.pre_setting = {
		.setting = preview_setting,
		.size = IMGSENSOR_ARRAY_SIZE(preview_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.cap_setting = {
		.setting = capture_setting,
		.size = IMGSENSOR_ARRAY_SIZE(capture_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.normal_video_setting = {
		.setting = video_setting,
		.size = IMGSENSOR_ARRAY_SIZE(video_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.streamon_setting = {
		.setting = stream_on,
		.size = IMGSENSOR_ARRAY_SIZE(stream_on),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.streamoff_setting = {
		.setting = stream_off,
		.size = IMGSENSOR_ARRAY_SIZE(stream_off),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.dump_info = {
		.setting = dump_setting,
		.size = IMGSENSOR_ARRAY_SIZE(dump_setting),
	},

	.margin = 2,                 //sensor framelength & shutter margin
	.min_shutter = 4,            //min shutter
	.max_frame_length = 0x2710,  //max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,    //isp gain delay frame for AE cycle
	.ihdr_support = 0,              //1, support; 0,not support
	.ihdr_le_firstline = 0,         //1,le first ; 0, se first
	.sensor_mode_num = 3,           //support sensor mode num ,don't support Slow motion

	.cap_delay_frame = 2,       //enter capture delay frame num
	.pre_delay_frame = 2,       //enter preview delay frame num
	.video_delay_frame = 2,     //enter video delay frame num
	.hs_video_delay_frame = 2,  //enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//enter slim video delay frame num

	.isp_driving_current = ISP_DRIVING_6MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R, /* sensor output first pixel color */
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,//mipi lane num
	.i2c_addr_table = {0x6C, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
	.i2c_speed = 400, // i2c read/write speed
	.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
};

enum {
	otp_page0 = 0,
	otp_page1,
};

enum {
	otp_close = 0,
	otp_open,
};

struct GC5025_LCEO_otp {
	kal_uint16 dd_param_x[DD_PARAM_QTY];
	kal_uint16 dd_param_y[DD_PARAM_QTY];
	kal_uint16 dd_param_type[DD_PARAM_QTY];
	kal_uint16 dd_cnt;
	kal_uint16 dd_flag;
	kal_uint16 reg_addr[10];
	kal_uint16 reg_value[10];
	kal_uint16 reg_num;
};

static imgsensor_t imgsensor = {
    .mirror = IMAGE_NORMAL,             //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3ED,                  //current shutter
    .gain =   0x40,                   //current gain
    .dummy_pixel = 0,                   //current dummypixel
    .dummy_line = 0,                    //current dummyline
    .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,      //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x6C,//record current sensor's i2c write id
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	/*preview*/
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
	/*capture*/
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
	/*video*/
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
};
#endif

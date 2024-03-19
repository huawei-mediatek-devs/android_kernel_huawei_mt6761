/*****************************************************************************
 *
 * Filename:
 * ---------
 *   s5k3l6_liteon_mipiraw_Sensor.h
 *
 * Project:
 * --------
 *   ALPS
 * Description:
 * ------------
 *   CMOS sensor header file.
 *
 ****************************************************************************/
#ifndef _S5K4H7_OFILM_H
#define _S5K4H7_OFILM_H

#define REG_GAIN_1X 0x20

#define SENSOR_FRM_LENGHT_LINE_REG_H  0x0340
#define SENSOR_FRM_LENGHT_LINE_REG_L  0x0341
#define SENSOR_LINE_LENGHT_PCK_REG_H  0x0342
#define SENSOR_LINE_LENGHT_PCK_REG_L  0x0343
#define SENSOR_COARSE_INT_TIME_REG_H  0x0202
#define SENSOR_COARSE_INT_TIME_REG_L  0x0203
#define SENSOR_ANA_GAIN_REG_H         0x0204
#define SENSOR_ANA_GAIN_REG_L         0x0205
#define SENSOR_FRM_CNT_REG            0x0005

static struct imgsensor_i2c_reg stream_on[] = {
	{0x0100, 0x01, 0x00},
};

static struct imgsensor_i2c_reg stream_off[] = {
	{0x0100, 0x00, 0x00},
};

static struct imgsensor_i2c_reg init_setting[] = {
	{0x0100, 0x00, 0x00},

	{0x0000, 0x12, 0x00},
	{0x0000, 0x48, 0x00},
	{0x0A02, 0x15, 0x00},
	{0x0B05, 0x01, 0x00},
	{0x3074, 0x06, 0x00},
	{0x3075, 0x2F, 0x00},
	{0x308A, 0x20, 0x00},
	{0x308B, 0x08, 0x00},
	{0x308C, 0x0B, 0x00},
	{0x3081, 0x07, 0x00},
	{0x307B, 0x85, 0x00},
	{0x307A, 0x0A, 0x00},
	{0x3079, 0x0A, 0x00},
	{0x306E, 0x71, 0x00},
	{0x306F, 0x28, 0x00},
	{0x301F, 0x20, 0x00},
	{0x306B, 0x9A, 0x00},
	{0x3091, 0x1F, 0x00},
	{0x30C4, 0x06, 0x00},
	{0x3200, 0x09, 0x00},
	{0x306A, 0x79, 0x00},
	{0x30B0, 0xFF, 0x00},
	{0x306D, 0x08, 0x00},
	{0x3080, 0x00, 0x00},
	{0x3929, 0x3F, 0x00},
	{0x3084, 0x16, 0x00},
	{0x3070, 0x0F, 0x00},
	{0x3B45, 0x01, 0x00},
	{0x30C2, 0x05, 0x00},
	{0x3069, 0x87, 0x00},
	{0x3924, 0x7F, 0x00},
	{0x3925, 0xFD, 0x00},
	{0x3C08, 0xFF, 0x00},
	{0x3C09, 0xFF, 0x00},
	{0x3C31, 0xFF, 0x00},
	{0x3C32, 0xFF, 0x00},
	{0x300A, 0x52, 0x00},
	{0x3012, 0x52, 0x00},
	{0x3013, 0x36, 0x00},
	{0x3019, 0x5F, 0x00},
	{0x301A, 0x57, 0x00},
	{0x3024, 0x10, 0x00},
	{0x3025, 0x4E, 0x00},
	{0x3026, 0x9A, 0x00},
	{0x302D, 0x0B, 0x00},
	{0x302E, 0x09, 0x00},

	{0x3931, 0x03, 0x00},
	{0x392F, 0x01, 0x00},
	{0x3930, 0x80, 0x00},
	{0x3932, 0x84, 0x00},
};

/*quater size_4:3 of 1632X1224@30.29fps 732Mbps@205.5mv*/
static struct imgsensor_i2c_reg preview_setting[] = {
	{0x0100, 0x00, 0x00},
	{0x0136, 0x18, 0x00},
	{0x0137, 0x00, 0x00},
	{0x0305, 0x06, 0x00},
	{0x0306, 0x00, 0x00},
	{0x0307, 0x88, 0x00},
	{0x030D, 0x06, 0x00},
	{0x030E, 0x00, 0x00},
	{0x030F, 0xB7, 0x00},
	{0x3C1F, 0x00, 0x00},
	{0x3C17, 0x00, 0x00},
	{0x3C1C, 0x05, 0x00},
	{0x3C1D, 0x15, 0x00},
	{0x0301, 0x04, 0x00},
	{0x0820, 0x02, 0x00},
	{0x0821, 0xDC, 0x00},
	{0x0822, 0x00, 0x00},
	{0x0823, 0x00, 0x00},
	{0x0112, 0x0A, 0x00},
	{0x0113, 0x0A, 0x00},
	{0x0114, 0x03, 0x00},
	{0x3906, 0x00, 0x00},
	{0x0344, 0x00, 0x00},
	{0x0345, 0x08, 0x00},
	{0x0346, 0x00, 0x00},
	{0x0347, 0x08, 0x00},
	{0x0348, 0x0C, 0x00},
	{0x0349, 0xC7, 0x00},
	{0x034A, 0x09, 0x00},
	{0x034B, 0x97, 0x00},
	{0x034C, 0x06, 0x00},
	{0x034D, 0x60, 0x00},
	{0x034E, 0x04, 0x00},
	{0x034F, 0xC8, 0x00},
	{0x0900, 0x01, 0x00},
	{0x0901, 0x22, 0x00},
	{0x0381, 0x01, 0x00},
	{0x0383, 0x01, 0x00},
	{0x0385, 0x01, 0x00},
	{0x0387, 0x03, 0x00},
	{0x0101, 0x00, 0x00},
	{0x0340, 0x09, 0x00},
	{0x0341, 0xCC, 0x00},
	{0x0342, 0x0D, 0x00},
	{0x0343, 0xFC, 0x00},
	{0x0200, 0x0D, 0x00},
	{0x0201, 0x6C, 0x00},
	{0x0202, 0x00, 0x00},
	{0x0203, 0x02, 0x00},
	{0x3400, 0x01, 0x00},
};
/*fullsize_4:3 of 3264X2448@30.29fps 732Mbps@205.5mv*/
static struct imgsensor_i2c_reg capture_setting[] = {
	{0x0100, 0x00, 0x00},
	{0x0136, 0x18, 0x00},
	{0x0137, 0x00, 0x00},
	{0x0305, 0x06, 0x00},
	{0x0306, 0x00, 0x00},
	{0x0307, 0x88, 0x00},
	{0x030D, 0x06, 0x00},
	{0x030E, 0x00, 0x00},
	{0x030F, 0xB7, 0x00},
	{0x3C1F, 0x00, 0x00},
	{0x3C17, 0x00, 0x00},
	{0x3C1C, 0x05, 0x00},
	{0x3C1D, 0x15, 0x00},
	{0x0301, 0x04, 0x00},
	{0x0820, 0x02, 0x00},
	{0x0821, 0xDC, 0x00},
	{0x0822, 0x00, 0x00},
	{0x0823, 0x00, 0x00},
	{0x0112, 0x0A, 0x00},
	{0x0113, 0x0A, 0x00},
	{0x0114, 0x03, 0x00},
	{0x3906, 0x00, 0x00},
	{0x0344, 0x00, 0x00},
	{0x0345, 0x08, 0x00},
	{0x0346, 0x00, 0x00},
	{0x0347, 0x08, 0x00},
	{0x0348, 0x0C, 0x00},
	{0x0349, 0xC7, 0x00},
	{0x034A, 0x09, 0x00},
	{0x034B, 0x97, 0x00},
	{0x034C, 0x0C, 0x00},
	{0x034D, 0xC0, 0x00},
	{0x034E, 0x09, 0x00},
	{0x034F, 0x90, 0x00},
	{0x0900, 0x00, 0x00},
	{0x0901, 0x00, 0x00},
	{0x0381, 0x01, 0x00},
	{0x0383, 0x01, 0x00},
	{0x0385, 0x01, 0x00},
	{0x0387, 0x01, 0x00},
	{0x0101, 0x00, 0x00},
	{0x0340, 0x09, 0x00},
	{0x0341, 0xCC, 0x00},
	{0x0342, 0x0D, 0x00},
	{0x0343, 0xFC, 0x00},
	{0x0200, 0x0D, 0x00},
	{0x0201, 0x6C, 0x00},
	{0x0202, 0x00, 0x00},
	{0x0203, 0x02, 0x00},
	{0x3400, 0x01, 0x00},
};

/*fullsize_4:3 of 3264X2448@30.29fps 732Mbps@205.5mv*/
static struct imgsensor_i2c_reg video_setting[] = {
	{0x0100, 0x00, 0x00},
	{0x0136, 0x18, 0x00},
	{0x0137, 0x00, 0x00},
	{0x0305, 0x06, 0x00},
	{0x0306, 0x00, 0x00},
	{0x0307, 0x88, 0x00},
	{0x030D, 0x06, 0x00},
	{0x030E, 0x00, 0x00},
	{0x030F, 0xB7, 0x00},
	{0x3C1F, 0x00, 0x00},
	{0x3C17, 0x00, 0x00},
	{0x3C1C, 0x05, 0x00},
	{0x3C1D, 0x15, 0x00},
	{0x0301, 0x04, 0x00},
	{0x0820, 0x02, 0x00},
	{0x0821, 0xDC, 0x00},
	{0x0822, 0x00, 0x00},
	{0x0823, 0x00, 0x00},
	{0x0112, 0x0A, 0x00},
	{0x0113, 0x0A, 0x00},
	{0x0114, 0x03, 0x00},
	{0x3906, 0x00, 0x00},
	{0x0344, 0x00, 0x00},
	{0x0345, 0x08, 0x00},
	{0x0346, 0x00, 0x00},
	{0x0347, 0x08, 0x00},
	{0x0348, 0x0C, 0x00},
	{0x0349, 0xC7, 0x00},
	{0x034A, 0x09, 0x00},
	{0x034B, 0x97, 0x00},
	{0x034C, 0x0C, 0x00},
	{0x034D, 0xC0, 0x00},
	{0x034E, 0x09, 0x00},
	{0x034F, 0x90, 0x00},
	{0x0900, 0x00, 0x00},
	{0x0901, 0x00, 0x00},
	{0x0381, 0x01, 0x00},
	{0x0383, 0x01, 0x00},
	{0x0385, 0x01, 0x00},
	{0x0387, 0x01, 0x00},
	{0x0101, 0x00, 0x00},
	{0x0340, 0x09, 0x00},
	{0x0341, 0xCC, 0x00},
	{0x0342, 0x0D, 0x00},
	{0x0343, 0xFC, 0x00},
	{0x0200, 0x0D, 0x00},
	{0x0201, 0x6C, 0x00},
	{0x0202, 0x00, 0x00},
	{0x0203, 0x02, 0x00},
	{0x3400, 0x01, 0x00},
};

static struct imgsensor_i2c_reg_table dump_setting[] = {
	{0x0100, 0x0100, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
	{0x0005, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
	{0x0340, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
	{0x0342, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
	{0x0204, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
	{0x0302, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0},
};

static imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0x0000,
	.sensor_id = S5K4H7_OFILM_SENSOR_ID,      //Sensor ID Value: 0x30C8//record sensor id defined in Kd_imgsensor.h
	.checksum_value = 0x143d0c73,       //checksum value for Camera Auto Test

	.pre = {
		.pclk = 272000000,              //record different mode's pclk
		.linelength  = 3580,            //record different mode's linelength
		.framelength = 2508,            //record different mode's framelength
		.startx= 0,                     //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 1632,      //record different mode's width of grabwindow
		.grabwindow_height = 1224,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 303,
		.mipi_pixel_rate = 292800000,
	},

	.cap = {
		.pclk = 272000000,              //record different mode's pclk
		.linelength  = 3580,            //record different mode's linelength
		.framelength = 2508,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 3264,      //record different mode's width of grabwindow
		.grabwindow_height = 2448,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 303,
		.mipi_pixel_rate = 292800000,
	},

	.normal_video = {
		.pclk = 272000000,              //record different mode's pclk
		.linelength  = 3580,            //record different mode's linelength
		.framelength = 2508,            //record different mode's framelength
		.startx = 0,                    //record different mode's startx of grabwindow
		.starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width  = 3264,      //record different mode's width of grabwindow
		.grabwindow_height = 2448,      //record different mode's height of grabwindow
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*following for GetDefaultFramerateByScenario()*/
		.max_framerate = 303,
		.mipi_pixel_rate = 292800000,
	},

	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.pre_setting = {
		.setting = preview_setting,
		.size = IMGSENSOR_ARRAY_SIZE(preview_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.cap_setting = {
		.setting = capture_setting,
		.size = IMGSENSOR_ARRAY_SIZE(capture_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.normal_video_setting = {
		.setting = video_setting,
		.size = IMGSENSOR_ARRAY_SIZE(video_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.streamon_setting = {
		.setting = stream_on,
		.size = IMGSENSOR_ARRAY_SIZE(stream_on),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.streamoff_setting = {
		.setting = stream_off,
		.size = IMGSENSOR_ARRAY_SIZE(stream_off),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.dump_info = {
		.setting = dump_setting,
		.size = IMGSENSOR_ARRAY_SIZE(dump_setting),
	},

	.margin = 4,                 //sensor framelength & shutter margin
	.min_shutter = 4,            //min shutter
	.max_frame_length = 0xFFFF,  //REG0x0202 <=REG0x0340-5//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,    //isp gain delay frame for AE cycle
	.ihdr_support = 0,              //1, support; 0,not support
	.ihdr_le_firstline = 0,         //1,le first ; 0, se first
	.sensor_mode_num = 3,           //support sensor mode num ,don't support Slow motion

	.cap_delay_frame = 3,       //enter capture delay frame num
	.pre_delay_frame = 3,       //enter preview delay frame num
	.video_delay_frame = 3,     //enter video delay frame num
	.hs_video_delay_frame = 0,  //enter high speed video  delay frame num
	.slim_video_delay_frame = 0,//enter slim video delay frame num

	.isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,//sensor output first pixel color
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
	.i2c_addr_table = {0x20,0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
	.i2c_speed = 400, // i2c read/write speed
	.addr_type = IMGSENSOR_I2C_WORD_ADDR,
};


static imgsensor_t imgsensor = {
	.mirror = IMAGE_NORMAL,             //mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x0200,                  //current shutter
	.gain =   0x0100,                   //current gain
	.dummy_pixel = 0,                   //current dummypixel
	.dummy_line = 0,                    //current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,      //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	/*preview*/
	{
		.full_w = 3264,
		.full_h = 2448,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 3264,
		.h0_size = 2448,
		.scale_w = 1632,
		.scale_h = 1224,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 1632,
		.h1_size = 1224,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 1632,
		.h2_tg_size = 1224,
	},
	/*capture*/
	{
		.full_w = 3264,
		.full_h = 2448,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 3264,
		.h0_size = 2448,
		.scale_w = 3264,
		.scale_h = 2448,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 3264,
		.h1_size = 2448,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 3264,
		.h2_tg_size = 2448,
	},
	/*video*/
	{
		.full_w = 3264,
		.full_h = 2448,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 3264,
		.h0_size = 2448,
		.scale_w = 3264,
		.scale_h = 2448,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 3264,
		.h1_size = 2448,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 3264,
		.h2_tg_size = 2448,
	},
};
#endif

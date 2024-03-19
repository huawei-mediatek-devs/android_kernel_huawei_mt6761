/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define PFX "OV8856"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
/*#include <asm/atomic.h>*/

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "ov8856mipiraw_Sensor.h"

#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)
#define SET_STREAMING_TEST 0
#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif
#define BYTE_NUM_PER_CMD  3

/* sensor default config */
#define SENSOR_SHUTTER_DEFAULT        0x4C00
#define SENSOR_GAIN_DEFAULT           0x200
#define SENSOR_I2C_WRITR_ID           0x6c
#define SENSOR_CURRENT_FPS            30
#define IMGSENSOR_LINGLENGTH_GAP      80

static struct imgsensor_info_struct imgsensor_info = {

	/*record sensor id defined in Kd_imgsensor.h*/
	.sensor_id = OV8856_SENSOR_ID,

	.checksum_value = 0xb1893b4f, /*checksum value for Camera Auto Test*/
	.pre = {
		.pclk = 144000000,	/*record different mode's pclk*/
		.linelength  = 1932,	/*record different mode's linelength*/
		.framelength = 2482,	/*record different mode's framelength*/
		.startx = 0, /*record different mode's startx of grabwindow*/
		.starty = 0,	/*record different mode's starty of grabwindow*/

		/*record different mode's width of grabwindow*/
		.grabwindow_width  = 1632,

		/*record different mode's height of grabwindow*/
		.grabwindow_height = 1224,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * nby different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 289000000,
	},
	.cap = {
		.pclk = 144000000,
		.linelength  = 1932,
		.framelength = 2482,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 289000000,
	},
	.cap1 = { /*capture for 15fps*/
		.pclk = 144000000,
		.linelength  = 1932,
		.framelength = 4964,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 289000000,
	},
	.normal_video = { /* cap*/
		.pclk = 144000000,
		.linelength  = 1932,
		.framelength = 2482,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 289000000,
	},
	.hs_video = {
		.pclk = 144000000, /*record different mode's pclk*/
		.linelength  = 1932, /*record different mode's linelength*/
		.framelength = 620, /*record different mode's framelength*/
		.startx = 0, /*record different mode's startx of grabwindow*/
		.starty = 0, /*record different mode's starty of grabwindow*/

		/*record different mode's width of grabwindow*/
		.grabwindow_width  = 640,
		/*record different mode's height of grabwindow*/
		.grabwindow_height = 480,

		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 290000000,
	},
	.slim_video = {/*pre*/
		.pclk = 144000000, /*record different mode's pclk*/
		.linelength  = 1932, /*record different mode's linelength*/
		.framelength = 2482, /*record different mode's framelength*/
		.startx = 0, /*record different mode's startx of grabwindow*/
		.starty = 0, /*record different mode's starty of grabwindow*/

		/*record different mode's width of grabwindow*/
		.grabwindow_width  = 1632,
		/*record different mode's height of grabwindow*/
		.grabwindow_height = 1224,

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 289000000,
	},
	.margin = 6,			/*sensor framelength & shutter margin*/
	.min_shutter = 6,		/*min shutter*/

	/*max framelength by sensor register's limitation*/
	.max_frame_length = 0x90f7,

	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2, /*isp gain delay frame for AE cycle*/
	.ihdr_support = 0,	  /*1, support; 0,not support*/
	.ihdr_le_firstline = 0,  /*1,le first ; 0, se first*/

	/*support sensor mode num ,don't support Slow motion*/
	.sensor_mode_num = 5,
	.cap_delay_frame = 3,		/*enter capture delay frame num*/
	.pre_delay_frame = 3,		/*enter preview delay frame num*/
	.video_delay_frame = 3,		/*enter video delay frame num*/
	.hs_video_delay_frame = 3, /*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 3,/*enter slim video delay frame num*/
	.isp_driving_current = ISP_DRIVING_6MA, /*mclk driving current*/

	/*Sensor_interface_type*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,

	/*sensor output first pixel color*/
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,

	.mclk = 24,/*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,/*mipi lane num*/

/*record sensor support all write id addr, only supprt 4must end with 0xff*/
	.i2c_addr_table = {0x6c, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,		/*mirrorflip information*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x4C00,			/*current shutter*/
	.gain = 0x200,				/*current gain*/
	.dummy_pixel = 0,			/*current dummypixel*/
	.dummy_line = 0,			/*current dummyline*/

	/*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.current_fps = 30,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,

	/*current scenario id*/
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0, /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x6c, /*record current sensor's i2c write id*/
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	/* Preview */
	{ 3296, 2480, 0, 12, 3296, 2456, 1648,
	  1228, 2, 2, 1632, 1224, 0, 0, 1632, 1224},

	/* capture */
	{ 3296, 2480, 0, 12, 3296, 2456, 3296,
	  2456, 4, 2, 3264, 2448, 0, 0, 3264, 2448},

	/* video*/
	{ 3296, 2480, 0, 12, 3296, 2456, 3296,
	  2456, 4, 2, 3264, 2448, 0, 0, 3264, 2448},

	/*hight speed video */
	{ 3296, 2480, 336, 272, 2624, 1936, 656,
	   484, 8, 2, 640, 480, 0, 0,  640,  480},

	/* slim video  */
	{ 3296, 2480, 0, 12, 3296, 2456, 1648,
	  1228, 2, 2, 1632, 1224, 0, 0, 1632, 1224}
};

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint16 ov8856_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN] = {0};
	kal_uint32 tosend = 0;
    kal_uint32 IDX  = 0;
	kal_uint16 addr = 0;
    kal_uint16 addr_last = 0;
    kal_uint16 data = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}

#if MULTI_WRITE
	if ((I2C_BUFFER_LEN - tosend) < BYTE_NUM_PER_CMD || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend,
			imgsensor.i2c_write_id, BYTE_NUM_PER_CMD, imgsensor_info.i2c_speed);
			tosend = 0;
	}
#else
		iWriteRegI2C(puSendCmd, BYTE_NUM_PER_CMD, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	kal_uint32 tmp_frame_length = imgsensor.frame_length;
	spin_lock(&imgsensor_drv_lock);
	if (imgsensor.frame_length%2 != 0)
	{
		imgsensor.frame_length =
			imgsensor.frame_length - imgsensor.frame_length%2;
		tmp_frame_length = imgsensor.frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("imgsensor.frame_length = %d\n", tmp_frame_length);
	write_cmos_sensor(SENSOR_VTS_REG_H, tmp_frame_length >> 8);
	write_cmos_sensor(SENSOR_VTS_REG_L, tmp_frame_length & 0xFF);
	write_cmos_sensor(SENSOR_HTS_REG_H, imgsensor.line_length >> 8);
	write_cmos_sensor(SENSOR_HTS_REG_L, imgsensor.line_length & 0xFF);
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length;

	pr_debug("framerate = %d, min framelength should enable?\n", framerate);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length)
	       ? frame_length : imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */


/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 tmp_frame_length = 0 ;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK
	 * to get exposure larger than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution*/
/* if shutter bigger than frame_length, should extend frame length first*/

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter)
		 ? imgsensor_info.min_shutter : shutter;

	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter =
		    (imgsensor_info.max_frame_length - imgsensor_info.margin);

	imgsensor.frame_length =
		imgsensor.frame_length - imgsensor.frame_length%2;
	tmp_frame_length = imgsensor.frame_length ;
	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / tmp_frame_length;

        /* calc fps between 297~305, real fps set to 296*/
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
        /* calc fps between 147~150, real fps set to 146*/
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length*/
		write_cmos_sensor(SENSOR_VTS_REG_H, tmp_frame_length >> 8);
		write_cmos_sensor(SENSOR_VTS_REG_L, tmp_frame_length & 0xFF);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(SENSOR_VTS_REG_H, tmp_frame_length >> 8);
		write_cmos_sensor(SENSOR_VTS_REG_L, tmp_frame_length & 0xFF);
	}

	/* Update Shutter*/
	write_cmos_sensor(SENSOR_LONG_EXPO_REG_L, (shutter << 4) & 0xFF);
	write_cmos_sensor(SENSOR_LONG_EXPO_REG_M, (shutter >> 4) & 0xFF);
	write_cmos_sensor(SENSOR_LONG_EXPO_REG_H, (shutter >> 12) & 0x0F);

	pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, tmp_frame_length);

}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 15 * BASEGAIN) {
		pr_debug("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 15 * BASEGAIN)
			gain = 15 * BASEGAIN;
	}

	reg_gain = gain*2;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(SENSOR_LONG_GAIN_REG_H, (reg_gain>>8));
	write_cmos_sensor(SENSOR_LONG_GAIN_REG_L, (reg_gain&0xFF));
	return gain;
}	/*	set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_debug("image_mirror = %d\n", image_mirror);

/********************************************************
 *
 *   0x3820[2] ISP Vertical flip
 *   0x3820[1] Sensor Vertical flip
 *
 *   0x3821[2] ISP Horizontal mirror
 *   0x3821[1] Sensor Horizontal mirror
 *
 *   ISP and Sensor flip or mirror register bit should be the same!!
 *
 ********************************************************/
/*reg: 0x3820
    bit[7]: vsub48_blc
    bit[6]: vflip_blc
    bit[5:4]: Not Used
    bit[3]: byp_isp_o
    bit[2]: vflip_diq
    bit[1]: vflip_arr
    bit[0]: hdr_en
*/
/*reg: 0x3821
    bit[7]: dig_hbin4
    bit[6]: hsync_en_o
    bit[5]: fst_vbin
    bit[4]: fst_hbin
    bit[3]: isp_hvar2
    bit[2]: 0:mirrored image, 1:Normal image
    bit[1]: 0:mirrored image, 1:Normal image
    bit[0]: dig_hbin2
*/
/*reg: 0x5001
    bit[7]: blc_vsync_sel
    bit[6]: pre_vsync_sel
    bit[5]: r_rlong_sel
    bit[4]: lenc_real_gain_rvs
    bit[3]: otp_option_en
    bit[2]: rblue_in_rvs
    bit[1]: awbm_bias_on
    bit[0]: latch_en
*/
/*reg: 0x5004
    bit[4]: r_dig_sel_from_reg
    bit[2]: blc_mirror_opt
    bit[1]: dig_gain_en
    bit[0]: lenc_decomp_en
*/
	switch (image_mirror) {
    	case IMAGE_NORMAL:
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1) & 0xB9) | 0x00));
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2) & 0xF9) | 0x06));
        	write_cmos_sensor(SENSOR_ISP_CTRL_L2E, ((read_cmos_sensor(SENSOR_ISP_CTRL_L2E) & 0xFC) | 0x03));
        	write_cmos_sensor(SENSOR_ISP_CTRL_01, ((read_cmos_sensor(SENSOR_ISP_CTRL_01) & 0xFB) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_04, ((read_cmos_sensor(SENSOR_ISP_CTRL_04) & 0xFB) | 0x04));
        	write_cmos_sensor(SENSOR_ISP_CTRL_FORMAT, 0x30);
        	break;

    	case IMAGE_H_MIRROR:
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1) & 0xB9) | 0x00));
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2) & 0xF9) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_L2E, ((read_cmos_sensor(SENSOR_ISP_CTRL_L2E) & 0xFC) | 0x03));
        	write_cmos_sensor(SENSOR_ISP_CTRL_01, ((read_cmos_sensor(SENSOR_ISP_CTRL_01) & 0xFB) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_04, ((read_cmos_sensor(SENSOR_ISP_CTRL_04) & 0xFB) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_FORMAT, 0x30);
        	break;

    	case IMAGE_V_MIRROR:
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1) & 0xB9) | 0x46));
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2) & 0xF9) | 0x06));
        	write_cmos_sensor(SENSOR_ISP_CTRL_L2E, ((read_cmos_sensor(SENSOR_ISP_CTRL_L2E) & 0xFC) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_01, ((read_cmos_sensor(SENSOR_ISP_CTRL_01) & 0xFB) | 0x04));
        	write_cmos_sensor(SENSOR_ISP_CTRL_04, ((read_cmos_sensor(SENSOR_ISP_CTRL_04) & 0xFB) | 0x04));
        	write_cmos_sensor(SENSOR_ISP_CTRL_FORMAT, 0x36);
        	break;

    	case IMAGE_HV_MIRROR:
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT1) & 0xB9) | 0x46));
        	write_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2, ((read_cmos_sensor(SENSOR_TIMEING_CTRL_FORMAT2) & 0xF9) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_L2E, ((read_cmos_sensor(SENSOR_ISP_CTRL_L2E) & 0xFC) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_01, ((read_cmos_sensor(SENSOR_ISP_CTRL_01) & 0xFB) | 0x04));
        	write_cmos_sensor(SENSOR_ISP_CTRL_04, ((read_cmos_sensor(SENSOR_ISP_CTRL_04) & 0xFB) | 0x00));
        	write_cmos_sensor(SENSOR_ISP_CTRL_FORMAT, 0x36);
        	break;

    	default:
    			pr_debug("Error image_mirror setting\n");
	}

}

static void sensor_init(void)
{
	pr_debug("v2 E\n");
	ov8856_table_write_cmos_sensor(addr_data_pair_init_ov8856,
		    sizeof(addr_data_pair_init_ov8856) / sizeof(kal_uint16));
#if SET_STREAMING_TEST
        /* stream on */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x01);
#endif
}	/*	sensor_init  */

static void preview_setting(void)
{
	pr_debug("E\n");
#if SET_STREAMING_TEST
        /* stream off */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x00);
#endif
	ov8856_table_write_cmos_sensor(addr_data_pair_preview_ov8856,
		sizeof(addr_data_pair_preview_ov8856) / sizeof(kal_uint16));

#if SET_STREAMING_TEST
        /* stream on */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x01);
#endif
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("E! currefps:%d\n", currefps);
#if SET_STREAMING_TEST
        /* stream off */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x00);
#endif
	if (currefps == 150) {
		ov8856_table_write_cmos_sensor(
			addr_data_pair_capture_15fps_ov8856,
	    sizeof(addr_data_pair_capture_15fps_ov8856) / sizeof(kal_uint16));
	} else {
		ov8856_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_ov8856,
	    sizeof(addr_data_pair_capture_30fps_ov8856) / sizeof(kal_uint16));

	}
#if SET_STREAMING_TEST
        /* stream on */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x01);
#endif
}

static void vga_setting_120fps(void)
{
#if SET_STREAMING_TEST
        /* stream off */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x00);
#endif

	ov8856_table_write_cmos_sensor(addr_data_pair_vga_setting_120fps_ov8856,
	sizeof(addr_data_pair_vga_setting_120fps_ov8856) / sizeof(kal_uint16));

#if SET_STREAMING_TEST
        /* stream on */
		write_cmos_sensor(SENSOR_STREAM_REG, 0x01);
#endif
}

static void hs_video_setting(void)
{
	pr_debug("E\n");
	vga_setting_120fps();
}

static void slim_video_setting(void)
{
	pr_debug("E\n");
	preview_setting();
}


/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2; /* retrt 2 times */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = (
		   (read_cmos_sensor(SENSOR_CHIP_ID_M) << 8) | read_cmos_sensor(SENSOR_CHIP_ID_L));

			if (*sensor_id == imgsensor_info.sensor_id) {
				if ((read_cmos_sensor(SENSOR_CHIP_ID_H)) == 0XB0) {
					ov8856version = OV8856R1A;

					pr_debug(
					"i2c write id: 0x%x, sensor id: 0x%x, ver = %d<0=r2a,1=r1a>\n",
					imgsensor.i2c_write_id,
					*sensor_id, ov8856version);

					return ERROR_NONE;
				}
			}
			pr_debug("Read sensor id fail,write_id:0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
/*const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};*/
	kal_uint8 i = 0;
	kal_uint8 retry = 2; /* retry 2 times */
	kal_uint16 sensor_id = 0;

	pr_debug("PLATFORM:Vison,MIPI 4LANE\n");
	pr_debug("read_cmos_sensor(0x302A): 0x%x\n", read_cmos_sensor(SENSOR_CHIP_ID_H));
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = (
		   (read_cmos_sensor(SENSOR_CHIP_ID_M) << 8) | read_cmos_sensor(SENSOR_CHIP_ID_L));

			if (sensor_id == imgsensor_info.sensor_id) {

				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			pr_debug(
			    "Read sensor id fail, write: 0x%x, sensor: 0x%x\n",
			    imgsensor.i2c_write_id, sensor_id);

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
	sensor_init();
	mdelay(10);
	#ifdef OV8856R1AOTP
	/*	pr_debug("Apply the sensor OTP\n");
	 *	struct otp_struct *otp_ptr =
	 * (struct otp_struct *)kzalloc(sizeof(struct otp_struct), GFP_KERNEL);
	 *	read_otp(otp_ptr);
	 *	apply_otp(otp_ptr);
	 *	kfree(otp_ptr);
	 */
	#endif
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = SENSOR_INIT_SHUTTER_DEFAULT;
	imgsensor.gain =    SENSOR_INIT_GAIN_DEFAULT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	pr_debug("E\n");

	/*No Need to implement this function*/
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/*imgsensor.video_mode = KAL_FALSE;*/
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	mdelay(10);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	/*15fps*/
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			pr_debug(
			"Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",

		imgsensor.current_fps, imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	mdelay(10);

	if (imgsensor.test_pattern == KAL_TRUE) {
		/*
		SENSOR_ISP_CTRL_00:
            bit[7]: blc_hdr_en
            bit[6]: dcblc_en
            bit[5]: lenc_en
            bit[4]: awb_gain_en
            bit[3]: r_long_short_rvs
            bit[2]: bc_en
            bit[1]: wc_en
            bit[0]: blc_en*/
		write_cmos_sensor(SENSOR_ISP_CTRL_00, (read_cmos_sensor(SENSOR_ISP_CTRL_00)&0xBF)|0x00);
	}
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	mdelay(10);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	mdelay(10);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	mdelay(10);

	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(
			MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	=
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
				  MSDK_SENSOR_INFO_STRUCT *sensor_info,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet*/
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */
	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX =
				imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX =
				imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
			break;

	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			sensor_info->SensorGrabStartX =
				imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.normal_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			  imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
			break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX =
				imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			    imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
			break;

	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX =
				imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			    imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
			break;

	default:
			sensor_info->SensorGrabStartX =
				imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;
	default:
			pr_debug("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate*/
	if (framerate == 0)
		/* Dynamic frame rate*/
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
    /* fps set to 296 when frame is 300 and auto-flicker enaled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
    /* fps set to 146 when frame is 150 and auto-flicker enaled */
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker	  */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
		    / framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.pre.framelength);
		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		  imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;

		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10;

		frame_length /= imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);

		if (frame_length
			   > imgsensor_info.normal_video.framelength)
			imgsensor.dummy_line =
			 frame_length - imgsensor_info.normal_video.framelength;

		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length = imgsensor_info.cap.pclk
		    / framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.cap.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.cap.framelength);
		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		  imgsensor_info.cap.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10;

		frame_length /= imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.hs_video.framelength)
			imgsensor.dummy_line =
		(frame_length - imgsensor_info.hs_video.framelength);

		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
	    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10;

		frame_length /= imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);

		if (frame_length > imgsensor_info.slim_video.framelength)
			imgsensor.dummy_line =
		(frame_length - imgsensor_info.slim_video.framelength);

		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		   imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		break;

	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk
		    / framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.pre.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		  imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		pr_debug(
		"error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		    enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);
    /*SENSOR_ISP_CTRL_00
        bit[7]: blc_vsync_sel
        bit[6]: pre_vsync_sel
        bit[5]: r_rlong_sel
        bit[4]: lenc_real_gain_rvs
        bit[3]: otp_option_en
        bit[2]: rblue_in_rvs
        bit[1]: awbm_bias_on
        bit[0]: latch_en
    */
    /*
    SENSOR_ISP_CTRL_00:
        bit[7]: blc_hdr_en
        bit[6]: dcblc_en
        bit[5]: lenc_en
        bit[4]: awb_gain_en
        bit[3]: r_long_short_rvs
        bit[2]: bc_en
        bit[1]: wc_en
        bit[0]: blc_en*/

    /*
    SENSOR_PRE_CTRL_00:
        bit[4]: Square mode 0: color square, 1: Black-white square
        bit[3:2]: 
        color bar type:
        00: standard colot bar
        01: Top-bottom darker color bar
        10: Right-left darker color bar
        11: bottom-top darker color bar
        bit[1:0]: 
        Test pattern mode
        00: color bar
        01: random data
        10: square
        11: black image
    */
	if (enable) {
		write_cmos_sensor(SENSOR_ISP_CTRL_00, 0x57);
		write_cmos_sensor(SENSOR_ISP_CTRL_01, 0x02);
		write_cmos_sensor(SENSOR_PRE_CTRL_00, 0x80);
	} else {
		write_cmos_sensor(SENSOR_ISP_CTRL_00, 0x77);
		write_cmos_sensor(SENSOR_ISP_CTRL_01, 0x0a);
		write_cmos_sensor(SENSOR_PRE_CTRL_00, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(SENSOR_STREAM_REG, 0X01);
	else
		write_cmos_sensor(SENSOR_STREAM_REG, 0x00);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = 0;
	UINT32 fps = 0;

	pr_debug("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4; /* return 4 byte */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;

	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(
			(BOOL)*feature_data_16, *(feature_data_16+1));
		break;

	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
		  (enum MSDK_SCENARIO_ID_ENUM)*feature_data, (UINT32)*(feature_data+1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(UINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;

	/*for factory mode auto testing*/
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            /*imgsensor_winsize_info arry 1 is capture setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            /*imgsensor_winsize_info arry 2 is video setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
            /*imgsensor_winsize_info arry 0 is preview setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);

		if (*feature_data != 0)
			set_shutter((UINT16)*feature_data);

		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if(imgsensor_info.cap.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.cap.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(imgsensor_info.normal_video.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.normal_video.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			if(imgsensor_info.pre.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.pre.grabwindow_width;
			}
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (UINT32)(*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (fps == 150)
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap1.mipi_pixel_rate;
			else
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}

		break;
	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV8856_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	OV8856_MIPI_RAW_SensorInit	*/

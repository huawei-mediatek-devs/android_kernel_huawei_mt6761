

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "hi556_holi_sensor.h"

#define DEBUG_HI556_HOLI 0

#define PFX "HI556_HOLI"
#define LOG_DBG(fmt, args...) \
	do { \
		if (DEBUG_HI556_HOLI) \
			pr_err(PFX "[%s] " fmt, __func__, ##args); \
	} while (0)
#define LOG_INF(fmt, args...)   pr_info(PFX "%s %d " fmt, __func__,__LINE__, ##args)
#define LOG_ERR(fmt, args...)   pr_err(PFX "[%s] " fmt, __func__, ##args)

#define HI556_MAXGAIN 16
#define RETRY_TIMES 2

static DEFINE_SPINLOCK(imgsensor_drv_lock);
#ifdef CONFIG_HUAWEI_CAMERA_DSM
extern int camerasensor_report_dsm_err(int type, int err_value, int add_value);
#endif

static void set_dummy(void)
{
	kal_int32 rc = ERROR_NONE;

	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_REG_H,
					imgsensor.frame_length, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0)
		LOG_ERR("write frame_length failed, frame_length:0x%x.\n", imgsensor.frame_length);
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_LINE_LENGTH_PCK_REG_H,
					imgsensor.line_length, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0)
		LOG_ERR("write line_length failed, line_length:0x%x.\n", imgsensor.frame_length);
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc = 0;
	kal_int32 sensor_id_rt = 0;
	kal_int32 sensor_id_l = 0;
	kal_int32 sensor_id_h = 0;
	kal_uint16 sensor_id = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
				       &sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}

	sensor_id_h = sensor_id & 0xffff;
	sensor_id_l = HOLI_OTP_OFFSET;
	sensor_id_rt = (sensor_id_h << 12 | sensor_id_l);
	return sensor_id_rt;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	if (!framerate || !imgsensor.line_length) {
		LOG_ERR("Invalid params. framerate=%d, line_length=%d.\n",
			framerate, imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
				 frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	LOG_DBG("ENTER\n");
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		  (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		/* calc fps between 297~305, real fps set to 298 */
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(298, 0);
		/* calc fps between 147~150, real fps set to 146 */
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_REG_H,
							 imgsensor.frame_length, IMGSENSOR_I2C_WORD_DATA);
	} else {/* Extend frame length */
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
		else
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_REG_H,
							 imgsensor.frame_length, IMGSENSOR_I2C_WORD_DATA);
	}

	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_INTEG_TIME_REG_H,
					 shutter, IMGSENSOR_I2C_WORD_DATA);
}

/*
 * FUNCTION : set_shutter
 * DESCRIPTION : This function set e-shutter of sensor to change exposure time.
 * PARAMETERS : shutter(exposured lines)
 * RETURNS : None
 */
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;
}
/*
 * FUNCTION : set_gain
 * DESCRIPTION : This function is to set global gain to sensor.
 * PARAMETERS : gain(sensor global gain base: 0x40)
 * RETURNS : the actually gain set to sensor.
 */
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X */
	/* [4:9] = M meams M X */
	/* Total gain = M + N /16 X */

	LOG_DBG("gain [%d]", gain);
	if (gain < BASEGAIN || gain > HI556_MAXGAIN * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > HI556_MAXGAIN * BASEGAIN)
			gain = HI556_MAXGAIN * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);

	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_ANA_GAIN_REG,
					 (reg_gain & 0xFFFF), IMGSENSOR_I2C_WORD_DATA);
	return gain;
}

static kal_uint32 sensor_dump_reg(void)
{
	kal_int32 rc = 0;

	LOG_DBG("ENTER\n");
	rc = imgsensor_sensor_i2c_process(&imgsensor, &imgsensor_info.dump_info);
	if (rc < 0)
		LOG_ERR("Failed.\n");
	LOG_DBG("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc = 0;

	LOG_DBG("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.init_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc = 0;

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.pre_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(void)
{
	kal_int32 rc = 0;

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.cap_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(void)
{
	kal_int32 rc = 0;

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.normal_video_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES;

	spin_lock(&imgsensor_drv_lock);
	/* init i2c config */
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("id reg: 0x%x, read id: 0x%x, expect id:0x%x.\n",
					imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Check sensor id fail, id reg: 0x%x,read id: 0x%x, expect id:0x%x.\n",
				imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	int rc = 0;
	UINT32 sensor_id = 0;

	LOG_INF("E\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	LOG_DBG("sensor probe successfully. sensor_id=0x%x.\n", sensor_id);
	rc = sensor_init();
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
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
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("E");
	/* No Need to implement this function */
	return ERROR_NONE;
}

/*
 * FUNCTION : preview
 * DESCRIPTION : This function start the sensor preview.
 * PARAMETERS : image_window ; address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data ; address pointer of line numbers in one period of VSYNC
 * RETURNS : kal_uint32 status
 */
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_preview_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("%s setting failed.\n", __func__);
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

/*
 * FUNCTION : capture
 * DESCRIPTION : This function setup the CMOS sensor in capture MY_OUTPUT mode
 * PARAMETERS : image_window ; address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data ; address pointer of line numbers in one period of VSYNC
 * RETURNS : kal_uint32 status
 */
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_capture_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("%s setting failed.\n", __func__);
		return rc;
	}
	LOG_INF("EXIT\n");
	return ERROR_NONE;

}

/*
 * FUNCTION : normal_video
 * DESCRIPTION : This function setup the CMOS sensor in normal_video MY_OUTPUT mode
 * PARAMETERS : image_window ; address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data ; address pointer of line numbers in one period of VSYNC
 * RETURNS : kal_uint32 status
 */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = imgsensor_info.normal_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_normal_video_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("%s video setting failed.\n", __func__);
		return rc;
	}
	LOG_INF("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	if (sensor_resolution != NULL) {
		sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
		sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

		sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
		sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

		sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
		sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	}
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (sensor_info != NULL && sensor_config_data != NULL) {
		sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
		sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
		sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
		sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
		sensor_info->SensorInterruptDelayLines = 4; /* not use */
		sensor_info->SensorResetActiveHigh = FALSE; /* not use */
		sensor_info->SensorResetDelayCount = 5; /* not use */

		sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
		sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
		sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
		sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

		sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
		sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
		sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;

		sensor_info->SensorMasterClockSwitch = 0; /* not use */
		sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
		/* The frame of setting shutter default 0 for TG int */
		sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
		/* The frame of setting sensor gain */
		sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
		sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
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
		sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
		sensor_info->SensorHightSampling = 0;  // 0 is default 1x
		sensor_info->SensorPacketECCOrder = 1;

		switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
			break;
		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
		}
	}
	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_DBG("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		rc = preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		rc = capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		rc = normal_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	if (framerate == 0)
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 when frame is 300 and auto-flicker enaled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 298;
	/* fps set to 146 when frame is 150 and auto-flicker enaled */
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length = 0;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if (framerate == 0 || imgsensor_info.pre.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
				       (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0 || imgsensor_info.normal_video.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			       imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
				       (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (framerate == 0 || imgsensor_info.cap.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
				       (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  // coding with  preview scenario by default
		if (framerate == 0 || imgsensor_info.pre.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
				       (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MUINT32 *framerate)
{
	if (framerate != NULL) {
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
		default:
			break;
		}
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	if (enable) {
		LOG_INF("enter color bar");
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_ISP_EN_REG_H,
						 0x0143, IMGSENSOR_I2C_WORD_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_TP_MODE_REG,
						 0x0002, IMGSENSOR_I2C_WORD_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_ISP_EN_REG_H,
						 0x0142, IMGSENSOR_I2C_WORD_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_TP_MODE_REG,
						 0x0000, IMGSENSOR_I2C_WORD_DATA);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc = 0;

	LOG_INF("Enter.enable:%d\n", enable);
	if (enable)
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamon_setting);
	else
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamoff_setting);

	if (rc < 0) {
		LOG_ERR("Failed enable:%d.\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control_hi556_holi(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	if (feature_para != NULL && feature_para_len != NULL) {
		UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
		UINT16 *feature_data_16 = (UINT16 *) feature_para;
		UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
		UINT32 *feature_data_32 = (UINT32 *) feature_para;
		unsigned long long *feature_data = (unsigned long long *) feature_para;
		struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;
		MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

		LOG_DBG("feature_id = %d\n", feature_id);
		switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter((UINT16)*feature_data);
			break;
		case SENSOR_FEATURE_SET_GAIN:
			set_gain((UINT16)*feature_data);
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			(void)imgsensor_sensor_i2c_write(&imgsensor, sensor_reg_data->RegAddr,
							 sensor_reg_data->RegData, IMGSENSOR_I2C_WORD_DATA);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			(void)imgsensor_sensor_i2c_read(&imgsensor, sensor_reg_data->RegAddr,
							(kal_uint16 *)&sensor_reg_data->RegData, IMGSENSOR_I2C_WORD_DATA);
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode((UINT16)*feature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *feature_data,
						      (MUINT32) *(feature_data + 1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
							  (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: // for factory mode auto testing
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = (UINT16) *feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				/* imgsensor_winsize_info arry 1 is capture setting */
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				/* imgsensor_winsize_info arry 2 is video setting */
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
				       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				// fall through
			default:
				/* imgsensor_winsize_info arry 0 is preview setting */
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
				       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			}
			break;
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			(void)streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			if (*feature_data != 0)
				set_shutter((UINT16)*feature_data);

			(void)streaming_control(KAL_TRUE);
			break;
		case SENSOR_HUAWEI_FEATURE_DUMP_REG:
			sensor_dump_reg();
			break;
		case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				// fall through
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			break;
		case SENSOR_FEATURE_GET_PIXEL_RATE:
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				if (imgsensor_info.cap.linelength > IMGSENSOR_LINGLENGTH_GAP) {
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
						(imgsensor_info.cap.pclk /
						 (imgsensor_info.cap.linelength - IMGSENSOR_LINGLENGTH_GAP)) *
						imgsensor_info.cap.grabwindow_width;
				}
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				if (imgsensor_info.normal_video.linelength > IMGSENSOR_LINGLENGTH_GAP) {
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
						(imgsensor_info.normal_video.pclk /
						 (imgsensor_info.normal_video.linelength - IMGSENSOR_LINGLENGTH_GAP)) *
						imgsensor_info.normal_video.grabwindow_width;
				}
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				// fall through
			default:
				if (imgsensor_info.pre.linelength > IMGSENSOR_LINGLENGTH_GAP) {
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
						(imgsensor_info.pre.pclk /
						 (imgsensor_info.pre.linelength - IMGSENSOR_LINGLENGTH_GAP)) *
						imgsensor_info.pre.grabwindow_width;
				}
				break;
			}
			break;
		default:
			break;
		}
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_hi556_holi,
	control,
	close
};

UINT32 HI556_HOLI_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;

	return ERROR_NONE;
}

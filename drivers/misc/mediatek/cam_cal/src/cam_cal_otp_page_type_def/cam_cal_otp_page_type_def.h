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

#ifndef __CAM_CAL_OTP_PAGE_TYPE_DEF_H
#define __CAM_CAL_OTP_PAGE_TYPE_DEF_H

#define OTP_MAX_BLOCK_NUM  10
#define OTP_MAX_PAGE_NUM  10

#if defined(HI556_LITEARY) || defined(HI556_HOLI)
static struct imgsensor_i2c_reg hi556_sensor_init_setting[] ={
	{0x0e00, 0x0102, 0x00}, //tg_pmem_sckpw/sdly
	{0x0e02, 0x0102, 0x00}, //tg_dmem_sckpw/sdly
	{0x0e0c, 0x0100, 0x00}, //tg_pmem_rom_dly
	{0x27fe, 0xe000, 0x00}, // firmware start address-ROM
	{0x0b0e, 0x8600, 0x00}, // BGR enable
	{0x0d04, 0x0100, 0x00}, // STRB(OTP Busy) output enable
	{0x0d02, 0x0707, 0x00}, // STRB(OTP Busy) output drivability
	{0x0f30, 0x6e25, 0x00}, // Analog PLL setting
	{0x0f32, 0x7067, 0x00}, // Analog CLKGEN setting
	{0x0f02, 0x0106, 0x00}, // PLL enable
	{0x0a04, 0x0000, 0x00}, // mipi disable
	{0x0e0a, 0x0001, 0x00}, // TG PMEM CEN anable
	{0x004a, 0x0100, 0x00}, // TG MCU enable
	{0x003e, 0x1000, 0x00}, // ROM OTP Continuous W/R mode enable
	{0x0a00, 0x0100, 0x00}, // Stream On
};

static struct imgsensor_i2c_reg hi556_otp_init_setting[] ={
	{0x0A02, 0x01, 0x00},
	{0x0A00, 0x00, 0x10},
	{0x0F02, 0x00, 0x00},
	{0x011A, 0x01, 0x00},
	{0x011B, 0x09, 0x00},
	{0x0D04, 0x01, 0x00},
	{0x0D00, 0x07, 0x00},
	{0x003E, 0x10, 0x00},
	{0x0a00, 0x01, 0x00}
};

static struct imgsensor_i2c_reg hi556_deinit_setting[] ={
	{0x0A00, 0x0000, 0x10},
	{0x003E, 0x0000, 0x00},
	{0x0a00, 0x0001, 0x00}
};
#endif

typedef struct
{
	unsigned int AddrOffset;
	unsigned int datalen;
} SENSOR_OTP_BLOCK_INFO_STRUCT;

typedef struct
{
	unsigned int page_status_addr;
	unsigned int page_status_data_size;
	unsigned int page_status_data_normal;
} CAM_CAL_OTP_PAGE_STATUS_T;

typedef struct
{
	unsigned int  otp_addr_config_h;
	unsigned int  otp_addr_config_l;
	unsigned int  otp_read_mode_addr;
	unsigned int  otp_read_mode;
	unsigned int  otp_read_addr;
} cam_cal_otp_read_config_t;

typedef struct
{
	CAM_CAL_OTP_PAGE_STATUS_T page_check_status;
	unsigned int blocknum;
	SENSOR_OTP_BLOCK_INFO_STRUCT otp_block_info[OTP_MAX_BLOCK_NUM];
} SENSOR_OTP_PAGE_INFO_STRUCT;

typedef struct stCAM_CAL_OTP_PAGE_TYPE_STRUCT {
	unsigned int sensorID;
    imgsensor_t  imgsensor;
	cam_cal_otp_read_config_t  otp_read_config;
	imgsensor_i2c_reg_setting_t sensor_init_setting;
	imgsensor_i2c_reg_setting_t otp_init_setting;
	imgsensor_i2c_reg_setting_t deinit_setting;
	SENSOR_OTP_PAGE_INFO_STRUCT otp_page_info[OTP_MAX_PAGE_NUM];
	unsigned int otp_page_num;
	unsigned int total_otp_size;
}stCAM_CAL_OTP_PAGE_TYPE_STRUCT_ST;

static stCAM_CAL_OTP_PAGE_TYPE_STRUCT_ST g_camCalotpmapInfo[] =
{
#if defined(HI556_LITEARY)
    {
        .sensorID      = 0x0556027,
        .imgsensor     =
        {
            .i2c_write_id = 0x40,
            .i2c_speed    = 400,
            .addr_type    = IMGSENSOR_I2C_WORD_ADDR,
        },
        .otp_read_config     =
        {
            .otp_addr_config_h  = 0x010a,
            .otp_addr_config_l  = 0x010b,
            .otp_read_mode_addr = 0x0102,
            .otp_read_mode      = 0x01,
            .otp_read_addr      = 0x0108,
        },
        .sensor_init_setting     =
        {
            .setting   = hi556_sensor_init_setting,
            .size      = ARRAY_SIZE(hi556_sensor_init_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_WORD_DATA,
            .delay     = 0,
        },
        .otp_init_setting     =
        {
            .setting   = hi556_otp_init_setting,
            .size      = ARRAY_SIZE(hi556_otp_init_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_BYTE_DATA,
            .delay     = 0,
        },
        .deinit_setting     =
        {
            .setting   = hi556_deinit_setting,
            .size      = ARRAY_SIZE(hi556_deinit_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_BYTE_DATA,
            .delay     = 0,
        },
        .otp_page_info[0] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x0401,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x01,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x0401,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_info[1] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x0BA1,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x13,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x0BA1,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_info[2] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x1341,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x37,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x1341,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_num         = 3,
        .total_otp_size       = 1952,
    },
#endif
#if defined(HI556_HOLI)
    {
        .sensorID      = 0x0556050,
        .imgsensor     =
        {
            .i2c_write_id = 0x50,
            .i2c_speed    = 400,
            .addr_type    = IMGSENSOR_I2C_WORD_ADDR,
        },
        .otp_read_config     =
        {
            .otp_addr_config_h  = 0x010a,
            .otp_addr_config_l  = 0x010b,
            .otp_read_mode_addr = 0x0102,
            .otp_read_mode      = 0x01,
            .otp_read_addr      = 0x0108,
        },
        .sensor_init_setting     =
        {
            .setting   = hi556_sensor_init_setting,
            .size      = ARRAY_SIZE(hi556_sensor_init_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_WORD_DATA,
            .delay     = 0,
        },
        .otp_init_setting     =
        {
            .setting   = hi556_otp_init_setting,
            .size      = ARRAY_SIZE(hi556_otp_init_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_BYTE_DATA,
            .delay     = 0,
        },
        .deinit_setting     =
        {
            .setting   = hi556_deinit_setting,
            .size      = ARRAY_SIZE(hi556_deinit_setting),
            .addr_type = IMGSENSOR_I2C_WORD_ADDR,
            .data_type = IMGSENSOR_I2C_BYTE_DATA,
            .delay     = 0,
        },
        .otp_page_info[0] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x0401,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x01,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x0401,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_info[1] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x0BA1,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x13,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x0BA1,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_info[2] =
        {
            .page_check_status =
            {
                .page_status_addr        = 0x1341,
                .page_status_data_size   = 1,
                .page_status_data_normal = 0x37,
            },
            .otp_block_info[0] =
            {
                .AddrOffset   = 0x1341,
                .datalen      = 1952,
            },
            .blocknum = 1,
        },
        .otp_page_num         = 3,
        .total_otp_size       = 1952,
    },
#endif

};

#endif

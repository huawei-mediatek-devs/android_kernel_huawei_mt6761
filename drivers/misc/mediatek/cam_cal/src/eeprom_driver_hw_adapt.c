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

#define PFX "eeprom_adapt"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include "kd_imgsensor.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include "eeprom_driver_hw_adapt.h"
/*#include <asm/system.h>  // for SM*/
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

static DEFINE_MUTEX(sensor_eeprom_mutex);

/*****************************************************************************
 Func Name     : eeprom_read_region
 Func Describe : read otp data via I2C
 input para    : current_Client, data_addr, data_size
 output para   : output_data
 return value  : error:0 , other:OK
*****************************************************************************/
unsigned int eeprom_read_region(struct i2c_client *current_Client,
		unsigned int Data_Addr, unsigned int readlen, unsigned char *data)
{
	unsigned int readsize = 0;
	unsigned int ReadDataLength = readlen;
	unsigned int dataOffset  = 0;
	unsigned int AddrOffset  = Data_Addr;
	unsigned char *pReadBuff = data;
	unsigned int datasize    = 0;
	unsigned char i          = 0;

	if (!current_Client || !data){
		pr_err("eeprom_read_region ptr is null!!\n");
		return 0;
	}

	/* read 8 byte every time, and retry 3 times when read fail */
	do {
		datasize = (ReadDataLength > EEPROM_READ_DATA_SIZE_8) ? EEPROM_READ_DATA_SIZE_8 : ReadDataLength;

		readsize = Common_read_region(current_Client, AddrOffset, pReadBuff, datasize);
		if (readsize != datasize) {
			for (i = 0; i < EEPROM_READ_FAIL_RETRY_TIMES; i++)
			{
				pr_err("I2C ReadData failed [0x%x], retry [%d]!!\n", AddrOffset, i);
				readsize = Common_read_region(current_Client, AddrOffset, pReadBuff, datasize);
				if (readsize == datasize) {
					break;
				}
			}
		}

		if (readsize != datasize){
			pr_err("I2C ReadData failed readsize [%d], datasize [%d]!!\n", readsize, datasize);
			return 0;
		}

		dataOffset     += datasize;
		ReadDataLength -= datasize;
		AddrOffset      = Data_Addr + dataOffset;
		pReadBuff       = data + dataOffset;
	} while (ReadDataLength > 0);

	return readlen;
}

/*****************************************************************************
 Func Name     : camcal_otp_set_sensor_setting
 Func Describe : set setting to sensor
 input para    : pstimgsensor, pst_reg_setting
 output para   : null
 return value  : error: -1 , other: OK
*****************************************************************************/
int camcal_otp_set_sensor_setting(imgsensor_t *pstimgsensor, imgsensor_i2c_reg_setting_t *pst_reg_setting)
{
	int rc = 0;

	if (!pstimgsensor || !pst_reg_setting){
		pr_err("pstimgsensor or pst_reg_setting is null!!\n");
		return -1;
	}

	if (0 != pst_reg_setting->setting){
		rc = imgsensor_sensor_write_setting(pstimgsensor, pst_reg_setting);
		if( rc < 0 ){
			pr_err("write sensor Failed.\n");
			return -1;
		}
	}

	return 0;
}
/*****************************************************************************
 Func Name     : camcal_read_hynix_otp_page_data
 Func Describe : read otp data from sensor
 input para    : current_Client, otp_map_index, addr, datasize
 output para   : databuff
 return value  : error: 0 , other: OK
*****************************************************************************/
unsigned int camcal_read_hynix_otp_page_data(struct i2c_client *current_Client, unsigned int otp_map_index, unsigned int addr, unsigned int datasize, unsigned char *databuff)
{
	unsigned int otp_addr_h = 0;
	unsigned int otp_addr_l = 0;
	unsigned int otp_read_mode_addr = 0;
	unsigned int otp_read_mode = 0;
	unsigned int otp_read_addr = 0;
	unsigned int returnsize = 0;
	unsigned char *otpbuff = 0;
	imgsensor_t *pstimgsensor = 0;
	unsigned int i = 0;

	if ((!current_Client) || (!databuff)){
		pr_err("current_Client or databuff is null!\n");
		return 0;
	}

	if (otp_map_index >= ARRAY_SIZE(g_camCalotpmapInfo)){
		pr_err("otp_map_index [0x%x] is error!\n", otp_map_index);
		return 0;
	}

	otp_addr_h = g_camCalotpmapInfo[otp_map_index].otp_read_config.otp_addr_config_h;
	otp_addr_l = g_camCalotpmapInfo[otp_map_index].otp_read_config.otp_addr_config_l;
	otp_read_mode_addr = g_camCalotpmapInfo[otp_map_index].otp_read_config.otp_read_mode_addr;
	otp_read_mode = g_camCalotpmapInfo[otp_map_index].otp_read_config.otp_read_mode;
	otp_read_addr = g_camCalotpmapInfo[otp_map_index].otp_read_config.otp_read_addr;

	pstimgsensor = &g_camCalotpmapInfo[otp_map_index].imgsensor;
	(void)imgsensor_sensor_i2c_write(pstimgsensor, otp_addr_h, (addr >> 8) & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(pstimgsensor, otp_addr_l, addr & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(pstimgsensor, otp_read_mode_addr, otp_read_mode, IMGSENSOR_I2C_BYTE_DATA);

	otpbuff = databuff;
	for (i = 0; i < datasize; i++)
	{
        /* read 1 byte every time */
		returnsize = Common_read_region(current_Client, otp_read_addr, otpbuff, 1);
		if (0 == returnsize){
			pr_err("read add [0x%x] fail!\n" ,i);
			return 0;
		}
		otpbuff++;
	}


	return datasize;
}

/*****************************************************************************
 Func Name     : camcal_check_page_status
 Func Describe : check page status
 input para    : current_Client, otp_map_index, page_index
 output para   : null
 return value  : error: -1, other: OK
*****************************************************************************/
int camcal_check_page_status(struct i2c_client *current_Client, unsigned int otp_map_index, unsigned int page_index)
{
	unsigned int page_status_addr = 0;
	unsigned int page_status_data_szie = 0;
	unsigned int page_status_data_normal = 0;
	unsigned char page_status_data_return = 0;
	unsigned int  returnsize = 0;

	if (!current_Client){
		pr_err("current_Client is null!\n");
		return -1;
	}

	if ((otp_map_index >= ARRAY_SIZE(g_camCalotpmapInfo))
			|| (page_index >= OTP_MAX_PAGE_NUM)){
		pr_err("otp_map_index [0x%x], page_index [0x%x] is error!\n", otp_map_index, page_index);
		return -1;
	}

	page_status_addr = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].page_check_status.page_status_addr;
	page_status_data_szie = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].page_check_status.page_status_data_size;
	page_status_data_normal = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].page_check_status.page_status_data_normal;

	returnsize = camcal_read_hynix_otp_page_data(current_Client, otp_map_index, page_status_addr, page_status_data_szie, &page_status_data_return);
	if (0 == returnsize){
		pr_err(" camcal_read_otp_page_data fail!\n");
		return -1;
	}

	if (page_status_data_return != page_status_data_normal){
		pr_err("page invalid return data [0x%x], expect data [0x%x] !\n", page_status_data_return, page_status_data_normal);
		return -1;
	}

	return 0;
}
/*****************************************************************************
 Func Name     : camcal_read_otp_page_func
 Func Describe : otp page type map read func
 input para    : otp_map_index
 output para   : null
 return value  : error: -1 , other: OK
*****************************************************************************/
int camcal_read_otp_page_func(struct i2c_client *current_Client, unsigned int sensorID, unsigned char **peeprombuff, unsigned int *eepromsize)
{
	unsigned int otp_page_type_map_size = 0;
	unsigned int total_otp_size = 0;
	unsigned int blocknum = 0;
	unsigned int blockaddr = 0;
	unsigned int blocksize = 0;
	unsigned int blocktotalsize = 0;
	unsigned int block_index = 0;
	unsigned int returnsize = 0;
	unsigned char *potpbuff = 0;
	unsigned char *potpbuff_tmp = 0;
	int rc = 0;
	unsigned int otp_map_index = 0;
	unsigned char page_index = 0;
	unsigned char page_num = 0;

	if ((!current_Client) || (!peeprombuff) || (!eepromsize)){
		pr_err("current_Client or eeprombuff or eepromsize is null!\n");
		return -1;
	}

	otp_page_type_map_size = ARRAY_SIZE(g_camCalotpmapInfo);
	for (otp_map_index = 0; otp_map_index < otp_page_type_map_size; otp_map_index++){
		if (sensorID == g_camCalotpmapInfo[otp_map_index].sensorID){
			break;
		}
	}

	if (otp_map_index >= otp_page_type_map_size){
		pr_err("can't find sensor [0x%x]!\n", sensorID);
		return -1;
	}

	total_otp_size = g_camCalotpmapInfo[otp_map_index].total_otp_size;
	if (total_otp_size == 0) {
		pr_err("total_otp_size is 0!\n");
		return -1;
	}

	potpbuff =  kmalloc(total_otp_size, GFP_KERNEL);
	if (NULL == potpbuff){
		pr_err("malloc fail [%d]!\n", total_otp_size);
		return -1;
	}

	rc = camcal_otp_set_sensor_setting(&g_camCalotpmapInfo[otp_map_index].imgsensor,
					&g_camCalotpmapInfo[otp_map_index].sensor_init_setting);
	if (rc < 0){
		pr_err("set sensor init setting error [%d]!\n", otp_map_index);
		kfree(potpbuff);
		return -1;
	}

	rc = camcal_otp_set_sensor_setting(&g_camCalotpmapInfo[otp_map_index].imgsensor,
					&g_camCalotpmapInfo[otp_map_index].otp_init_setting);
	if (rc < 0){
		pr_err("set otp init setting error [%d]!\n", otp_map_index);
		kfree(potpbuff);
		return -1;
	}

	potpbuff_tmp = potpbuff;
	page_num = g_camCalotpmapInfo[otp_map_index].otp_page_num;
	for (page_index = 0; page_index < page_num; page_index++){
		rc = camcal_check_page_status(current_Client, otp_map_index, page_index);
		if (rc < 0){
			pr_err("page_index [%d] check error!\n", page_index);
			continue;
		}

		blocknum = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].blocknum;
		for (block_index = 0; block_index < blocknum; block_index++){
			blockaddr = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].otp_block_info[block_index].AddrOffset;
			blocksize = g_camCalotpmapInfo[otp_map_index].otp_page_info[page_index].otp_block_info[block_index].datalen;
			if ((blocktotalsize + blocksize) > total_otp_size){
				pr_err("blocktotalsize [%d], blocksize [%d], total_otp_size [%d]!\n", blocktotalsize, blocksize, total_otp_size);
				kfree(potpbuff);
				return -1;
			}
			returnsize = camcal_read_hynix_otp_page_data(current_Client, otp_map_index, blockaddr, blocksize, potpbuff_tmp);
			if (returnsize != blocksize){
				pr_err("returnsize [%d], blocksize [%d]!\n", returnsize, blocksize);
				kfree(potpbuff);
				return -1;
			}
			potpbuff_tmp = potpbuff_tmp + blocksize;
			blocktotalsize = blocktotalsize + blocksize;
		}
		if (blocktotalsize == total_otp_size){
			break;
		}
	}

	if (blocktotalsize != total_otp_size){
		pr_err("blocktotalsize [%d] != total_otp_size [%d]!\n", blocktotalsize, total_otp_size);
		kfree(potpbuff);
		return -1;
	}

	rc = camcal_otp_set_sensor_setting(&g_camCalotpmapInfo[otp_map_index].imgsensor,
		&g_camCalotpmapInfo[otp_map_index].deinit_setting);
	if (rc < 0){
		pr_err("set otp init setting error [%d]!\n", otp_map_index);
		kfree(potpbuff);
		return -1;
	}

	*peeprombuff = potpbuff;
	*eepromsize  = blocktotalsize;

	return blocktotalsize;
}

/*****************************************************************************
 Func Name     : camcal_read_eeprom_func
 Func Describe : eeprom type map read func
 input para    : current_Client, sensorID
 output para   : eeprombuff, eepromsize
 return value  : error: -1 , other: OK
*****************************************************************************/
int camcal_read_eeprom_func(struct i2c_client *current_Client, unsigned int sensorID, unsigned char **peeprombuff, unsigned int *eepromsize)
{
	unsigned int eeprom_type_map_size = 0;
	unsigned int eeprom_readsize = 0;
	unsigned int blocknum = 0;
	unsigned int blockaddr = 0;
	unsigned int blocksize = 0;
	unsigned int blocktotalsize = 0;
	unsigned int returnsize = 0;
	unsigned char * peeprom_readbuff = 0;
	unsigned char * peeprom_readbuff_tmp = 0;
	int eeprom_map_index = 0;
	int block_index = 0;

	if ((!current_Client) || (!peeprombuff) || (!eepromsize)){
		pr_err("current_Client or eeprombuff is null!\n");
		return -1;
	}

	eeprom_type_map_size = ARRAY_SIZE(g_camCalE2promMapInfo);
	for (eeprom_map_index = 0; eeprom_map_index < eeprom_type_map_size; eeprom_map_index++){
		if (sensorID == g_camCalE2promMapInfo[eeprom_map_index].sensorID){
			break;
		}
	}

	if (eeprom_map_index >= eeprom_type_map_size){
		pr_err("can't find sensor [0x%x]!\n", sensorID);
		return -1;
	}

	eeprom_readsize = g_camCalE2promMapInfo[eeprom_map_index].e2prom_total_size;
	if (eeprom_readsize == 0) {
		pr_err("eeprom_readsize is 0!\n");
		return -1;
	}

	peeprom_readbuff = kmalloc(eeprom_readsize, GFP_KERNEL);
	if (NULL == peeprom_readbuff){
		pr_err("malloc fail [%d]!\n", eeprom_readsize);
		return -1;
	}

	peeprom_readbuff_tmp = peeprom_readbuff;
	blocknum = g_camCalE2promMapInfo[eeprom_map_index].e2prom_block_num;
	for (block_index = 0; block_index < blocknum; block_index++){
		blockaddr = g_camCalE2promMapInfo[eeprom_map_index].e2prom_block_info[block_index].AddrOffset;
		blocksize = g_camCalE2promMapInfo[eeprom_map_index].e2prom_block_info[block_index].datalen;
		if ((blocktotalsize + blocksize) > eeprom_readsize){
			pr_err("returnsize [%d], blocksize [%d], eeprom_readsize [%d]!\n", returnsize, blocksize, eeprom_readsize);
			kfree(peeprom_readbuff);
			return -1;
		}

		returnsize = eeprom_read_region(current_Client, blockaddr, blocksize, peeprom_readbuff_tmp);
		if (returnsize != blocksize){
			pr_err("returnsize [%d], blocksize [%d]!\n", returnsize, blocksize);
			kfree(peeprom_readbuff);
			return -1;
		}
		peeprom_readbuff_tmp = peeprom_readbuff_tmp + blocksize;
		blocktotalsize = blocktotalsize + blocksize;

		if (blocktotalsize == eeprom_readsize){
			break;
		}
	}

	if (blocktotalsize != eeprom_readsize){
		pr_err("blocktotalsize [%d] != eeprom_readsize [%d]!\n", blocktotalsize, eeprom_readsize);
		kfree(peeprom_readbuff);
		return -1;
	}

	*peeprombuff = peeprom_readbuff;
	*eepromsize  = blocktotalsize;

	pr_err("blocksize [%d]!\n", blocktotalsize);
	return blocktotalsize;
}

/*****************************************************************************
 Func Name     : eeprom_read_data
 Func Describe : eeprom read api, after search sensor will call this func
 input para    : sensorID
 output para   : null
 return value  : error: -1 , other: OK
*****************************************************************************/
int eeprom_read_data(unsigned int sensorID)
{
	unsigned int deviceID = 0;
	int i = 0;
	unsigned char *eeprombuff = 0;
	int ret = 0;
	unsigned int camcalmtkmapsize = 0;
	unsigned int eepromsize = 0;
	unsigned int slaveaddr = 0;
	struct i2c_client *current_Client = NULL;
	camcal_read_func_type readCamCalDatafunc = NULL;

	camcalmtkmapsize = sizeof(g_camCalMtkMapInfo)/sizeof(g_camCalMtkMapInfo[0]);
	for (i = 0; i < camcalmtkmapsize; i++)
	{
		if (sensorID == g_camCalMtkMapInfo[i].sensorID)
		{
			deviceID = g_camCalMtkMapInfo[i].deviceID;
			slaveaddr = g_camCalMtkMapInfo[i].i2c_addr;
			readCamCalDatafunc = g_camCalMtkMapInfo[i].camcal_read_func;
			break;
		}
	}

	if (i == camcalmtkmapsize){
		pr_err("can't find sensor [0x%x]!\n", sensorID);
		return -1;
	}

	if (true == g_camCalMtkMapInfo[i].activeflag){
		pr_err("sensor [0x%x] has find befor!\n", sensorID);
		return -1;
	}

	current_Client = EEPROM_get_i2c_client(deviceID);
	if (NULL == current_Client){
		pr_err("current_Client is null!\n");
		return -1;
	}

	current_Client->addr = slaveaddr >> 1;

	if (NULL == readCamCalDatafunc){
		pr_err("readCamCalDatafunc is null!\n");
		return -1;
	}

	pr_info("read eeprom data start!\n");

	mutex_lock(&sensor_eeprom_mutex);
	ret = readCamCalDatafunc(current_Client, sensorID, &eeprombuff, &eepromsize);
	if (ret < 0){
		pr_err("sensor [0x%x] read eeprom fail!\n", sensorID);
		mutex_unlock(&sensor_eeprom_mutex);
		return -1;
	}

	g_camCalMtkMapInfo[i].eeprombuff = eeprombuff;
	g_camCalMtkMapInfo[i].buffsize   = eepromsize;
	g_camCalMtkMapInfo[i].activeflag = true;
	mutex_unlock(&sensor_eeprom_mutex);

	pr_info("read eeprom data end!\n");

	return eepromsize;
}

/*****************************************************************************
 Func Name     : eeprom_get_data_from_buff
 Func Describe : get eeprom data from eeprom buff
 input para    : sensorID, size
 output para   : data
 return value  : error: -1 , other: OK
*****************************************************************************/
int eeprom_get_data_from_buff(unsigned char *data, unsigned int sensorID, unsigned int size)
{
	int i = 0;
	unsigned int camcalmtkmapsize = 0;

	if (0 == data){
		pr_err("data is null !\n");
		return -1;
	}

	camcalmtkmapsize = sizeof(g_camCalMtkMapInfo)/sizeof(g_camCalMtkMapInfo[0]);
	for (i = 0; i < camcalmtkmapsize; i++)
	{
		if ((sensorID == g_camCalMtkMapInfo[i].sensorID)
			&& (true == g_camCalMtkMapInfo[i].activeflag))
		{
			break;
		}
	}

	if (i == camcalmtkmapsize){
		return -1;
	}

	if ((NULL == g_camCalMtkMapInfo[i].eeprombuff) || (0 == g_camCalMtkMapInfo[i].buffsize))
	{
		pr_err("eeprombuff is null !\n");
		return -1;
	}

	pr_info("read data size [%d] !\n", size);
	if (size != g_camCalMtkMapInfo[i].buffsize){
		pr_err("size is err [0x%x]!\n", size);
		return -1;
	}

	mutex_lock(&sensor_eeprom_mutex);

	memcpy(data, g_camCalMtkMapInfo[i].eeprombuff, size);

	kfree(g_camCalMtkMapInfo[i].eeprombuff);
	g_camCalMtkMapInfo[i].eeprombuff = NULL;
	g_camCalMtkMapInfo[i].buffsize   = 0;
	g_camCalMtkMapInfo[i].activeflag = false;
	mutex_unlock(&sensor_eeprom_mutex);

	return size;
}

/*****************************************************************************
 Func Name     : EEPROM_hal_get_data
 Func Describe : hal get eeprom data will call this func
 input para    : sensorID, size
 output para   : data
 return value  : error: -1 , other: OK
*****************************************************************************/
int EEPROM_hal_get_data(unsigned char *data, unsigned int sensorID, unsigned int size)
{
	int ret = 0;

	if (0 == data){
		pr_err("data is null !\n");
		return -1;
	}

	pr_info("hal get data size [%d]!\n", size);

	ret = eeprom_get_data_from_buff(data, sensorID, size);
	if (ret < 0){
		pr_err("eeprom_get_data_from_buff fail!\n");
		ret = eeprom_read_data(sensorID);
		if (ret > 0){
			return eeprom_get_data_from_buff(data, sensorID, size);
		}
	}

	return ret;
}

/*****************************************************************************
 Func Name     : get_eeprom_byte_info_by_offset
 Func Describe : read 1 byte data from eeprom
 input para    : offset, sensorID
 output para   : null
 return value  : data value
*****************************************************************************/
unsigned char get_eeprom_byte_info_by_offset(unsigned int offset, unsigned int sensorID)
{
	unsigned int deviceID = 0;
	int i = 0;
	unsigned int returnsize = 0;
	unsigned int camcalmtkmapsize = 0;
	unsigned char readvalue = 0;
	unsigned int slaveaddr = 0;
	struct i2c_client *current_Client = NULL;

	camcalmtkmapsize = ARRAY_SIZE(g_camCalMtkMapInfo);
	for (i = 0; i < camcalmtkmapsize; i++)
	{
		if (sensorID == g_camCalMtkMapInfo[i].sensorID)
		{
			deviceID = g_camCalMtkMapInfo[i].deviceID;
			slaveaddr = g_camCalMtkMapInfo[i].i2c_addr;
			break;
		}
	}

	if (i == camcalmtkmapsize){
		pr_err("can't find sensor [0x%x]!\n", sensorID);
		return 0;
	}

	current_Client = EEPROM_get_i2c_client(deviceID);
	if (NULL == current_Client){
		pr_err("current_Client is null!\n");
		return 0;
	}

	current_Client->addr = slaveaddr >> 1;

	mutex_lock(&sensor_eeprom_mutex);
	/* read 1 byte from eeprom */
	returnsize = eeprom_read_region(current_Client, offset, 1, &readvalue);
	if (0 == returnsize){
		pr_err("read offset [%d] fail!\n", offset);
		mutex_unlock(&sensor_eeprom_mutex);
		return 0;
	}
	mutex_unlock(&sensor_eeprom_mutex);

	return readvalue;
}

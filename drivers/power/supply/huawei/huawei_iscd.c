/*
 * drivers/power/huawei_iscd.c
 *
 *huawei iscd driver
 *
 * Copyright (C) 2012-2015 HUAWEI, Inc.
 * Author: HUAWEI, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "huawei_iscd.h"
#include "mtk_charger_intf.h"
#include <linux/slab.h>
#include <linux/power/huawei_power_dsm.h>

char dsm_buff[ISCD_DSM_LOG_SIZE_MAX] = { 0 };
 
void iscd_reset_sample_info(struct iscd_info *iscd);
void iscd_main_proc(struct charger_manager *info);

extern int gauge_get_coulomb(void);
extern void fg_ocv_query_soc(int ocv);
extern int iscd_dsm_reporter(char* dsm_info, int errno);
extern int fg_get_soc_by_ocv(int ocv);
extern int fg_get_fcc(void);

void iscd_reset_sample_info(struct iscd_info *iscd)
{
	if ( NULL == iscd )
	{
		return;
	}
	iscd->isc = INVALID_ISC;
	iscd->enable = DISABLED;
	iscd->chg_done = false;
	iscd->size = SAMPLE_ZERO;
	iscd->loop_cnt = 0;
	memset(iscd->sample_info, 0, sizeof(iscd->sample_info));

	return;
}

static void iscd_init_dsm_level(struct iscd_info *iscd)
{
    iscd->level_config.isc_threshold = ISCD_DSM_THRESHOLD;
    iscd->level_config.dsm_err_no = DSM_BATTERY_ISCD_LEVEL0;
    iscd->level_config.dsm_report_cnt = 0;
    iscd->level_config.dsm_report_time = 0;

    return;
}

void iscd_probe(struct charger_manager *info)
{
	struct iscd_info *iscd;
	if ( NULL == info ){
		return;
	}

	iscd = kzalloc(sizeof(struct iscd_info), GFP_KERNEL);
	if (NULL == iscd) {
		chr_err("%s failed to alloc iscd struct\n",__func__);
		return;/*lint !e429*/
	}

	iscd_reset_sample_info(iscd);
	iscd_init_dsm_level(iscd);

	info->iscd = iscd;

	chr_err("ISCD: Init Success.\n");

	return;
}

static int iscd_sample_battery_info(struct charger_manager *info,
									struct iscd_sample_info *sample_info)
{
	int ocv_soc = 0;
	int fcc = 0;
	int ocv_mv = 0;
	int tbatt;
	s64 ocv_soc_uAh = 0;
	s64 cc_value;
	struct timespec sample_time;

	if (!info ||!sample_info) {
		chr_err("ISCD iscd buffer is null\n");
		return ERROR;
	}

	//battary temprature
	tbatt = battery_get_bat_temperature();
	if (tbatt > ISCD_TBATT_MAX|| tbatt < ISCD_TBATT_MIN) {
		chr_err("ISCD battery temperature is %d, out of range [%d, %d]",
			tbatt, ISCD_TBATT_MIN, ISCD_TBATT_MAX);
		return ERROR;
	}
	//sample time
	sample_time = current_kernel_time();

	//cc_value
	cc_value = (s64)gauge_get_coulomb(); // 0.1mAh

	//ocv_volt = vbat, when charg done
	ocv_mv = battery_get_bat_voltage();
	if (ocv_mv > ISCD_OCV_UV_MAX || ocv_mv <= ISCD_OCV_UV_MIN){
		//fg_ocv_query_soc will return directly, so can not get soc.
		chr_err("ISCD sample ocv_volt:%d wrong,	 try to next loop\n", ocv_mv);
		return ERROR;
	}

	//ocv soc
	fcc = fg_get_fcc();
	ocv_soc = fg_get_soc_by_ocv(ocv_mv * 10); // input unit 0.1mv, ocv_soc:9500=95%
	ocv_soc_uAh = (ocv_soc * (fcc / PERSENT_TRANS)) / PERSENT_TRANS; // keep precision

	sample_info->sample_time = sample_time;
	sample_info->tbatt = tbatt;
	sample_info->ocv_volt_mv = ocv_mv;
	sample_info->cc_value = cc_value * 100; // 0.1mAh to uAh
	sample_info->ocv_soc_uAh = ocv_soc_uAh;

	chr_err("iscd sample info: time_s=%ld,tbatt=%d,ocv_mV=%d,cc_uAh=%lld,"
		"ocv_soc=%d,fcc=%d,ocv_soc_uAh=%lld\n",
		sample_info->sample_time.tv_sec, sample_info->tbatt,
		sample_info->ocv_volt_mv, sample_info->cc_value,
		ocv_soc, fcc, sample_info->ocv_soc_uAh);

	return SUCCESS;
}

static void iscd_sample_info_choice(struct charger_manager *info,
				struct iscd_sample_info *sample_info)
{
    int delta_tbatt = 0;
    int delta_tbatt_abs = 0;

	s64 delta_cc = 0;

	if ( SAMPLE_ZERO== info->iscd->size )
	{
		//save sample
		memcpy(&(info->iscd->sample_info[0]), sample_info, sizeof(struct iscd_sample_info));
		info->iscd->size = SAMPLE_ONE;
	}
	else if ( SAMPLE_ONE == info->iscd->size )
	{
		//recharg check, when discharg, Qt0>Qt1
		delta_cc = info->iscd->sample_info[0].cc_value - sample_info->cc_value;
		//temprature check, tbat<10
        delta_tbatt = info->iscd->sample_info[0].tbatt - sample_info->tbatt;
	    delta_tbatt_abs = delta_tbatt >= 0 ? delta_tbatt : -delta_tbatt;

		if ( (ISCD_RECHARGE_CC <= delta_cc) || (ISCD_TBAT_DELTA_MAX <= delta_tbatt_abs) )
		{
			memset(&(info->iscd->sample_info[0]), 0, sizeof(struct iscd_sample_info));
			memcpy(&(info->iscd->sample_info[0]), sample_info, sizeof(struct iscd_sample_info));
			info->iscd->size = SAMPLE_ONE;
			chr_err("ISCD delta_cc(%lld) >= %d, delta_tbat(%d) >= %d, try to next loop\n",
				delta_cc, ISCD_RECHARGE_CC, delta_tbatt, ISCD_TBAT_DELTA_MAX);
			return;
		}

		//save sample
		memcpy(&(info->iscd->sample_info[1]), sample_info, sizeof(struct iscd_sample_info));
		info->iscd->size = SAMPLE_TWO;
	}
	else
	{
		chr_err("%s should not have 2 samples, check log.", __func__);
		iscd_reset_sample_info(info->iscd);
		return;
	}

	return;
}
static int iscd_sample_proc(struct charger_manager *info)
{
	int ret = SUCCESS;
	struct iscd_sample_info sample_info;

	memset(&sample_info, 0, sizeof(struct iscd_sample_info));

	ret = iscd_sample_battery_info(info, &sample_info);
	if ( SUCCESS != ret ){
		chr_err("ISCD sample battery info fail!\n");
		return ret;
	}

	//recharg + other check point + accept or reject
	iscd_sample_info_choice(info, &sample_info);
	return ret;
}

static int iscd_calc_isc_by_two_samples(struct charger_manager *info)
{
	int delta_tbatt = 0;
	int delta_ocv = 0;
	s64 delta_cc = 0;
	s64 delta_ocv_soc_uAh = 0;
	time_t delta_time = 0;
	int isc = INVALID_ISC;

	delta_time = info->iscd->sample_info[1].sample_time.tv_sec - info->iscd->sample_info[0].sample_time.tv_sec;
	delta_tbatt = info->iscd->sample_info[1].tbatt - info->iscd->sample_info[0].tbatt;
	delta_ocv = info->iscd->sample_info[0].ocv_volt_mv - info->iscd->sample_info[1].ocv_volt_mv;
	delta_ocv_soc_uAh = info->iscd->sample_info[0].ocv_soc_uAh - info->iscd->sample_info[1].ocv_soc_uAh;
	delta_cc = info->iscd->sample_info[1].cc_value- info->iscd->sample_info[0].cc_value;
	chr_err("ISCD calc isc by sample s1 s2, delta_time(s2-s1) = %lds, delta_tbatt(s2-s1) = %d, "
		"delta_ocv(s1-s2) = %dmV, delta_ocv_soc_uAh(s1-s2) = %llduAh, delta_cc(s2-s1) = %llduAh \n",
		delta_time, delta_tbatt, delta_ocv, delta_ocv_soc_uAh, delta_cc);

	if (delta_time > 0){
		isc = ((int)(delta_ocv_soc_uAh - delta_cc)) * SEC_PER_HOUR /(int)delta_time;
		chr_err("ISCD isc calc by two samples is %d uA\n", isc);
	}
	if ((isc < ISCD_CURR_VALID_MIN) || (isc > ISCD_CURR_VALID_MAX)) {
		chr_err("ISCD isc calc by two 10min samples is invalid(%d), discard it.\n", isc);
		isc = INVALID_ISC;
	}
	info->iscd->isc = isc;
	return isc;
}

void iscd_dump_dsm_info(struct charger_manager *info, char *buf)
{
	int i;
	char tmp_buf[ISCD_ERR_NO_STR_SIZE] = { 0 };

	if (!info || !buf){
		chr_err("ISCD %s input para is null\n", __func__);
		return;
	}
	chr_info("ISCD %s ++\n", __func__);
	snprintf(tmp_buf, (size_t)ISCD_ERR_NO_STR_SIZE, "isc is %d uA\n", info->iscd->isc);
	strncat(buf, tmp_buf, strlen(tmp_buf));
	snprintf(tmp_buf, (size_t)ISCD_ERR_NO_STR_SIZE, "sample_info:\n");
	strncat(buf, tmp_buf, strlen(tmp_buf));
	snprintf(tmp_buf, (size_t)ISCD_ERR_NO_STR_SIZE, "id time  tbatt ocv       ocv_rm/uAh cc/uAh\n");
	strncat(buf, tmp_buf, strlen(tmp_buf));

	for (i = 0; i < info->iscd->size; i++) {
		snprintf(tmp_buf, (size_t)ISCD_ERR_NO_STR_SIZE, "%-2d %-5ld %-5d %-7d %-10lld %-7lld\n",
			i,info->iscd->sample_info[i].sample_time.tv_sec, info->iscd->sample_info[i].tbatt,
			info->iscd->sample_info[i].ocv_volt_mv, info->iscd->sample_info[i].ocv_soc_uAh,
			info->iscd->sample_info[i].cc_value);
		strncat(buf, tmp_buf, strlen(tmp_buf));
	}
	chr_info("ISCD %s --\n", __func__);
}

static void iscd_dsm_report_decide(struct charger_manager *info)
{
	int ret = 0;
	struct timespec now = current_kernel_time();
	if ( info->iscd->isc <= info->iscd->level_config.isc_threshold )
	{
		chr_info("ISCD: isc not reach the threshold, no report dsm.\n");
	    return;
	}
	if (ISCD_DSM_REPORT_CNT_MAX <= info->iscd->level_config.dsm_report_cnt )
	{
		chr_info("ISCD: already report %d times, no report dsm.\n", info->iscd->level_config.dsm_report_cnt);
		return;
	}

	if (!info->iscd->level_config.dsm_report_time ||
		(now.tv_sec - info->iscd->level_config.dsm_report_time >= ISCD_DSM_REPORT_INTERVAL)) 
	{
		iscd_dump_dsm_info(info, dsm_buff);
		ret = power_dsm_dmd_report(POWER_DSM_BATTERY,info->iscd->level_config.dsm_err_no,dsm_buff);
		chr_info("ISCD: report dsm: size %d, dsm_buff::%s", ret, dsm_buff);

		info->iscd->level_config.dsm_report_cnt++;
		info->iscd->level_config.dsm_report_time = now.tv_sec;
		memset(&dsm_buff, (unsigned)0, sizeof(dsm_buff));
		return;
	}

	chr_info("ISCD: report in one day, no report dsm. nowtm=%ld, last_reporttm=%ld.\n",
		now.tv_sec, info->iscd->level_config.dsm_report_time);
	return;
}


void iscd_main_proc(struct charger_manager *info)
{
	int ret = ERROR;

	if ( NULL == info ){
		chr_err("%s input null err.\n", __func__);
		return;
	}

	info->iscd->chg_done = true;
	(info->iscd->loop_cnt)++;

	if ( ISCD_ACT_LOOP > info->iscd->loop_cnt ){
		return;
	}
	//cnt=30, 10min arrive
	info->iscd->loop_cnt = 0;
	//sample, recharge check in fun
	ret = iscd_sample_proc(info);
	if ( SUCCESS != ret )
	{
		iscd_reset_sample_info(info->iscd);
		return;
	}

	if ( SAMPLE_TWO == info->iscd->size )
	{
		//only get two samples between 10min, do calc
		ret = iscd_calc_isc_by_two_samples(info);
		if ( INVALID_ISC != ret )
		{
			//report DMD
			iscd_dsm_report_decide(info);
		}
		else
		{
			chr_info("ISCD: invalid isc, no report dsm.\n");
		}

		iscd_reset_sample_info(info->iscd);
	}

	return;
}

/* END:	  Added for short current:	2018/10/17 */

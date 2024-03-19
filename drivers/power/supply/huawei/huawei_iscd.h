/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __HUAWEI_ISCD_H__
#define __HUAWEI_ISCD_H__

#include <linux/time.h>

/* BEGIN: Added for short current:	2018/10/17 */
#define ISCD_SMAPLE_LEN_MAX 2
#define INVALID_ISC		(-1)

#define ISCD_ACT_LOOP		30		//20s*30=10min

#define ISCD_TBATT_MAX		50
#define ISCD_TBATT_MIN		10
#define ISCD_CURR_VALID_MIN	0
#define ISCD_CURR_VALID_MAX	500000  // uA

#define TENTH				10

#define SAMPLE_ZERO		0
#define SAMPLE_ONE		1
#define SAMPLE_TWO		2

#define SUCCESS			0
#define ERROR			1

#define ENABLED			1
#define DISABLED		0

#define ISCD_OCV_UV_MIN	2500  //2.5V
#define ISCD_OCV_UV_MAX	4500 //4.5V

#define ISCD_RECHARGE_CC	1000	//uAh
#define ISCD_TBAT_DELTA_MAX	10
#define PERSENT_TRANS		100

#define SEC_PER_HOUR 3600

#define ISCD_DSM_THRESHOLD		100000  // uA
#define ISCD_DSM_REPORT_CNT_MAX		2
#define ISCD_DSM_REPORT_INTERVAL	(72*3600)	// 72h
#define ISCD_DSM_LOG_SIZE_MAX		(2048)
#define ISCD_ERR_NO_STR_SIZE 128

enum ISCD_RESET_SAMPLE_TYPE {
	ISCD_RESET_INIT,
	ISCD_RESET_PLGIN,
	ISCD_RESET_PLGOUT,
	ISCD_RESET_AFTCACL,
	ISCD_RESET_NOTCHGDONE,
	ISCD_RESET_RECOVERR,

	ISCD_RESET_BUTT
};

struct iscd_sample_info
{
	struct timespec sample_time;
	int ocv_volt_mv;	//mV
	s64 ocv_soc_uAh;	//uAh, capacity get by OCV form battery model parameter table
	s64 cc_value;		//uAh
	int tbatt;
};


struct iscd_level_config{
	int isc_threshold;
	int dsm_err_no;
	int dsm_report_cnt;
	time_t dsm_report_time;
};

struct iscd_info {
	int enable;
	bool chg_done;

	int isc;//internal short current, uA

	int size;
	unsigned int loop_cnt;
	struct iscd_sample_info sample_info[ISCD_SMAPLE_LEN_MAX];

	struct iscd_level_config level_config;
};

/* END:	  Added for short current: 2018/10/17 */


#endif /* __HUAWEI_ISCD_H__ */

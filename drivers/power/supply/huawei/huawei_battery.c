/*
 * drivers/power/huawei/huawei_battery.c
 *
 * huawei charger driver
 *
 * Copyright (C) 2012-2017 HUAWEI, Inc.
 * Author: HUAWEI, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/power/huawei_charger.h>
#include <linux/i2c.h>
#include <linux/version.h>


/*add include MTK platform head file*/
#include <mt-plat/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <pmic.h>
#include <mtk_charger_intf.h>

/*For MTK platform*/
typedef enum charger_type CHARGER_TYPE;
//static CHARGER_TYPE g_chr_type;
//static bool g_chgr_online;

/*add For MTK platform*/
static struct charger_consumer *g_pconsumer;

static int huawei_factory_diag_flag = 0;
int huawei_set_charger_hz_onoff = 0; /*default: disable*/
int huawei_set_charger_charging_enable = 0; /*default: disable*/
int huawei_set_charger_input_current = -1; /*default: no user input current limit*/

extern void charger_enable(struct charger_consumer *consumer, int chg_en);
extern int charger_manager_enable_charging(struct charger_consumer *consumer, int idx, bool en);
extern int charger_manager_reset_learned_cc(struct charger_consumer *consumer,int val);
extern int charger_manager_set_voltage_max(struct charger_consumer *consumer,int val);
extern int charger_manager_set_current_max(struct charger_consumer *consumer,int val);
extern signed int battery_get_vbus(void);
extern signed int battery_get_bat_voltage(void);
extern signed int battery_get_bat_temperature(void);

extern int product_version;
#ifndef DOUBLE_85
#define DOUBLE_85  1
#endif
#define DEFAULT_BATT_TEMP               25
#define LOW_BATT_TEMP                   2

#define BATTERY_CURRENT_DIVI 10
#define UA_PER_MA       (1000)
#define MIN_INPUT_CURRENT 100000  /*100mA*/


#define BUFFER_SIZE                     30
#define CHG_OVP_REG_NUM                 3
#define CHG_DIS_REG_NUM                 3
#define DEFAULT_FASTCHG_MAX_CURRENT     2000
#define DEFAULT_FASTCHG_MAX_VOLTAGE     4400000
#define DEFAULT_USB_ICL_CURRENT         2000
#define DEFAULT_CUSTOM_COOL_LOW_TH      0
#define DEFAULT_CUSTOM_COOL_UP_TH       50
#define DEFAULT_CUSTOM_COOL_CURRENT     300
/* 1A/2A charger adaptive algo */
#define BATTERY_IBUS_MARGIN_MA          2000
#define BATTERY_IBUS_CONFIRM_COUNT      6
#define BATTERY_IBUS_WORK_DELAY_TIME    1000
#define BATTERY_IBUS_WORK_START_TIME    3000
#define IINLIM_1000                     1000
#define IINLIM_1500                     1500
#define IINLIM_2000                     2000
#define BATT_HEALTH_TEMP_HOT            600
#define BATT_HEALTH_TEMP_WARM           480
#define BATT_HEALTH_TEMP_COOL           100
#define BATT_HEALTH_TEMP_COLD           -200
#define EMPTY_SOC                       0
#define FACTORY_SOC_LOW                 1
#define FULL_SOC                        100

#define MONITOR_CHARGING_DELAY_TIME 5000


#define MV_TO_UV                        1000
#define MA_TO_UA                        1000

struct huawei_battery_info {
    struct device *dev;
    struct power_supply *batt_psy;
    enum power_supply_property *batt_props;
    int  num_props;

    struct power_supply *bk_batt_psy;
    struct power_supply *usb_psy;
    struct power_supply *hwbatt_psy;
    struct power_supply_desc hwbatt_desc;
    struct power_supply_config hwbatt_cfg;
    struct power_supply *extra_batt_psy;
    struct power_supply_desc batt_extra_psy_desc;
    struct power_supply_config batt_cfg;
    int charger_batt_capacity_mah;
    bool charger_power_path_support;
    int  usb_icl_current;

    struct delayed_work sync_voltage_work;
};
static struct huawei_battery_info *g_info =NULL;


static bool factory_mode = false;
static int __init early_parse_factory_mode(char *cmdline)
{
    if (!cmdline) {
        return 0;
    }

    if ((cmdline) && !strncmp(cmdline, "factory", strlen("factory"))) {
        factory_mode = true;
    }

    return 0;
}
early_param("androidboot.huawei_swtype", early_parse_factory_mode);

/***************************************************
 Function: get_prop_from_psy
 Description: get property from power supply instance
 Parameters:
    psy         the power supply instance
    prop        the property need to get
    val         the place used to hold values
 Return:
    0 means read successful
    negative mean fail
***************************************************/
static int get_prop_from_psy(struct power_supply *psy,
                enum power_supply_property prop,
                union power_supply_propval *val)
{
    int rc = 0;

    if (!psy || !val ||!(psy->desc)) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return -EINVAL;
    }

    rc = psy->desc->get_property(psy, prop, val);
    if (rc < 0) {
        pr_debug("Get property %d from %s psy fail, rc = %d\n",
            prop, psy->desc->name, rc);
    }

    return rc;
}

#if 0
/***************************************************
 Function: set_prop_from_psy
 Description: set property to power supply instance
 Parameters:
    psy         the power supply instance
    prop        the property need to set
    val         the place used to hold values
 Return:
    0 means read successful
    negative mean fail
***************************************************/
static int set_prop_to_psy(struct power_supply *psy,
                enum power_supply_property prop,
                const union power_supply_propval *val)
{
    int rc = 0;

    if (!psy || !val||!(psy->desc) ) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return -EINVAL;
    }

    rc = psy->desc->set_property(psy, prop, val);
    if (rc < 0) {
        pr_err("Set property %d to %s psy fail, rc = %d\n",
            prop, psy->desc->name, rc);
    }

    return rc;
}
#endif

/***************************************************
 Function: sync_max_voltage
 Description: sync max voltage between hwbatt and bk battery
 Parameters:
    info            huawei_battery_info
 Return:
    0 means sccuess, negative means error
***************************************************/
static int sync_max_voltage(struct huawei_battery_info *info)
{
    int rc = 0;
    //union power_supply_propval val = {0, };

    if (!info) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return -EINVAL;
    }

    //rc = info->hwbatt_psy->desc->get_property(info->hwbatt_psy,
                    //POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
    if (rc < 0) {
        return rc;
    }

    //rc = vote(info->vbat_votable, VBAT_BASP_VOTER, true, val.intval);

    return rc;
}

/***************************************************
 Function: battery_sync_voltage_work
 Description: sync max voltage between hwbatt and bk battery

 Parameters:
    work            work_struct
 Return: NA
***************************************************/
static void battery_sync_voltage_work(struct work_struct *work)
{
    struct huawei_battery_info *info = NULL;

    if (!work) {
        pr_err("Invalid param, fatal error\n");
        return;
    }

    info = container_of(work, struct huawei_battery_info,
                                    sync_voltage_work.work);
    if (!info) {
        pr_err("Invalid battery info point\n");
        return;
    }

	sync_max_voltage(info);

}

struct charger_device *huawei_get_charger_device(struct charger_consumer *consumer, int idx)
{
	struct charger_device *pdev = NULL;
	struct charger_manager *info = NULL;

	if (consumer != NULL) {
		info = consumer->cm;
		if (info != NULL) {
			if (idx == MAIN_CHARGER) {
				pdev = info->chg1_dev;
			} else if (idx == SLAVE_CHARGER){
				pdev = info->chg2_dev;
			} else {
				pdev = NULL;
			}
		}
	}
	return pdev;
}

struct charger_data *huawei_get_charger_data(struct charger_consumer *consumer, int idx)
{
	struct charger_data *pdata = NULL;
	struct charger_manager *info = NULL;

	if (consumer != NULL) {
		info = consumer->cm;
		if (info != NULL) {
			if (idx == MAIN_CHARGER) {
				pdata = &info->chg1_data;
			} else if (idx == SLAVE_CHARGER){
				pdata = &info->chg2_data;
			} else {
				pdata = NULL;
			}
		}
	}
	return pdata;
}

int huawei_get_charging_enable(void)
{
	return huawei_set_charger_charging_enable;
}

int huawei_set_charging_enable(struct charger_consumer *consumer,int chg_en)
{
	if (consumer != NULL){
		huawei_set_charger_charging_enable = chg_en;
		charger_enable(consumer,chg_en);
		chr_err("%s set charging_enable to %d\n", __func__, huawei_set_charger_charging_enable);
	}else{
		chr_err("%s, input para is null\n", __func__);
	}
	return 0;
}

int huawei_get_hz_enable(void)
{
	return huawei_set_charger_hz_onoff;
}

int huawei_set_hz_enable(struct charger_consumer *consumer, int idx, int hz_en)
{
	struct charger_device *pdev = NULL;

	if (consumer != NULL) 
	{
		pdev = huawei_get_charger_device(consumer, idx);
		if (pdev != NULL) 
		{
			huawei_set_charger_hz_onoff = hz_en;
			charger_dev_enable_hz(pdev, hz_en);
			chr_err("%s set hz_enable to %d\n", __func__, huawei_set_charger_hz_onoff);
		}
//		charger_manager_enable_charging(consumer, idx, !hz_en);
	}
	return 0;
}

int huawei_set_max_input_curr_limit(struct charger_consumer *consumer, int idx, int input_current)
{
	struct charger_manager *chg_info = NULL;
	struct charger_data *pdata = NULL;
	struct charger_device *pdev = NULL;
	CHARGER_TYPE  chr_type;
	int temp = DEFAULT_BATT_TEMP;


	temp = battery_get_bat_temperature();
	chr_err("%s:  get temp:%d input_current %d\n", __func__,  temp,input_current);
	chr_type = mt_get_charger_type();

	if ((consumer != NULL)&&(g_info != NULL))
	{
		pdev = huawei_get_charger_device(consumer, idx);
		pdata = huawei_get_charger_data(consumer, idx);
		chg_info = consumer->cm;
		if ( (chg_info != NULL) && (pdev != NULL) && (pdata != NULL) )
		{
			if ((product_version == DOUBLE_85)&&(temp<LOW_BATT_TEMP))
			{
					huawei_set_charger_input_current = g_info->usb_icl_current *MV_TO_UV;
					chr_err("DOUBLE_85,LOW_BATT_TEMP set ibus = %d\n",huawei_set_charger_input_current);
			}
			else
			{
#if 0
				if (input_current <= 0) {
					huawei_set_charger_input_current = info->data.ac_charger_input_current;  /*unlimit*/
				} else if (input_current <= MIN_INPUT_CURRENT) {
					huawei_set_charger_input_current = MIN_INPUT_CURRENT;
				} else {
					huawei_set_charger_input_current = min(input_current, info->data.ac_charger_input_current);
				}

#endif
			huawei_set_charger_input_current = min(input_current, g_info->usb_icl_current*MV_TO_UV );

			}
			pdata->input_current_limit = huawei_set_charger_input_current;
			chr_err("%s set max iin_current to %d\n", __func__, huawei_set_charger_input_current);

			if ( (chr_type != CHARGER_UNKNOWN) && (huawei_set_charger_hz_onoff != 1) )
			{
				//charger_dev_set_input_current(pdev, pdata->input_current_limit);
				return chg_info->change_current_setting(chg_info);
			}
		}
	}
	return 0;
}

int huawei_get_thermal_input_curr_limit(struct charger_consumer *consumer, int idx)
{
	struct charger_data *pdata = NULL;

	if (consumer != NULL) 
	{
		pdata = huawei_get_charger_data(consumer, idx);
		if (pdata != NULL) {
			return pdata->thermal_input_current_limit;
		}
	}

	return 0;
}

int huawei_set_thermal_input_curr_limit(struct charger_consumer *consumer, int idx, int input_current)
{
	struct charger_manager *chg_info = NULL;
	struct charger_data *pdata = NULL;
	CHARGER_TYPE  chr_type;

	if (product_version == DOUBLE_85)
	{
		return 0;
	}
	chr_type = mt_get_charger_type();

	if (consumer != NULL)
	{
		pdata = huawei_get_charger_data(consumer, idx);
		chg_info = consumer->cm;
		if ( (chg_info != NULL) && (pdata != NULL) &&(g_info !=NULL))
		{
			chr_err("%s:  idx:%d set in_thermal_limit:%d\n", __func__, idx, input_current);
			if (input_current <= 1) {
				pdata->thermal_input_current_limit = g_info->usb_icl_current*MV_TO_UV;
			} else {
				pdata->thermal_input_current_limit = min(input_current, g_info->usb_icl_current*MV_TO_UV);
			}

			if ( (chr_type != CHARGER_UNKNOWN) && (huawei_set_charger_hz_onoff != 1) )
			{
				return chg_info->change_current_setting(chg_info);
			}
		}
	}
	return 0;
}

static enum power_supply_property hwbatt_properties[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_CHARGE_TYPE,
    POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
    POWER_SUPPLY_PROP_CHARGE_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CHARGE_CURRENT_NOW,
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_IN_THERMAL,
    POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
    POWER_SUPPLY_PROP_HIZ_MODE,
    POWER_SUPPLY_PROP_FACTORY_DIAG,
    POWER_SUPPLY_PROP_BATTERY_TYPE,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
    POWER_SUPPLY_PROP_RESET_LEARNED_CC,
};

static int hwbatt_get_property(struct power_supply *psy,
    enum power_supply_property psp, union power_supply_propval *val)
{
    int rc = 0;
    bool curr_sign = battery_get_bat_current_sign();

    if (NULL == g_info || NULL == g_pconsumer) {
        dev_info(g_info->dev, "Invalid battery info pointer\n");
        return -EINVAL;
    }


    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        rc = get_prop_from_psy(g_info->bk_batt_psy, POWER_SUPPLY_PROP_ONLINE, val);
        if (rc < 0) {
            pr_err("%s: get POWER_SUPPLY_PROP_ONLINE failed\n", __func__);
            val->intval = 0;
        }
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_OCV:
        val->intval = battery_get_bat_voltage();
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        val->intval = mt_get_charger_type();
        break;
    case POWER_SUPPLY_PROP_CHARGE_VOLTAGE_NOW:
        val->intval = battery_get_vbus();
        break;
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        val->intval = huawei_get_charging_enable();
        break;
    case POWER_SUPPLY_PROP_CHARGE_CURRENT_NOW:
        val->intval = curr_sign ? (battery_get_bat_current()/BATTERY_CURRENT_DIVI) : -1*(battery_get_bat_current()/BATTERY_CURRENT_DIVI);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
        break;
    case POWER_SUPPLY_PROP_IN_THERMAL:
        val->intval = (huawei_get_thermal_input_curr_limit(g_pconsumer, MAIN_CHARGER)/UA_PER_MA);/*mA to uA*/
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
        //val->intval = huawei_get_max_input_curr_limit(g_pconsumer);
        val->intval = g_info->usb_icl_current;
        break;
    case POWER_SUPPLY_PROP_HIZ_MODE:
        val->intval = huawei_get_hz_enable();
        break;
    case POWER_SUPPLY_PROP_FACTORY_DIAG:
        val->intval = !(huawei_factory_diag_flag);
        break;
    case POWER_SUPPLY_PROP_BATTERY_TYPE:
        val->strval = huawei_get_battery_type();
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
        val->intval =charger_manager_const_charge_current_max(g_pconsumer);
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        val->intval = charger_manager_get_voltage_max(g_pconsumer );
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
        val->intval = DEFAULT_FASTCHG_MAX_VOLTAGE;
        break;
    case POWER_SUPPLY_PROP_RESET_LEARNED_CC:
        val->intval = 0 ;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int hwbatt_set_property(struct power_supply *psy,
    enum power_supply_property psp, const union power_supply_propval *val)
{
	int ret = 0;
    if (NULL == g_info || NULL == g_pconsumer) {
        dev_info(g_info->dev, "Invalid battery info pointer\n");
        return -EINVAL;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        huawei_set_charging_enable(g_pconsumer, val->intval);
        return 0;
    case POWER_SUPPLY_PROP_IN_THERMAL:
        huawei_set_thermal_input_curr_limit(g_pconsumer, MAIN_CHARGER, val->intval*UA_PER_MA);/*note: mA to uA*/
        return 0;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
        huawei_set_max_input_curr_limit(g_pconsumer, MAIN_CHARGER, val->intval*UA_PER_MA);/*note: mA to uA*/
        return 0;
    case POWER_SUPPLY_PROP_HIZ_MODE:
        huawei_set_hz_enable(g_pconsumer, MAIN_CHARGER, val->intval);
        return 0;
    case POWER_SUPPLY_PROP_FACTORY_DIAG:
        huawei_factory_diag_flag = !val->intval;
        if (g_info->charger_power_path_support) {
            huawei_set_charging_enable(g_pconsumer, val->intval);
        } else {
            if (huawei_factory_diag_flag){ 
                huawei_set_max_input_curr_limit(g_pconsumer, MAIN_CHARGER, MIN_INPUT_CURRENT);/*limit 100mA input current */
            } else {
                huawei_set_max_input_curr_limit(g_pconsumer, MAIN_CHARGER, 0);/*unlimit input current*/
            }
        }
        return ret;
	case POWER_SUPPLY_PROP_RESET_LEARNED_CC:
		ret =charger_manager_reset_learned_cc(g_pconsumer,val->intval );
		 return ret;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = charger_manager_set_voltage_max(g_pconsumer,val->intval );
		 return ret;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = charger_manager_set_current_max(g_pconsumer,val->intval );
		 return ret;
    default:
        return -EINVAL;
    }

    return 0;
}

static int hwbatt_property_is_writeable(struct power_supply *psy,
                enum power_supply_property prop)
{
    int rc = 0;

    if (!psy) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return 0;
    }

    /* pre process */
    switch(prop) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
    case POWER_SUPPLY_PROP_IN_THERMAL:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
    case POWER_SUPPLY_PROP_HIZ_MODE:
    case POWER_SUPPLY_PROP_FACTORY_DIAG:
	case POWER_SUPPLY_PROP_RESET_LEARNED_CC:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
        rc = 1;
        break;
    default:
        rc = 0;
        break;
    }
    return rc;
}
static enum power_supply_property hw_extra_batt_props[] = {
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
};

#define DEFAULT_BATTERY_TEMP      250
#define DEFAULT_BATT_CAPACITY      50
static int hw_extra_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
    int rc = 0;
	if((!psy)||(!val))
		return -EINVAL;
    if (NULL == g_info) {
        dev_info(g_info->dev, "Invalid battery info pointer\n");
        return -EINVAL;
    }
	switch (psp) {
	    case POWER_SUPPLY_PROP_TEMP:
			rc = get_prop_from_psy(g_info->batt_psy, POWER_SUPPLY_PROP_TEMP, val);
			if (rc < 0) {
				pr_err("%s: get POWER_SUPPLY_PROP_TEMP failed\n", __func__);
				val->intval = DEFAULT_BATTERY_TEMP;
			}
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			rc = get_prop_from_psy(g_info->batt_psy, POWER_SUPPLY_PROP_CAPACITY, val);
			if (rc < 0) {
				pr_err("%s: get POWER_SUPPLY_PROP_CAPACITY failed\n", __func__);
				val->intval = DEFAULT_BATT_CAPACITY;
			}
			break;
		default:
			break;
	}
	return 0;
}

/***************************************************
 Function: huawei_battery_probe
 Description: battery driver probe
 Parameters:
    pdev            the device related with battery
 Return:
    0 mean drvier probe successful
    negative mean probe fail
***************************************************/
static int huawei_battery_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct huawei_battery_info *info = NULL;
    struct device_node *node = NULL;
    const char *psy_name = NULL;
    int usb_icl_current = 0;


    if (!pdev) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return -EINVAL;
    }
    node = pdev->dev.of_node;
	if (!node) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

    info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
    if (!info) {
        dev_err(&pdev->dev, "Unable to alloc memory!\n");
        return -ENOMEM;
    }
    info->dev = &pdev->dev;
    platform_set_drvdata(pdev, info);

    info->charger_power_path_support = of_property_read_bool(node, "product_charger_support_powerpath");
   ret = of_property_read_u32(pdev->dev.of_node, "huawei,icl_current", &usb_icl_current);
   if(ret<0)
   {
       info->usb_icl_current = DEFAULT_USB_ICL_CURRENT;
   }
   else {
    info->usb_icl_current = usb_icl_current;
   }
   dev_err(info->dev, "charger_power_path_support: %d usb_icl=%d\n", info->charger_power_path_support,info->usb_icl_current );


    info->hwbatt_desc.name = "hwbatt";
    info->hwbatt_desc.type = POWER_SUPPLY_TYPE_HWBATT;
    info->hwbatt_desc.properties = hwbatt_properties;
    info->hwbatt_desc.num_properties = ARRAY_SIZE(hwbatt_properties);
    info->hwbatt_desc.set_property = hwbatt_set_property;
    info->hwbatt_desc.get_property = hwbatt_get_property;
    info->hwbatt_desc.property_is_writeable = hwbatt_property_is_writeable;
    info->hwbatt_cfg.drv_data = info;
    info->hwbatt_cfg.of_node = info->dev->of_node;

    info->hwbatt_psy = power_supply_register(&pdev->dev, &info->hwbatt_desc, &info->hwbatt_cfg);
    if (IS_ERR(info->hwbatt_psy)) {
        dev_err(&pdev->dev, "Failed to register power supply: %ld\n", PTR_ERR(info->hwbatt_psy));
        ret = -EPROBE_DEFER;
        goto PSY_REG_FAIL;
    }


    psy_name = "battery";
    info->batt_psy = power_supply_get_by_name(psy_name);
    if (!info->batt_psy) {
        dev_err(info->dev, "Cannot get batery psy %s\n", psy_name);
        ret = -EPROBE_DEFER;
        goto PSY_GET_FAIL;
    }

    psy_name = "charger";
    info->bk_batt_psy = power_supply_get_by_name(psy_name);
    if (!info->bk_batt_psy) {
        dev_err(info->dev, "Cannot get bk_batery psy %s\n", psy_name);
        ret = -EPROBE_DEFER;
        goto PSY_GET_FAIL;
    }

    psy_name = "usb";
    info->usb_psy = power_supply_get_by_name(psy_name);
    if (!info->usb_psy) {
        dev_err(info->dev, "Cannot get usb psy %s\n",psy_name);
        ret = -EPROBE_DEFER;
        goto PSY_GET_FAIL;
    }
    INIT_DELAYED_WORK(&info->sync_voltage_work, battery_sync_voltage_work);
    schedule_delayed_work(&info->sync_voltage_work,
					msecs_to_jiffies(MONITOR_CHARGING_DELAY_TIME));
    g_info = info;

    g_pconsumer = charger_manager_get_by_name(&pdev->dev, "charger");

    info->batt_extra_psy_desc.name = "Battery";
    info->batt_extra_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
    info->batt_extra_psy_desc.properties = hw_extra_batt_props;
    info->batt_extra_psy_desc.num_properties = ARRAY_SIZE(hw_extra_batt_props);
    info->batt_extra_psy_desc.set_property = NULL;
    info->batt_extra_psy_desc.get_property = hw_extra_batt_get_prop;
    info->batt_extra_psy_desc.property_is_writeable = NULL;
    info->batt_cfg.drv_data = info;
    info->batt_cfg.of_node = info->dev->of_node;

    info->extra_batt_psy = power_supply_register(&pdev->dev, &info->batt_extra_psy_desc, &info->batt_cfg);
    if (IS_ERR(info->extra_batt_psy)) {
        dev_err(&pdev->dev, "Failed to register power supply: %ld\n", PTR_ERR(info->extra_batt_psy));
        ret = -EPROBE_DEFER;
        goto PSY_REG_FAIL;
    }
    pr_info("%s ok\n",__func__);

    return 0;

PSY_GET_FAIL:
	power_supply_unregister(info->hwbatt_psy);
PSY_REG_FAIL:
    devm_kfree(&pdev->dev, info);
    info = NULL;
    return ret;
}

/***************************************************
 Function: huawei_battery_remove
 Description: battery driver remove
 Parameters:
    pdev            the device related with battery
 Return:
    0 mean driver remove successful
    negative mean remove fail
***************************************************/
static int huawei_battery_remove(struct platform_device *pdev)
{
    struct huawei_battery_info *info = NULL;

    if (!pdev) {
        pr_err("%s: Invalid param, fatal error\n", __func__);
        return -EINVAL;
    }

    info = dev_get_drvdata(&pdev->dev);

    power_supply_unregister(info->hwbatt_psy);
    devm_kfree(&pdev->dev, info);
	g_info = NULL;

    return 0;
}

static struct of_device_id battery_match_table[] =
{
    {
        .compatible = "huawei,battery",
        .data = NULL,
    },
    {
    },
};

static struct platform_driver huawei_battery_driver =
{
    .probe = huawei_battery_probe,
    .remove = huawei_battery_remove,
    .driver =
    {
        .name = "huawei,battery",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(battery_match_table),
    },
};

static int __init huawei_battery_init(void)
{
    return platform_driver_register(&huawei_battery_driver);
}
static void __exit huawei_battery_exit(void)
{
    platform_driver_unregister(&huawei_battery_driver);
}
module_init(huawei_battery_init);
module_exit(huawei_battery_exit);

MODULE_DESCRIPTION("huawei battery driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:huawei-battery");

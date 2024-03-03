/*
 * ETA6937 battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/time.h>

#include "mtk_charger_intf.h"
#include "eta6937_reg.h"
#ifdef CONFIG_HUAWEI_DEV_SELFCHECK
#include <huawei_platform/dev_detect/hw_dev_detect.h>
#endif
static struct charger_device *g_chargerDev;
static void Otg_Start_Timer(void);
struct hrtimer g_kickWdtTimer;
wait_queue_head_t g_waitOtgWdtQue;
bool g_otgWdtTimeout = false;
#define WAIT_SECONDS 10
#define WAIT_NS 0
static bool g_otg_En = false;
static bool g_overTime = false;

enum eta6937_part_no {
	ETA6937 = ETA_ID,
	SY6923 = SY_ID,
};

struct eta6937_config {
	int chg_mv;
	int chg_ma;
	int ivl_mv;
	int icl_ma;
	int safety_chg_mv;
	int safety_chg_ma;
	int iterm_ma;
	int batlow_mv;
	int sensor_mohm;
	bool enable_term;
};


struct eta6937 {
	struct device *dev;
	struct i2c_client *client;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	enum eta6937_part_no part_no;
	int revision;
	struct eta6937_config cfg;
	bool charge_enabled;
	int chg_mv;
	int chg_ma;
	int ivl_mv;
	int icl_ma;
	bool boost_mode;
	int prev_stat_flag;
	int prev_fault_flag;
	int reg_stat;
	int reg_fault;
	int reg_stat_flag;
	int reg_fault_flag;
	int skip_reads;
	int skip_writes;
	struct mutex i2c_rw_lock;
};


static int eta_set_chargevoltage(struct charger_device *chg_dev, u32 volt);
static int eta_set_chargecurrent(struct charger_device *chg_dev, u32 curr);
static int EtaSetVdpm(struct charger_device *chg_dev, u32 volt);
static int eta_set_input_current_limit(struct charger_device *chg_dev, u32 curr);

unsigned int g_hiz_status = ETA6937_HZ_MODE_DISABLE;

static int __eta_read_reg(struct eta6937 *bq, u8 reg, u8 *data)
{
	s32 ret = 0;
	int i = 0;

	if(NULL == bq || NULL == data)
		return ERROR;
	for(i=0;i<READ_TIMES;i++){
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if (ret < 0) {
			pr_err("i2c read fail: [%d]can't read from reg 0x%02X\n", i, reg);
			mdelay(SECOND_1);//sleep 1 ms
			continue;
		}
		else{
			*data = (u8)ret;
			return 0;
		}
	}
	return ret;
}

static int __eta_write_reg(struct eta6937 *bq, int reg, u8 val)
{
	s32 ret = 0;
	int i = 0;

	if(NULL == bq)
		return ERROR;
	for(i=0;i<READ_TIMES;i++){
		ret = i2c_smbus_write_byte_data(bq->client, reg, val);
		if (ret < 0) {
			pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
			mdelay(SECOND_1);//sleep 1 ms
			continue;

		}
		else{
			return 0;
		}
	}
	return ret;
}

static int eta_read_byte(struct eta6937 *bq, u8 reg, u8 *data)
{
	int ret = 0;

	if(NULL == bq || NULL == data)
		return ERROR;
	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __eta_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int eta_update_bits(struct eta6937 *bq, u8 reg,
					u8 mask, u8 data)
{
	int ret = 0;
	u8 tmp = 0;
	u8 val = 0;

	if(NULL == bq)
		return ERROR;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __eta_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	//hz mode
	if((ETA6937 == bq->part_no) &&
	    (ETA6937_REG_01 == reg) &&
	    g_hiz_status) {
	    tmp &= ~ETA6937_HZ_MODE_MASK;
	    val = ETA6937_HZ_MODE_ENABLE << ETA6937_HZ_MODE_SHIFT;
	    tmp |= val & ETA6937_HZ_MODE_MASK;
	}

	if((bq->part_no == ETA6937) &&
		(reg == ETA6937_REG_01) &&
		bq->charge_enabled &&
		!g_overTime) {
		tmp &= ~ETA6937_CHARGE_ENABLE_MASK;
		val = ETA6937_CHARGE_ENABLE << ETA6937_CHARGE_ENABLE_SHIFT;
		tmp |= val & ETA6937_CHARGE_ENABLE_MASK;
	}
	ret = __eta_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int eta_enable_charger(struct eta6937 *bq)
{
	int ret = 0;
	u8 en = ETA6937_CHARGE_ENABLE << ETA6937_CHARGE_ENABLE_SHIFT;
	int times = 0;

	if(NULL == bq) {
		return ERROR;
	}
	if(bq->part_no == ETA6937) {
		for(times = 0; times < ENABLE_TIMES; times++) {
			ret = eta_update_bits(bq, ETA6937_REG_01,
				ETA6937_CHARGE_ENABLE_MASK, en);
		}
	} else {
		gpio_set_value(GPIO_CHG_EN_0, GPIO_OUT_ZERO);
	}

	return ret;
}

static int eta_disable_charger(struct eta6937 *bq)
{
	int ret = 0;

	u8 val = ETA6937_CHARGE_DISABLE << ETA6937_CHARGE_ENABLE_SHIFT;
	if(NULL == bq) {
		return ERROR;
	}

	if(bq->part_no == ETA6937) {
		ret = eta_update_bits(bq, ETA6937_REG_01,
			ETA6937_CHARGE_ENABLE_MASK, val);
	} else {
		gpio_set_value(GPIO_CHG_EN_0, GPIO_OUT_ONE);
	}

	return ret;
}


static int eta_set_low_chg(struct eta6937 *bq, bool en)
{
	if(NULL == bq)
		return ERROR;
	return eta_update_bits(bq, ETA6937_REG_05, ETA6937_LOW_CHG_MASK,
				en ? LOWCHG_EN : LOWCHG_DISEN);
}
static int eta_enable_term(struct eta6937 *bq, bool enable)
{
	u8 val = 0;
	int ret = 0;

	if(NULL == bq)
		return ERROR;
	if (enable)
		val = ETA6937_TERM_ENABLE;
	else
		val = ETA6937_TERM_DISABLE;

	val <<= ETA6937_TERM_ENABLE_SHIFT;

	ret = eta_update_bits(bq, ETA6937_REG_01,
				ETA6937_TERM_ENABLE_MASK, val);

	return ret;
}

int eta_reset_chip(struct eta6937 *bq)
{
	int ret = 0;
	u8 val = ETA6937_RESET << ETA6937_RESET_SHIFT;

	if(NULL == bq)
		return ERROR;
	ret = eta_update_bits(bq, ETA6937_REG_04,
				ETA6937_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(eta_reset_chip);

static int eta_set_vbatlow_volt(struct eta6937 *bq, int volt)
{
	int ret = 0;
	u8 val = 0;

	if(NULL == bq)
		return ERROR;
	val = (volt - ETA6937_WEAK_BATT_VOLT_BASE) / ETA6937_WEAK_BATT_VOLT_LSB;

	val <<= ETA6937_WEAK_BATT_VOLT_SHIFT;

	pr_info("VBARLOW:0x%02X\n", val);

	ret = eta_update_bits(bq, ETA6937_REG_01,
				ETA6937_WEAK_BATT_VOLT_MASK, val);
	return ret;
}

static int eta_set_safety_reg(struct eta6937 *bq, int volt, int curr)
{
	u8 ichg = 0;
	u8 vchg = 0;
	u8 val = 0;

	if(NULL == bq)
		return ERROR;
	ichg = (curr * bq->cfg.sensor_mohm / UNIT_100 -  ETA6937_MAX_ICHG_BASE) / ETA6937_MAX_ICHG_LSB;//current register map

	ichg <<= ETA6937_MAX_ICHG_SHIFT;

	vchg = (volt - ETA6937_MAX_VREG_BASE) / ETA6937_MAX_VREG_LSB;
	vchg <<= ETA6937_MAX_VREG_SHIFT;

	val = ichg | vchg;

	pr_info("Safety REG[6]:0x%02X\n", val);

	return eta_update_bits(bq, ETA6937_REG_06,
				ETA6937_MAX_VREG_MASK | ETA6937_MAX_ICHG_MASK, val);
}

static int eta_parse_dt(struct device *dev, struct eta6937 *bq)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	if(NULL == np || NULL == bq)
		return ERROR;
	bq->charge_enabled = !(of_property_read_bool(np, "ti,charging-disabled"));

	bq->cfg.enable_term = of_property_read_bool(np, "ti,eta6937,enable-term");

	ret = of_property_read_u32(np, "ti,eta6937,current-sensor-mohm",
					&bq->cfg.sensor_mohm);
	if (ret)
		return ret;

	if (bq->cfg.sensor_mohm == 0) {
		pr_info("invalid sensor resistor value, use 68mohm by default\n");
		bq->cfg.sensor_mohm = RESISTANCE;
	}
	pr_info("sensor_mohm:%d\n", bq->cfg.sensor_mohm);

	ret = of_property_read_u32(np, "ti,eta6937,charge-voltage",
					&bq->cfg.chg_mv);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,charge-current",
					&bq->cfg.chg_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,input-current-limit",
					&bq->cfg.icl_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,input-voltage-limit",
					&bq->cfg.ivl_mv);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,safety-max-charge-voltage",
					&bq->cfg.safety_chg_mv);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,safety-max-charge-current",
					&bq->cfg.safety_chg_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,eta6937,vbatlow-volt",
					&bq->cfg.batlow_mv);
	if (ret) {
		pr_err("Failed to get batlow_mv:%d\n", ret);
	}

	ret = of_property_read_u32(np, "ti,eta6937,term-current",
					&bq->cfg.iterm_ma);

	return ret;
}

static int eta_detect_device(struct eta6937 *bq)
{
	int ret = 0;
	u8 data = 0;

	if(NULL == bq)
		return ERROR;
	ret = eta_read_byte(bq, ETA6937_REG_03, &data);
	if (ret == 0) {
		bq->part_no = (data & ETA6937_REVISION_MASK) >> ETA6937_REVISION_SHIFT;
	}

	return ret;
}

static int eta_set_term_current(struct eta6937 *bq, int curr_ma)
{
	u8 ichg = 0;

	if(NULL == bq)
		return ERROR;
	ichg = (curr_ma * bq->cfg.sensor_mohm / UNIT_100 -  ETA6937_ITERM_BASE) / ETA6937_ITERM_LSB;

	ichg <<= ETA6937_ITERM_SHIFT;

	pr_info("iterm:%d\n", ichg);
	return eta_update_bits(bq, ETA6937_REG_04,
				ETA6937_ITERM_MASK, ichg);
}

static int eta_set_charge_profile(struct eta6937 *bq)
{
	int ret = 0;

	if(NULL == bq)
		return ERROR;
	pr_info("chg_mv:%d, chg_ma:%d, icl_ma:%d, ivl_mv:%d\n",
			bq->cfg.chg_mv, bq->cfg.chg_ma, bq->cfg.icl_ma, bq->cfg.ivl_mv);

	ret = eta_set_chargevoltage(bq->chg_dev, bq->cfg.chg_mv * UNIT_1000);
	if (ret < 0) {
		pr_err("Failed to set charge voltage:%d\n", ret);
		return ret;
	}

	ret = eta_set_chargecurrent(bq->chg_dev, bq->cfg.chg_ma * UNIT_1000);
	if (ret < 0) {
		pr_err("Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = eta_set_input_current_limit(bq->chg_dev, bq->cfg.icl_ma * UNIT_1000);
	if (ret < 0) {
		pr_err("Failed to set input current limit:%d\n", ret);
		return ret;
	}

	ret = EtaSetVdpm(bq->chg_dev, bq->cfg.ivl_mv * UNIT_1000);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
		return ret;
	}
	return 0;
}

static int eta6937_init_device(struct eta6937 *bq)
{
	int ret = 0;

	if(NULL == bq)
		return ERROR;
	bq->chg_mv = bq->cfg.chg_mv;
	bq->chg_ma = bq->cfg.chg_ma;
	bq->ivl_mv = bq->cfg.ivl_mv;
	bq->icl_ma = bq->cfg.icl_ma;


	/*safety register can only be written before other writes occur,
	  if lk code exist, it should write this register firstly before
	  write any other registers
	*/
	ret = eta_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0)
		pr_err("Failed to set safety register:%d\n", ret);

	ret = eta_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = eta_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);

	ret = eta_set_term_current(bq, bq->cfg.iterm_ma);
	if (ret < 0) {
		pr_err("Failed to set term current:%d,rc=%d\n",bq->cfg.iterm_ma,ret);
	}

	eta_set_charge_profile(bq);
	eta_set_low_chg(bq, 0);
	if (bq->charge_enabled) {
		ret = eta_enable_charger(bq);
	} else {
		ret = eta_disable_charger(bq);
	}

	return 0;
}

static void eta_dump_regs(struct eta6937 *bq)
{
	int ret = 0;
	u8 addr = 0;
	u8 val = 0;

	if(NULL == bq)
		return;
	for (addr = ADDR_FIRST; addr <= ADDR_ALL; addr++) {
		msleep(SECOND_2);
		ret = eta_read_byte(bq, addr, &val);
		if (!ret)
			pr_info("Reg[%02X] = 0x%02X\n", addr, val);
	}
}
/*
 *  Charger device interface
 */
static int eta_charging(struct charger_device *chg_dev, bool enable)
{
	struct eta6937 *bq = NULL;
	int ret = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if (enable) {
		ret = eta_enable_charger(bq);
	} else {
		ret = eta_disable_charger(bq);
	}

	ret = eta_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0) {
		pr_err("Failed to set safety register:%d\n", ret);
	}

	pr_info("%s charger\n", enable ? "enable" : "disable");
	bq->charge_enabled = enable;

	return 0;
}

static int eta6937_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct eta6937 *bq = NULL;

	if(NULL == chg_dev)
		return ERROR;

	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;
	ret = eta_charging(chg_dev, true);
	if (!ret)
		pr_info("plug in and enable charging:%d\n", ret);

	return ret;

}

static int eta_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct eta6937 *bq = NULL;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;
	ret = eta_charging(chg_dev, false);
	if (!ret)
		pr_err("plug out and disable charging:%d\n", ret);

	return ret;
}


static int eta_dump_register(struct charger_device *chg_dev)
{

	struct eta6937 *bq = NULL;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	eta_dump_regs(bq);

	return 0;
}
static int eta_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct eta6937 *bq = NULL;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;
	*en = bq->charge_enabled;

	return 0;
}
static int eta_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct eta6937 *bq = NULL;
	int ret = 0;
	u8 val = 0;

	if(NULL == chg_dev || NULL == done)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	ret = eta_read_byte(bq, ETA6937_REG_00, &val);
	if (!ret) {
		val = val & ETA6937_STAT_MASK;
		val = val >> ETA6937_STAT_SHIFT;
		*done = (val == ETA6937_STAT_CHGDONE);
	}

	return ret;
}

static int eta_set_chargecurrent(struct charger_device *chg_dev, u32 curr)
{
	struct eta6937 *bq = NULL;
	u8 ichg = 0;
	u8 addExtraCurr = 0;
	u8 baseCurr = 0;
	int ret = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;

	curr /= UNIT_1000; /*to mA */
	if(bq->part_no == ETA6937) {
		if(curr < CURRENT_550) {
			//set min current
			curr = CURRENT_550;
		} else if(curr > CURRENT_1950) {
			//set max current
			curr = CURRENT_1950;
		}

		if(curr < CURRENT_1350) {
			//current register map
			ichg = (curr - ETA6937_ICHG_OFFSET_LEVET0) / ETA6937_ICHG_OFFSET_STEP;
			addExtraCurr = ETA6937_ADD_CURR_800_DISABLE << ETA6937_ADD_CURR_800_SHIFT;
		} else {
			ichg = (curr - ETA6937_ICHG_OFFSET_LEVET0 - CURRENT_800) / ETA6937_ICHG_OFFSET_STEP;
			addExtraCurr = ETA6937_ADD_CURR_800_ENABLE << ETA6937_ADD_CURR_800_SHIFT;
		}

		ret = eta_update_bits(bq, ETA6937_REG_05, ETA6937_ADD_CURR_800_MASK, addExtraCurr);
		if (ret < 0) {
			pr_err("Failed to set ADD_CURR_800 ret:%d\n", ret);
		}
		pr_info("set:%d\n", ichg);
	} else {
		if(curr<CURRENT_680) {
			curr = CURRENT_680;//get min current
		}
		ichg = (curr * bq->cfg.sensor_mohm / UNIT_100 -  ETA6937_ICHG_BASE) / ETA6937_ICHG_LSB;//current register map
		pr_info("set:%d\n", ichg);
		if(ichg > MAX_CURREN_VALUE) {
			ichg = MAX_CURREN_VALUE;
		}
	}
	baseCurr = ETA6937_ICHG_OFFSET_LVL_EN << ETA6937_ICHG_OFFSET_SHIFT;
	ichg <<= ETA6937_ICHG_SHIFT;
	pr_info("Set Charge Current: curr(%d) val(0x%02X)...0x%02X\n", curr, ichg, baseCurr);

	return eta_update_bits(bq, ETA6937_REG_04,
		ETA6937_ICHG_MASK | ETA6937_ICHG_OFFSET_MASK, ichg | baseCurr);
}

static int eta_get_chargecurrent(struct charger_device *chg_dev, u32 *curr)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;
	int ret = 0;
	int ichg = 0;

	if(NULL == chg_dev || NULL == curr)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;
	if(!bq->cfg.sensor_mohm)
		return ERROR;
	ret = eta_read_byte(bq, ETA6937_REG_04, &val);
	if (!ret) {
		ichg = ((u32)(val & ETA6937_ICHG_MASK ) >> ETA6937_ICHG_SHIFT) * ETA6937_ICHG_LSB;
		ichg +=  ETA6937_ICHG_BASE;
		ichg = ichg * UNIT_100 / bq->cfg.sensor_mohm;
		*curr = ichg * UNIT_1000; /*to uA*/
	}

	return ret;
}

static int eta_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	int ret = 0;

	if(NULL == chg_dev || NULL == curr)
		return ERROR;
	*curr = MIN_CURRENT;
	pr_info("get_min_ichg\n");
	return ret;
}

static int eta_set_chargevoltage(struct charger_device *chg_dev, u32 volt)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	volt /= UNIT_1000; /*to mV*/
	val = (volt - ETA6937_VREG_BASE)/ETA6937_VREG_LSB;
	val <<= ETA6937_VREG_SHIFT;

	pr_info("Set Charge Voltage: volt(%d) val(0x%02X)\n", volt, volt);

	return eta_update_bits(bq, ETA6937_REG_02,
				ETA6937_VREG_MASK, val);
}

static int eta_get_chargevoltage(struct charger_device *chg_dev, u32 *cv)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;
	int ret = 0;
	int volt = 0;

	if(NULL == chg_dev || NULL == cv)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	ret = eta_read_byte(bq, ETA6937_REG_02, &val);
	if (!ret) {
		volt = val & ETA6937_VREG_MASK;
		volt = (volt >> ETA6937_VREG_SHIFT) * ETA6937_VREG_LSB;
		volt = volt + ETA6937_VREG_BASE;
		*cv = volt * UNIT_1000; /*to uV*/
	}

	return ret;
}

static int EtaSetVdpm(struct charger_device *chg_dev, u32 volt)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	volt /= UNIT_1000; /*to mV*/
	val = (volt - ETA6937_VSREG_BASE) / ETA6937_VSREG_LSB;
	val <<= ETA6937_VSREG_SHIFT;

	pr_info("Set Input Voltage Limit: volt(%d) val(0x%02X)\n", volt, val);

	return eta_update_bits(bq, ETA6937_REG_05,
				ETA6937_VSREG_MASK, val);
}

static int EtaSetExtraCurrentLimit(struct eta6937 *bq, u32 curr)
{
	u8 val = 0;
	u8 extraEn = 0;
	int ret = 0;

	if ((curr <= CURRENT_300) && (curr > 0)) {
		val = ETA6937_ILIMI_CURR300;//300mA
	} else if ((curr <= CURRENT_500) && (curr > CURRENT_300)) {
		val = ETA6937_ILIMI_CURR500;//500mA
	} else if ((curr <= CURRENT_800) && (curr > CURRENT_500)) {
		val = ETA6937_ILIMI_CURR800;//800mA
	} else if ((curr <= CURRENT_1200 ) && (curr > CURRENT_800)) {
		val = ETA6937_ILIMI_CURR1200;//1200mA
	} else {
		val = ETA6937_ILIMI_CURR1500;//1500mA
	}
	pr_info("Set Extra Input Current Limit: curr(%d) val(0x%02X)\n", curr, val);

	extraEn = EXTRA_ILIMT_ENABLE << ETA6937_EXTRA_ILIMT_EN_SHIFT;
	ret = eta_update_bits(bq, ETA6937_REG_07,
			ETA6937_EXTRA_ILIMT_EN_MASK | ETA6937_EXTRA_ILIMT_CHG_MASK,
			extraEn | val);
	return ret;
}

static int eta_set_input_current_limit(struct charger_device *chg_dev, u32 curr)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;
	u8 extraEn = 0;
	int ret = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	curr /= UNIT_1000;/*to mA*/
	if((bq->part_no == ETA6937) && (curr > CURRENT_500)) {
		return EtaSetExtraCurrentLimit(bq, curr);
	}
	if ((curr < CURRENT_500)&&(curr > 0))
		val = ETA6937_IINLIM_100MA;//100mA
	else if ((curr < CURRENT_550)&&(curr >= CURRENT_500))
		val = ETA6937_IINLIM_500MA;//500mA
	else if (curr <= CURRENT_800)
		val = ETA6937_IINLIM_800MA;//800mA
	else
		val = ETA6937_IINLIM_NOLIM;//no limit

	val <<= ETA6937_IINLIM_SHIFT;

	pr_info("Set Input Current Limit: curr(%d) val(0x%02X)\n", curr, val);

	extraEn = EXTRA_ILIMT_DISABLE << ETA6937_EXTRA_ILIMT_EN_SHIFT;
	ret = eta_update_bits(bq, ETA6937_REG_07,
				ETA6937_EXTRA_ILIMT_EN_MASK, extraEn);
	if (ret < 0) {
		pr_err("Failed to disabl EXTRA_ILIMT.ret=:%d \n", ret);
	}
	return eta_update_bits(bq, ETA6937_REG_01,
				ETA6937_IINLIM_MASK, val);
}

static int eta_get_input_current_limit(struct charger_device *chg_dev, u32 *curr)
{
	struct eta6937 *bq = NULL;
	u8 val = 0;
	int ret = 0;
	int ilim = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	ret = eta_read_byte(bq, ETA6937_REG_01, &val);
	if (!ret) {
		val = val & ETA6937_IINLIM_MASK;
		val = val >> ETA6937_IINLIM_SHIFT;
		if (val == ETA6937_IINLIM_100MA)
			ilim = CURRENT_100;//100mA
		else if (val == ETA6937_IINLIM_500MA)
			ilim = CURRENT_500;//500mA
		else if (val == ETA6937_IINLIM_800MA)
			ilim = CURRENT_800;//800mA
		else
			ilim = 0;

		*curr = ilim * UNIT_1000; /*to uA*/
	}

	return ret;
}

static int eta_enable_otg(struct charger_device *chg_dev, bool en);
static int eta_reset_watchdog_timer(struct charger_device *chg_dev)
{
	struct eta6937 *bq = NULL;
	u8 val = ETA6937_TMR_RST << ETA6937_TMR_RST_SHIFT;
	int ret = 0;
	u8 fault_reg = 0;
	static unsigned int chgPreTime = 0;
	static unsigned int chgNewTime = 0;
	unsigned int period = 0;

	if(NULL == chg_dev)
		return ERROR;

	bq = dev_get_drvdata(&chg_dev->dev);

	if(NULL == bq)
		return ERROR;

	ret = eta_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0) {
		pr_err("Failed to set safety register:%d\n", ret);
	}

	ret = eta_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0) {
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);
	}

	ret = eta_set_term_current(bq, bq->cfg.iterm_ma);
	if (ret < 0) {
		pr_err("Failed to set term current:%d,rc=%d\n",bq->cfg.iterm_ma,ret);
	}

	if(g_otg_En){
		ret = eta_read_byte(bq, ETA6937_REG_00, &fault_reg);
		if(!ret){
			fault_reg = fault_reg & ETA6937_FAULT_MASK;
			fault_reg = fault_reg >> ETA6937_FAULT_SHIFT;
		}
		if(fault_reg == ETA6937_OVER_LOAD) {
			eta_enable_otg(chg_dev, g_otg_En);
		}
	}
	if(bq->part_no == SY6923)
		return 0;
	chgNewTime = GetChargingTime();
	if (chgNewTime < chgPreTime) {
		chgNewTime = 0;
		chgPreTime = 0;
	}
	period = chgNewTime - chgPreTime;
	g_overTime = false;
	if(period > RESET_OVER_TIME) {
		chgPreTime = chgNewTime;
		g_overTime = true;
		pr_info("chgTime_enter\n");
		ret = eta_disable_charger(bq);
		if (ret < 0) {
			pr_err("Failed to set disable chg ret:%d\n", ret);
		}
		ret = eta_enable_charger(bq);
		if (ret < 0) {
			pr_err("Failed to set enable chg ret:%d\n", ret);
		}
	}
	return eta_update_bits(bq, ETA6937_REG_00,
				ETA6937_TMR_RST_MASK, val);
}

static int eta_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct eta6937 *bq = NULL;
	int ret = 0;
	u8 val = 0;
	u8 fault_reg = 0;
	u8 otg_pin = 0;
	u8 otg_en = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	pr_info("eta_otg_en:%d\n", en);
	g_otg_En = en;
	ret = eta_read_byte(bq, ETA6937_REG_00, &fault_reg);
	if(!ret) {
		pr_info("eta_read_status:0x%02X\n", fault_reg);
	}
	if(bq->part_no == SY6923) {
		if(en) {
			otg_pin = ETA6937_OTG_PL_LOW << ETA6937_OTG_PL_SHIFT;
			otg_en = ETA6937_OTG_ENABLE << ETA6937_OTG_EN_SHIFT;
			Otg_Start_Timer();
		} else {
			otg_pin = ETA6937_OTG_PL_HIGH << ETA6937_OTG_PL_SHIFT;
			otg_en = ETA6937_OTG_DISABLE << ETA6937_OTG_EN_SHIFT;
			hrtimer_cancel(&g_kickWdtTimer);
		}
		ret = eta_update_bits(bq, ETA6937_REG_02,
				ETA6937_OTG_PL_MASK | ETA6937_OTG_EN_MASK, otg_pin | otg_en);
	} else {
		if(en) {
			val = ETA6937_BOOST_MODE;
			Otg_Start_Timer();
		} else {
			val = ETA6937_CHARGER_MODE;
			hrtimer_cancel(&g_kickWdtTimer);
		}
		val <<= ETA6937_OPA_MODE_SHIFT;

		ret = eta_update_bits(bq, ETA6937_REG_01,
				ETA6937_OPA_MODE_MASK, val);
	}
	return ret;
}

static int eta_charger_do_event(struct charger_device *chg_dev, u32 event,
				u32 args)
{
	struct eta6937 *bq = NULL;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	if(NULL == bq)
		return ERROR;
	switch (event) {
	case EVENT_EOC:
		dev_info(bq->dev, "do eoc event\n");
		charger_dev_notify(bq->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		dev_info(bq->dev, "do recharge event\n");
		charger_dev_notify(bq->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

static int eta_enable_hz(struct charger_device *chg_dev, bool en)
{
	struct eta6937 *bq = NULL;
	int ret = 0;
	u8 val = 0;

	if(NULL == chg_dev)
		return ERROR;
	bq = dev_get_drvdata(&chg_dev->dev);
	pr_info("enable_hz:%d\n",en);
	if(en){
		g_hiz_status = ETA6937_HZ_MODE_ENABLE;
		val = ETA6937_HZ_MODE_ENABLE << ETA6937_HZ_MODE_SHIFT;
		ret = eta_update_bits(bq, ETA6937_REG_01,
			ETA6937_HZ_MODE_MASK, val);
	} else {
		g_hiz_status = ETA6937_HZ_MODE_DISABLE;
		val = ETA6937_HZ_MODE_DISABLE << ETA6937_HZ_MODE_SHIFT;
		ret = eta_update_bits(bq, ETA6937_REG_01,
			ETA6937_HZ_MODE_MASK, val);
	}
	eta_reset_watchdog_timer(chg_dev);

	return ret;
}

static struct charger_ops eta6937_chg_ops = {
	/* Normal charging */
	.plug_in = eta6937_plug_in,
	.plug_out = eta_plug_out,
	.dump_registers = eta_dump_register,
	.enable = eta_charging,
	.is_enabled = eta_is_charging_enable,
	.get_charging_current = eta_get_chargecurrent,
	.set_charging_current = eta_set_chargecurrent,
	.get_input_current = eta_get_input_current_limit,
	.set_input_current = eta_set_input_current_limit,
	.get_constant_voltage = eta_get_chargevoltage,
	.set_constant_voltage = eta_set_chargevoltage,
	.kick_wdt = eta_reset_watchdog_timer,
	.set_mivr = EtaSetVdpm,
	.is_charging_done = eta_is_charging_done,
	.get_min_charging_current = eta_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = NULL,
	.is_safety_timer_enabled = NULL,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = eta_enable_otg,
	.set_boost_current_limit = NULL,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.event = eta_charger_do_event,
	.enable_hz = eta_enable_hz,
};

void Wake_Up_Wdt(void)
{
    g_otgWdtTimeout = true;
    wake_up(&g_waitOtgWdtQue);
}

enum hrtimer_restart Otg_Hrtimer_Func(struct hrtimer *timer)
{
    Wake_Up_Wdt();
    return HRTIMER_NORESTART;
}

static void Kick_Wdt_Init_Timer(void)
{
    hrtimer_init(&g_kickWdtTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_kickWdtTimer.function = Otg_Hrtimer_Func;
}

static void Otg_Start_Timer(void)
{
    ktime_t ktime = ktime_set(WAIT_SECONDS, WAIT_NS);
    hrtimer_start(&g_kickWdtTimer, ktime, HRTIMER_MODE_REL);
}

static int Otg_Kick_Wdt_Thread(void *arg)
{

    while (1) {
        wait_event(g_waitOtgWdtQue, (g_otgWdtTimeout == true));
        g_otgWdtTimeout = false;
        eta_reset_watchdog_timer(g_chargerDev);
        Otg_Start_Timer();
    }
    return 0;
}

bool eta_rst_chg_en_flag = false;
static int eta6937_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct eta6937 *bq = NULL;
	int ret = 0;

	if(NULL == client || NULL == id)
		return ERROR;
	client->addr = I2C_ADDR;
	bq = devm_kzalloc(&client->dev, sizeof(struct eta6937), GFP_KERNEL);
	if (!bq) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->client = client;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	ret = eta_detect_device(bq);
	if (!ret && ((bq->part_no == ETA6937)||(bq->part_no == SY6923))) {
#ifdef CONFIG_HUAWEI_DEV_SELFCHECK
		set_hw_dev_detect_result(DEV_DETECT_CHARGER);
#endif
		pr_info("charger device detected, bq->part_no=%d\n",
							bq->part_no);
	} else {
		pr_info("charger device default, bq->part_no=%d\n",
							bq->part_no);
		bq->part_no = SY6923;
	}

	if(bq->part_no == SY6923) {
		ret = gpio_request(GPIO_CHG_EN_0, NULL);
		if (ret) {
			pr_err("Could not request GPIO171.\n");
		}
		ret = gpio_direction_output(GPIO_CHG_EN_0, GPIO_DIR_OUT);
		if (ret) {
			pr_err("Could not set  GPIO171 as output.\n");
		}
	}
	if (client->dev.of_node)
		eta_parse_dt(&client->dev, bq);

		/* Register charger device */
	bq->chg_dev = charger_device_register(
		"primary_chg", &client->dev, bq, &eta6937_chg_ops,
		&bq->chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		goto err_0;
	}
    g_chargerDev = bq->chg_dev;

	ret = eta6937_init_device(bq);
	if (ret) {
		pr_err("device init failure: %d\n", ret);
		goto err_0;
	}
    Kick_Wdt_Init_Timer();
    init_waitqueue_head(&g_waitOtgWdtQue);
    kthread_run(Otg_Kick_Wdt_Thread, NULL, "otg_wdt_thread");

	pr_info("charger driver probe successfully\n");

	return 0;

err_0:

	return ret;
}

static int eta6937_charger_remove(struct i2c_client *client)
{
	struct eta6937 *bq = i2c_get_clientdata(client);

	if(NULL == bq)
		return ERROR;
	if(bq->part_no == SY6923) {
		gpio_free(GPIO_CHG_EN_0);
	}
	mutex_destroy(&bq->i2c_rw_lock);

	return 0;
}


static void eta6937_charger_shutdown(struct i2c_client *client)
{
	pr_info("shutdown\n");
}

static struct of_device_id eta6937_charger_match_table[] = {
	{.compatible = "eta,eta6937"},
	{},
};


static const struct i2c_device_id eta6937_charger_id[] = {
	{ "eta6937", ETA6937 },
	{},
};

MODULE_DEVICE_TABLE(i2c, eta6937_charger_id);

static struct i2c_driver eta6937_charger_driver = {
	.driver		= {
		.name	= "eta6937",
		.of_match_table = eta6937_charger_match_table,
	},
	.id_table	= eta6937_charger_id,

	.probe		= eta6937_charger_probe,
	.remove		= eta6937_charger_remove,
	.shutdown   = eta6937_charger_shutdown,
};

module_i2c_driver(eta6937_charger_driver);

MODULE_DESCRIPTION("TI ETA6937 Charger Driver");
MODULE_LICENSE("GPL2");
MODULE_AUTHOR("Texas Instruments");

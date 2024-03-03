#include "lcd_kit_disp.h"
#include "lcd_kit_power.h"

uint32_t g_lcd_kit_gpio = 0;
uint32_t g_lcd_kit_vci = 0;
/********************************************************************
*power type
********************************************************************/
static struct gpio_desc gpio_req_cmds[] = {
	{
		DTYPE_GPIO_REQUEST, WAIT_TYPE_US, 0,
		GPIO_NAME, &g_lcd_kit_gpio, 0
	},
};

static struct gpio_desc gpio_free_cmds[] = {
	{
		DTYPE_GPIO_FREE, WAIT_TYPE_US, 0,
		GPIO_NAME, &g_lcd_kit_gpio, 0
	},
};

static struct gpio_desc gpio_high_cmds[] = {
	{
		DTYPE_GPIO_OUTPUT, WAIT_TYPE_US, 0,
		GPIO_NAME, &g_lcd_kit_gpio, 1
	},
};

static struct gpio_desc gpio_low_cmds[] = {
	{
		DTYPE_GPIO_OUTPUT, WAIT_TYPE_US, 0,
		GPIO_NAME, &g_lcd_kit_gpio, 0
	},
};

struct gpio_power_arra gpio_power[] = {
	{GPIO_REQ, ARRAY_SIZE(gpio_req_cmds), gpio_req_cmds},
	{GPIO_HIGH, ARRAY_SIZE(gpio_high_cmds), gpio_high_cmds},
	{GPIO_LOW, ARRAY_SIZE(gpio_low_cmds), gpio_low_cmds},
	{GPIO_RELEASE, ARRAY_SIZE(gpio_free_cmds), gpio_free_cmds},
};

static struct lcd_kit_vci_type lcd_kit_vci_map[] = {
	{LDO_17, DEVICE_LCDANALOG},
	{LDO_9, DEVICE_SDIO},
};

void lcd_kit_gpio_tx(uint32_t type, uint32_t op)
{
	int i = 0;
	struct gpio_power_arra* gpio_cm = NULL;

	switch (type) {
		case LCD_KIT_VCI:
			g_lcd_kit_gpio = power_hdl->lcd_vci.buf[1];
			break;
		case LCD_KIT_IOVCC:
			g_lcd_kit_gpio = power_hdl->lcd_iovcc.buf[1];
			break;
		case LCD_KIT_VSP:
			g_lcd_kit_gpio = power_hdl->lcd_vsp.buf[1];
			break;
		case LCD_KIT_VSN:
			g_lcd_kit_gpio = power_hdl->lcd_vsn.buf[1];
			break;
		case LCD_KIT_RST:
			g_lcd_kit_gpio = power_hdl->lcd_rst.buf[1];
			break;
		case LCD_KIT_BL:
			g_lcd_kit_gpio = power_hdl->lcd_backlight.buf[1];
			break;
		case LCD_KIT_TP_RST:
			g_lcd_kit_gpio = power_hdl->tp_rst.buf[1];
			break;
		default:
			LCD_KIT_ERR("not support type:%d\n", type);
			break;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_power); i++) {
		if (gpio_power[i].oper == op) {
			gpio_cm = &gpio_power[i];
			break;
		}
	}
	if (i >= ARRAY_SIZE(gpio_power)) {
		LCD_KIT_ERR("not found cm from gpio_power\n");
		return ;
	}
	if (gpio_cm->num > 0){
		gpio_cmds_tx(gpio_cm->cm, gpio_cm->num);
		return ;
	}
	else {
		LCD_KIT_INFO("gpio is not bigger than 0\n");
		return;
	}
}

int lcd_kit_pmu_ctrl(uint32_t type, uint32_t enable)
{
	struct pmu_operators* pmu_ops = NULL;

	pmu_ops = get_operators(PMU_MODULE_NAME_STR);
	if (!pmu_ops) {
		LCD_KIT_ERR("can not get pmu_ops!\n");
		return LCD_KIT_FAIL;
	}
	switch (type) {
		case LCD_KIT_VCI:
			if (enable) {
				pmu_ops->pmu_enable(g_lcd_kit_vci);
			} else {
				pmu_ops->pmu_disable(g_lcd_kit_vci);
			}
			break;
		case LCD_KIT_IOVCC:
			if (enable) {
				pmu_ops->pmu_enable(DEVICE_LCDIO);
			} else {
				pmu_ops->pmu_disable(DEVICE_LCDIO);
			}
			break;
		case LCD_KIT_VDD:
			if (enable) {
			#if defined(FASTBOOT_PMU_HI6421V700)
				pmu_ops->pmu_enable(DEVICE_LDO25);
			#endif
			} else {
			#if defined(FASTBOOT_PMU_HI6421V700)
				pmu_ops->pmu_disable(DEVICE_LDO25);
			#endif
			}
			break;
		default:
			break;
	}
	return LCD_KIT_OK;
}

int lcd_kit_charger_ctrl(uint32_t type, uint32_t enable)
{
	struct charger_power_operators* charger_ops = NULL;

	charger_ops = get_operators(CHARGER_POWER_MODULE_NAME_STR);
	if (!charger_ops) {
		LCD_KIT_ERR("can not get charger_power_ops!\n");
		return LCD_KIT_FAIL;
	}
	switch (type) {
		case LCD_KIT_VSP:
			if (enable) {
				charger_ops->charger_power_enable(DEVICE_LCD_BIAS);
				charger_ops->charger_power_enable(DEVICE_LCD_VSP);
			} else {
				charger_ops->charger_power_disable(DEVICE_LCD_VSP);
				charger_ops->charger_power_disable(DEVICE_LCD_BIAS);
			}
			break;
		case LCD_KIT_VSN:
			if (enable) {
				charger_ops->charger_power_enable(DEVICE_LCD_VSN);
			} else {
				charger_ops->charger_power_disable(DEVICE_LCD_VSN);
			}
			break;
		case LCD_KIT_BL:
			if (enable) {
				charger_ops->charger_power_enable(DEVICE_WLED);
			} else {
				charger_ops->charger_power_disable(DEVICE_WLED);
			}
			break;
		default:
			break;
	}
	return LCD_KIT_OK;
}

static uint32_t lcd_kit_choose_vci(uint32_t ldo_num)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lcd_kit_vci_map); i++) {
		if(lcd_kit_vci_map[i].ldo_num == ldo_num) {
			LCD_KIT_INFO("find ldo%d in lcd_kit_vci_map\n", lcd_kit_vci_map[i].ldo_num);
			return lcd_kit_vci_map[i].pmu_type;
		}
	}

	LCD_KIT_INFO("[%s] can not find ldo%d, use default\n", __func__, ldo_num);
	return DEVICE_LCDANALOG;
}

int lcd_kit_power_init(void)
{
	struct charger_power_operators* charger_ops = NULL;
	struct pmu_operators* pmu_ops = NULL;

	/*bias set*/
	if (power_hdl->lcd_vsp.buf && power_hdl->lcd_vsp.buf[0] == REGULATOR_MODE) {
		charger_ops = get_operators(CHARGER_POWER_MODULE_NAME_STR);
		if (!charger_ops) {
			LCD_KIT_ERR("can not get charger_power_ops!\n");
			return LCD_KIT_FAIL;
		}
		charger_ops->charger_power_set_voltage(DEVICE_LCD_BIAS, power_hdl->lcd_vsp.buf[2]);
		charger_ops->charger_power_set_voltage(DEVICE_LCD_VSP, power_hdl->lcd_vsp.buf[2]);
		charger_ops->charger_power_set_voltage(DEVICE_LCD_VSN, power_hdl->lcd_vsn.buf[2]);
	}
	/*vci set*/
	if (power_hdl->lcd_vci.buf && power_hdl->lcd_vci.buf[0] == REGULATOR_MODE) {
		/*judge vci is valid*/
		if (power_hdl->lcd_vci.buf[1] > 0) {
			g_lcd_kit_vci = lcd_kit_choose_vci(power_hdl->lcd_vci.buf[1]);
			pmu_ops = get_operators(PMU_MODULE_NAME_STR);
			if (!pmu_ops) {
				LCD_KIT_ERR("can not get pmu_ops!\n");
				return LCD_KIT_FAIL;
			}
			pmu_ops->pmu_set_voltage(g_lcd_kit_vci, power_hdl->lcd_vci.buf[2]);
		}
	}
	/*iovcc set*/
	if (power_hdl->lcd_iovcc.buf && power_hdl->lcd_iovcc.buf[0] == REGULATOR_MODE) {
		/*judge iovcc is valid*/
		if (power_hdl->lcd_iovcc.buf[1] > 0) {
			pmu_ops = get_operators(PMU_MODULE_NAME_STR);
			if (!pmu_ops) {
				LCD_KIT_ERR("can not get pmu_ops!\n");
				return LCD_KIT_FAIL;
			}
			pmu_ops->pmu_set_voltage(DEVICE_LCDIO, power_hdl->lcd_iovcc.buf[2]);
		}
	}
	/*vdd set*/
	if (power_hdl->lcd_vdd.buf && power_hdl->lcd_vdd.buf[0] == REGULATOR_MODE) {
		/*judge vdd is valid*/
		if (power_hdl->lcd_vdd.buf[1] > 0) {
			pmu_ops = get_operators(PMU_MODULE_NAME_STR);
			if (!pmu_ops) {
				LCD_KIT_ERR("can not get pmu_ops!\n");
				return LCD_KIT_FAIL;
			}
		#if defined(FASTBOOT_PMU_HI6421V700)
			pmu_ops->pmu_set_voltage(DEVICE_LDO25, power_hdl->lcd_vdd.buf[2]);
		#endif
		}
	}
	return LCD_KIT_OK;
}


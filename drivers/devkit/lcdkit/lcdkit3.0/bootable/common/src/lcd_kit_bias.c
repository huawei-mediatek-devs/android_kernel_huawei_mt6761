#include "lcd_kit_bias.h"
#include "lcd_kit_common.h"

static struct lcd_kit_bias_ops *g_bias_ops = NULL;
static struct lcd_kit_bias_recognize bias_recognize[MAX_BIAS_NUM];
int lcd_kit_bias_register(struct lcd_kit_bias_ops* ops)
{
	if (g_bias_ops) {
		LCD_KIT_ERR("g_bias_ops has already been registered!\n");
		return LCD_KIT_FAIL;
	}
	g_bias_ops = ops;
	return LCD_KIT_OK;
}

int lcd_kit_bias_unregister(struct lcd_kit_bias_ops* ops)
{
	if (g_bias_ops == ops) {
		g_bias_ops = NULL;
		return LCD_KIT_OK;
	}
	LCD_KIT_ERR("g_bias_ops unregister fail!\n");
	return LCD_KIT_FAIL;
}

struct lcd_kit_bias_ops* lcd_kit_get_bias_ops(void)
{
	return g_bias_ops;
}

void lcd_kit_dts_set_bias_status(char *name)
{
	struct lcd_kit_adapt_ops* adapt_ops = NULL;
	int ret = 0;

	adapt_ops = lcd_kit_get_adapt_ops();
	if (!adapt_ops) {
		LCD_KIT_ERR("can not get adapt_ops!\n");
		return;
	}
	if (adapt_ops->change_dts_status_by_compatible){
		adapt_ops->change_dts_status_by_compatible(name);
	}
}

int lcd_kit_bias_recognize_register(int (*func)(void))
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < MAX_BIAS_NUM; i++){
		if (bias_recognize[i].used == 0){
			bias_recognize[i].bias_ic_recognize = func;
			bias_recognize[i].used = 1;
			break;
		}
	}
	if (i >= MAX_BIAS_NUM) {
		ret = LCD_KIT_FAIL;
		LCD_KIT_ERR("bias recognize register not success.\n");
	}
	return ret;
}

int lcd_kit_bias_recognize_call(int (*func)(void))
{
	int ret = 0;
	int i = 0;
	for (i = 0; i < MAX_BIAS_NUM; i++){
		if (bias_recognize[i].used == 1){
			ret = bias_recognize[i].bias_ic_recognize();
		}
		if(ret == LCD_KIT_OK){
		LCD_KIT_INFO("bias recognize call success.\n");
		break;
		}
	}
	if (i >= MAX_BIAS_NUM) {
		ret = LCD_KIT_FAIL;
		LCD_KIT_INFO("bias recognize call fail.\n");
	}
	return ret;
}

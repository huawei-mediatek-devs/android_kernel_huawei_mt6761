#include "lcd_kit_bl.h"
#include "lcd_kit_common.h"

static struct lcd_kit_bl_ops *g_bl_ops = NULL;
static struct lcd_kit_bl_recognize backlight_recognize[MAX_BACKLIGHT_IC_NUM];
int lcd_kit_bl_register(struct lcd_kit_bl_ops* ops)
{
	if (g_bl_ops) {
		LCD_KIT_ERR("g_bl_ops has already been registered!\n");
		return LCD_KIT_FAIL;
	}
	g_bl_ops = ops;
	return LCD_KIT_OK;
}

int lcd_kit_bl_unregister(struct lcd_kit_bl_ops* ops)
{
	if (g_bl_ops == ops) {
		g_bl_ops = NULL;
		return LCD_KIT_OK;
	}
	LCD_KIT_ERR("g_bl_ops unregister fail!\n");
	return LCD_KIT_FAIL;
}

struct lcd_kit_bl_ops* lcd_kit_get_bl_ops(void)
{
	return g_bl_ops;
}

int lcd_kit_backlight_recognize_register(int (*func)(void))
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < MAX_BACKLIGHT_IC_NUM; i++){
		if (backlight_recognize[i].used == 0){
			backlight_recognize[i].backlight_ic_recognize = func;
			backlight_recognize[i].used = 1;
			break;
		}
	}
	if (i >= MAX_BACKLIGHT_IC_NUM) {
		ret = LCD_KIT_FAIL;
		LCD_KIT_ERR("backlight recognize register not success.\n");
	}
	return ret;
}

int lcd_kit_backlight_recognize_call(int (*func)(void))
{
	int ret = 0;
	int i = 0;
	for (i = 0; i < MAX_BACKLIGHT_IC_NUM; i++){
		if (backlight_recognize[i].used == 1){
			ret = backlight_recognize[i].backlight_ic_recognize();
		}
		if(ret == LCD_KIT_OK){
		LCD_KIT_INFO("backlight recognize call success.\n");
		break;
		}
	}
	if (i >= MAX_BACKLIGHT_IC_NUM) {
		ret = LCD_KIT_FAIL;
		LCD_KIT_ERR("backlight recognize call fail.\n");
	}
	return ret;
}

#ifndef _LCD_KIT_BL_H_
#define _LCD_KIT_BL_H_
#include <types.h>
#include <stdarg.h>

#define MAX_BACKLIGHT_IC_NUM 10

struct lcd_kit_bl_ops {
	int (*set_backlight)(unsigned int level);
};

struct lcd_kit_bl_recognize {
	int used;
	int (*backlight_ic_recognize)(void);
};

/*function declare*/
struct lcd_kit_bl_ops* lcd_kit_get_bl_ops(void);
int lcd_kit_bl_register(struct lcd_kit_bl_ops* ops);
int lcd_kit_bl_unregister(struct lcd_kit_bl_ops* ops);
#endif

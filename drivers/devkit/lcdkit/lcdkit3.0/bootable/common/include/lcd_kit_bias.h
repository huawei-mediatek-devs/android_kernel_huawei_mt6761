#ifndef _LCD_KIT_BIAS_H_
#define _LCD_KIT_BIAS_H_
#include <types.h>
#include <stdarg.h>

#define MAX_BIAS_NUM 10

struct lcd_kit_bias_ops {
	int (*set_bias_voltage)(int vpos, int vneg);
};

/*function declare*/
struct lcd_kit_bias_ops* lcd_kit_get_bias_ops(void);
int lcd_kit_bias_register(struct lcd_kit_bias_ops* ops);
int lcd_kit_bias_unregister(struct lcd_kit_bias_ops* ops);
void lcdkit_dts_set_bias_status(char *name);

struct lcd_kit_bias_recognize {
	int used;
	int (*bias_ic_recognize)(void);
};

#endif


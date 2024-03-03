#ifndef _LCD_KIT_ADAPT_H_
#define _LCD_KIT_ADAPT_H_

#include <stdio.h>

#define DTS_COMP_LCD_PANEL_TYPE 	"huawei,lcd_panel_type"
#define DTS_LCD_PANEL_TYPE 	"/huawei,lcd_panel"
#define LCD_TYPE_NAME_MAX		30
#define MIPI_MODE_LP		1
#define MIPI_MODE_HS		0

/*print log*/
extern u32 g_lcd_kit_msg_level;

enum mtk_lcd_type {
    LCDKIT,  /*for lcdkit*/
 	LCD_KIT,  /*for lcdkit3.0*/
 	UNKNOWN_LCD,
};

typedef struct
{
	enum mtk_lcd_type lcd_type;
	unsigned char lcd_name[20];
} LCD_TYPE_INFO;

#define LCD_KIT_ERR(msg, ...)    \
	if (g_lcd_kit_msg_level >= MSG_LEVEL_ERROR)  \
		dprintf(CRITICAL, "[LCD_KIT/E]%s: "msg, __func__, ## __VA_ARGS__)
#define LCD_KIT_WARNING(msg, ...)    \
	if (g_lcd_kit_msg_level >= MSG_LEVEL_WARNING)	\
		dprintf(SPEW, "[LCD_KIT/W]%s: "msg, __func__, ## __VA_ARGS__)
#define LCD_KIT_INFO(msg, ...)    \
	if (g_lcd_kit_msg_level >= MSG_LEVEL_INFO)	\
		dprintf(SPEW, "[LCD_KIT/I]%s: "msg, __func__, ## __VA_ARGS__)
#define LCD_KIT_DEBUG(msg, ...)    \
	if (g_lcd_kit_msg_level >= MSG_LEVEL_DEBUG) \
		dprintf(SPEW, "[LCD_KIT/D]%s: "msg, __func__, ## __VA_ARGS__)

extern int lcdkit_get_lcd_id(void);
extern int lcd_kit_get_product_id(void);
extern void lcdkit_set_lcd_panel_type(char *type);
extern void lcdkit_set_lcd_panel_status(char *lcd_name);
extern int lcd_kit_get_lcd_type(void);
extern void lcd_kit_set_lcd_name_to_no_lcd(void);
extern void lcd_kit_get_lcdname(void);
extern int lcd_kit_get_detect_type(void);
extern void lcdkit_set_lcd_ddic_max_brightness (unsigned int bl_val);
/*function declare*/
#endif

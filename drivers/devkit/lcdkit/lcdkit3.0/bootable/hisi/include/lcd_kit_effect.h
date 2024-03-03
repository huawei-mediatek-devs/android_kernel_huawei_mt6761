#ifndef _LCD_KIT_EFFECT_H_
#define _LCD_KIT_EFFECT_H_

#include <sys.h>
#include <boot.h>
#include <oeminfo_ops.h>
#include <global_ddr_map.h>
#include "hisi_fb.h"

#define LCD_KIT_COLOR_INFO_SIZE 8
#define LCD_KIT_SERIAL_INFO_SIZE 16
#define OEMINFO_GAMMA_DATA 114    /*OEMINFO ID used for gamma data */
#define OEMINFO_GAMMA_LEN  115    /*OEMINFO ID used fir gamma data len */

/*
 *1542 = gamma_r + gamma_g + gamma_b = (257 + 257 + 257) * sizeof(u16);
 *1542 = degamma_r + degamma_g + degamma_b = (257 + 257 +257) * sizeof(u16);
*/
#define GM_IGM_LEN (1542 + 1542)
#define GM_LUT_LEN 257

struct panelid {
	uint32_t modulesn;
	uint32_t equipid;
	uint32_t modulemanufactdate;
	uint32_t vendorid;
};

struct coloruniformparams {
	uint32_t c_lmt[3];
	uint32_t mxcc_matrix[3][3];
	uint32_t white_decay_luminace;
};

struct colormeasuredata {
	uint32_t chroma_coordinates[4][2];
	uint32_t white_luminance;
};

struct lcdbrightnesscoloroeminfo {
	uint32_t id_flag;
	uint32_t tc_flag;
	struct panelid  panel_id;
	struct coloruniformparams color_params;
	struct colormeasuredata color_mdata;
};

struct lcd_kit_brightness_color_uniform {
	uint32_t support;
	struct lcd_kit_dsi_panel_cmds modulesn_cmds;
	struct lcd_kit_dsi_panel_cmds equipid_cmds;
	struct lcd_kit_dsi_panel_cmds modulemanufact_cmds;
	struct lcd_kit_dsi_panel_cmds vendorid_cmds;
};

void lcd_bright_rgbw_id_from_oeminfo_process(struct hisi_fb_data_type* phisifd);
int lcd_kit_write_gm_to_reserved_mem();
void lcd_kit_bright_rgbw_id_from_oeminfo_process(struct hisi_fb_data_type* hisifd);
#endif

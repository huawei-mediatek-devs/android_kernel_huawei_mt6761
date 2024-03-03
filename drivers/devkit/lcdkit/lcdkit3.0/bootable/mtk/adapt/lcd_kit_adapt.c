#include "lcd_kit_disp.h"
#include "lcd_kit_power.h"
#include "lcd_kit_common.h"
#include "lcd_kit_adapt.h"
#include "lcm_drv.h"
#include <libfdt.h>
extern void *g_fdt;

static int lcd_id_cur_status[LCD_TYPE_NAME_MAX] = {0};
char lcd_type_buf[LCD_TYPE_NAME_MAX];

static LCD_TYPE_INFO lcm_info_list[] =
{
	{LCDKIT, "LCDKIT"},
	{LCD_KIT, "LCD_KIT"},//lcdkit3.0
};

extern LCM_UTIL_FUNCS lcm_util_mtk;

#define mipi_dsi_cmds_tx(cmdq, cmds) \
		lcm_util_mtk.mipi_dsi_cmds_tx(cmdq, cmds)

#define mipi_dsi_cmds_rx(out, cmds, len) \
		lcm_util_mtk.mipi_dsi_cmds_rx(out, cmds, len)

static int lcd_kit_get_data_by_property(const char* compatible, const char* propertyname, int **data, int *len)
{
	int offset = 0;
    struct fdt_property *property = NULL;

	if (!compatible || !propertyname || !data || !len) {
		LCD_KIT_ERR("domain_name, item_name or value is NULL!\n");
		return -1;
	}

	offset = fdt_node_offset_by_compatible(g_fdt, -1, compatible);
	if (offset < 0) {
		LCD_KIT_INFO("can not find %s node by compatible\n",
			compatible);
		return -1;
	}

    property = fdt_get_property(g_fdt, offset, propertyname, len);
	if(!property){
		LCD_KIT_INFO("can not find %s\n", propertyname);
		return -1;
	}

	if (!property->data) {
		return LCD_KIT_FAIL;
	}
	*data = property->data;
	return LCD_KIT_OK;
}

static int get_dts_property(const char *compatible, const char *propertyname, const uint32_t **data, int *length)
{
	int offset = 0;
    int len;
    struct fdt_property *property = NULL;

	if (!compatible || !propertyname || !data || !length) {
		LCD_KIT_ERR("domain_name, item_name or value is NULL!\n");
		return -1;
	}

	offset = fdt_node_offset_by_compatible(g_fdt, -1, compatible);
    if(offset < 0){
        LCD_KIT_ERR("-----can not find %s node by compatible \n",compatible);
        return -1;
    }

    property = fdt_get_property(g_fdt, offset, propertyname, &len);
	if(!property){
		LCD_KIT_ERR("-----can not find %s \n", propertyname);
		return -1;
	}

	*data= property->data;
	*length = len;

    return 0;
}

int lcd_kit_get_detect_type(void)
{
	int type = 0;
	int ret = 0;

    ret = lcd_kit_parse_get_u32_default(DTS_COMP_LCD_PANEL_TYPE, "detect_type", &type, 0);
    if (ret < 0)
    {
        return -1;
    }

	LCD_KIT_INFO("LCD panel detect type = %d\n", type);
	return type;
}

void lcd_kit_get_lcdname(void)
{
	int type = 0;
	int offset = 0;
    int len;
    struct fdt_property *property = NULL;

	offset = fdt_node_offset_by_compatible(g_fdt, -1, DTS_COMP_LCD_PANEL_TYPE);
    if(offset < 0){
        LCD_KIT_ERR("-----can not find %s node by compatible \n",DTS_COMP_LCD_PANEL_TYPE);
        return -1;
    }

    property = fdt_get_property(g_fdt, offset, "support_lcd_type", &len);
	if(!property){
		LCD_KIT_ERR("-----can not find support_lcd_type \n");
		return -1;
	}

	memset(lcd_type_buf, 0, LCD_TYPE_NAME_MAX);
    memcpy(lcd_type_buf, (char *)property->data, (unsigned int)strlen((char *)property->data) + 1);

	return;
}

int lcd_kit_get_lcd_type(void)
{
	int i;
	int length = sizeof(lcm_info_list)/sizeof(LCD_TYPE_INFO);

	for(i = 0; i < length; i++) {
		if (0 == strncmp(lcd_type_buf, (char *)lcm_info_list[i].lcd_name, strlen((char *)(lcm_info_list[i].lcd_name)))) {
			return lcm_info_list[i].lcd_type;
		}
	}

	return UNKNOWN_LCD;
}
void lcd_kit_set_lcd_name_to_no_lcd(void)
{
	memcpy(lcd_type_buf, "NO_LCD", 7);
	return;
}

static int get_dts_u32_index(const char *compatible, const char *propertyname, uint32_t index, uint32_t *out_val)
{
	int ret;
	int len;
	const uint32_t *data;
	uint32_t retTmp;
	struct fdt_operators *fdt_ops = NULL;

	if (!compatible || !propertyname || !out_val) {
		LCD_KIT_ERR("domain_name, item_name or value is NULL!\n");
		return -1;
	}

	ret = get_dts_property(compatible, propertyname, &data, &len);
	if ((ret < 0) || (len < 0))
		return ret;

	if ((index * sizeof(uint32_t)) >= (uint32_t)len) {
		LCD_KIT_ERR("out of range!\n");
		return -1;
	}

	retTmp = *(data + index);
	retTmp = fdt32_to_cpu(retTmp);
	*out_val = retTmp;

	return 0;
}

int lcdkit_get_lcd_id(void)
{
	uint32_t lcdid_count = 0;
	int ret;
	uint32_t i;
	uint32_t gpio_id = 0;
	int lcd_id_status = 0;
	int lcd_id_up=0,lcd_id_down=0;
	const int lcd_nc_value = 2;
	uint32_t* dts_data_p = NULL;
	int dts_data_len = 0;

	ret = get_dts_property(DTS_COMP_LCD_PANEL_TYPE, "gpio_id", (const uint32_t**)&dts_data_p, &dts_data_len);
	if (ret < 0) {
		LCD_KIT_ERR("get id dts failed, or excess maximum support id pins number!\n");
		return -1;
	}

	lcdid_count = dts_data_len / 4;
	for (i = 0; i < lcdid_count; i++) {
		ret = get_dts_u32_index(DTS_COMP_LCD_PANEL_TYPE, "gpio_id", i, &gpio_id);
    	mt_set_gpio_mode(gpio_id, GPIO_MODE_00);
    	mt_set_gpio_dir(gpio_id, GPIO_DIR_IN);
    	mt_set_gpio_pull_enable(gpio_id, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(gpio_id,GPIO_PULL_UP);
    	mdelay(10);
    	lcd_id_up = mt_get_gpio_in(gpio_id);

    	mt_set_gpio_pull_select(gpio_id,GPIO_PULL_DOWN);
    	mdelay(10);
    	lcd_id_down = mt_get_gpio_in(gpio_id);
    	mt_set_gpio_pull_select(gpio_id,GPIO_PULL_DISABLE);
        if(lcd_id_up == lcd_id_down)
    		lcd_id_cur_status[i] = lcd_id_up;
    	else
    		lcd_id_cur_status[i] = lcd_nc_value;

		lcd_id_status |= (lcd_id_cur_status[i] << (2 * i));
    	}

    	LCD_KIT_INFO("[uboot]:%s ,lcd_id_status=%d.\n",__func__,lcd_id_status);
    return lcd_id_status;
}

int lcd_kit_get_product_id(void)
{
	int ret = 0;
    int product_id = NULL;

    ret = lcd_kit_parse_get_u32_default(DTS_COMP_LCD_PANEL_TYPE, "product_id", &product_id, 0);
    if (ret < 0)
    {
		product_id = 1000;
    }

	return product_id;
}

void lcdkit_set_lcd_panel_type (char *type)
{
	int ret = 0;
	int offset = 0;

	if (!type) {
		LCD_KIT_ERR("type is NULL!\n");
		return;
	}

	offset = fdt_path_offset(g_fdt, DTS_LCD_PANEL_TYPE);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find panel node, change fb dts failed\n");
		return;
	}

	ret = fdt_setprop_string(g_fdt, offset, (const char*)"lcd_panel_type", (const void*)type);
	if (ret) {
		LCD_KIT_ERR("Cannot update lcd panel type(errno=%d)!\n", ret);
	}
	return;
}


void lcdkit_set_lcd_ddic_max_brightness (unsigned int bl_val)
{
	int ret = 0;
	int offset = 0;

	offset = fdt_path_offset(g_fdt, DTS_LCD_PANEL_TYPE);
	if (offset < 0) {
		LCD_KIT_ERR("Set max brightness Could not find huawei,lcd_panel node, change fb dts failed\n");
		return;
	}

	ret = fdt_setprop_cell(g_fdt, offset, (const char*)"panel_ddic_max_brightness", bl_val);
	if (ret) {
		LCD_KIT_ERR("Cannot update lcd ddic max brightness(errno=%d)!\n", ret);
	}
	return;
}

void lcdkit_set_lcd_panel_status (char *lcd_name)
{
	int ret = 0;
	int offset = 0;

	if (!lcd_name) {
		LCD_KIT_ERR("type is NULL!\n");
		return;
	}

	offset = fdt_path_offset(g_fdt, lcd_name);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find panel node, change fb dts failed\n");
		return;
	}

	ret = fdt_setprop_string(g_fdt, offset, (const char*)"status", (const void*)"ok");
	if (ret) {
		LCD_KIT_ERR("Cannot update lcd panel type(errno=%d)!\n", ret);
		return;
	}

	LCD_KIT_INFO("lcdkit_set_lcd_panel_status OK!\n");
	return;
}

static int lcd_kit_cmds_to_mtk_dsi_cmds(struct lcd_kit_dsi_cmd_desc* lcd_kit_cmds, struct dsi_cmd_desc* cmd)
{
	if (lcd_kit_cmds == NULL) {
		LCD_KIT_ERR("lcd_kit_cmds is null point!\n");
		return LCD_KIT_FAIL;
	}
	if (cmd == NULL) {
		LCD_KIT_ERR("cmd is null point!\n");
		return LCD_KIT_FAIL;
	}

	cmd->dtype = lcd_kit_cmds->payload[0];
	cmd->vc =  lcd_kit_cmds->vc;
	cmd->dlen =  (lcd_kit_cmds->dlen - 1);
	cmd->payload = &lcd_kit_cmds->payload[1];
    cmd->link_state = MIPI_MODE_LP;

	return LCD_KIT_OK;
}

static int lcd_kit_cmds_to_mtk_dsi_read_cmds(struct lcd_kit_dsi_cmd_desc* lcd_kit_cmds, struct dsi_cmd_desc* cmd)
{
	if (lcd_kit_cmds == NULL) {
		LCD_KIT_ERR("lcd_kit_cmds is null point!\n");
		return LCD_KIT_FAIL;
	}
	if (cmd == NULL) {
		LCD_KIT_ERR("cmd is null point!\n");
		return LCD_KIT_FAIL;
	}

	cmd->dtype = lcd_kit_cmds->payload[0];
	cmd->vc =  lcd_kit_cmds->vc;
	cmd->dlen =  lcd_kit_cmds->dlen;
    cmd->link_state = MIPI_MODE_LP;

	return LCD_KIT_OK;
}

static int mtk_mipi_dsi_cmds_tx(struct lcd_kit_dsi_cmd_desc *cmds, int cnt)
{
	struct lcd_kit_dsi_cmd_desc *cm = cmds;
	struct dsi_cmd_desc dsi_cmd;

	int i = 0;

	if (NULL == cmds) {
		LCD_KIT_ERR("cmds is NULL");
		return -1;
	}

	for (i = 0; i < cnt; i++) {
    	lcd_kit_cmds_to_mtk_dsi_cmds(cm, &dsi_cmd);
		(void)mipi_dsi_cmds_tx(NULL, &dsi_cmd);

		if (cm->wait) {
			if (cm->waittype == WAIT_TYPE_US)
				udelay(cm->wait);
			else if (cm->waittype == WAIT_TYPE_MS) {
				mdelay(cm->wait);
			}
			else
				mdelay(cm->wait * 1000);
		}
		cm++;
	}

	return cnt;
}
static int lcd_kit_cmd_is_write(struct lcd_kit_dsi_cmd_desc* cmd)
{
	int ret = LCD_KIT_FAIL;

	if (!cmd) {
		LCD_KIT_ERR("cmd is NULL!\n");
		return -1;
	}

	switch (cmd->dtype) {
		case DTYPE_GEN_WRITE:
		case DTYPE_GEN_WRITE1:
		case DTYPE_GEN_WRITE2:
		case DTYPE_GEN_LWRITE:
		case DTYPE_DCS_WRITE:
		case DTYPE_DCS_WRITE1:
		case DTYPE_DCS_LWRITE:
		case DTYPE_DSC_LWRITE:
			ret = 1;
			break;
		case DTYPE_GEN_READ:
		case DTYPE_GEN_READ1:
		case DTYPE_GEN_READ2:
		case DTYPE_DCS_READ:
			ret = 0;
			break;
		default:
			ret = -1;
			break;
	}
	return ret;
}

/*
 *  dsi send cmds
*/
int lcd_kit_dsi_cmds_tx(void* hld, struct lcd_kit_dsi_panel_cmds* cmds)
{
    int i;

	if (!cmds) {
		LCD_KIT_ERR("cmd is NULL!\n");
		return -1;
	}

	for (i = 0; i < cmds->cmd_cnt; i++) {
		mtk_mipi_dsi_cmds_tx(&cmds->cmds[i], 1);
	}

	return 0;
}

/*
 *  dsi receive cmds
*/
int lcd_kit_dsi_cmds_rx(void *hld, uint8_t *out, struct lcd_kit_dsi_panel_cmds *cmds)
{
	int i = 0;
	int ret = 0;
	struct dsi_cmd_desc dsi_cmd;

	if (!cmds || !out) {
		LCD_KIT_ERR("cmds or out is NULL!\n");
		return LCD_KIT_FAIL;
	}

	for (i = 0; i < cmds->cmd_cnt; i++) {
		if (lcd_kit_cmd_is_write(&cmds->cmds[i])) {
			lcd_kit_cmds_to_mtk_dsi_cmds(&cmds->cmds[i], &dsi_cmd);
			ret = mtk_mipi_dsi_cmds_tx(&cmds->cmds[i], 1);
			if (ret < 0) {
				LCD_KIT_ERR("mipi cmd tx failed!\n");
				return LCD_KIT_FAIL;
			}
		} else {
			lcd_kit_cmds_to_mtk_dsi_read_cmds(&cmds->cmds[i], &dsi_cmd);
			ret = mipi_dsi_cmds_rx(out, &dsi_cmd, dsi_cmd.dlen);
			if (ret == 0) {
				LCD_KIT_ERR("mipi cmd rx failed!\n");
				return LCD_KIT_FAIL;
			}
		}
	}

	return LCD_KIT_OK;
}

static int lcd_kit_buf_trans(const char* inbuf, int inlen, char** outbuf, int* outlen)
{
	char* buf;
	int i;
	int bufsize = inlen;

	if (!inbuf || !outbuf || !outlen) {
		LCD_KIT_ERR("inbuf is null point!\n");
		return LCD_KIT_FAIL;
	}
	/*The property is 4bytes long per element in cells: <>*/
	bufsize = bufsize / 4;
	/*If use bype property: [], this division should be removed*/
	buf = malloc(sizeof(char) * bufsize);
	if (!buf) {
		LCD_KIT_ERR("buf is null point!\n");
		return LCD_KIT_FAIL;
	}
	//For use cells property: <>
	for (i = 0; i < bufsize; i++) {
		buf[i] = inbuf[i * 4 + 3];
	}
	*outbuf = buf;
	*outlen = bufsize;
	return LCD_KIT_OK;
}


static int lcd_kit_gpio_enable(u32 type)
{
	lcd_kit_gpio_tx(type, GPIO_REQ);
	lcd_kit_gpio_tx(type, GPIO_HIGH);
	return LCD_KIT_OK;
}

static int lcd_kit_gpio_disable(u32 type)
{
	lcd_kit_gpio_tx(type, GPIO_LOW);
	lcd_kit_gpio_tx(type, GPIO_RELEASE);
	return LCD_KIT_OK;
}

static int lcd_kit_regulator_enable(u32 type)
{
	int ret = LCD_KIT_OK;

	switch(type)
	{
		case LCD_KIT_VCI:
		case LCD_KIT_IOVCC:
		case LCD_KIT_VDD:
			ret = lcd_kit_pmu_ctrl(type, 1);
			break;
		case LCD_KIT_VSP:
		case LCD_KIT_VSN:
		case LCD_KIT_BL:
			ret = lcd_kit_charger_ctrl(type, 1);
			break;
		default:
			ret = LCD_KIT_FAIL;
			LCD_KIT_ERR("regulator type:%d not support\n", type);
			break;
	}
	return ret;
}

static int lcd_kit_regulator_disable(u32 type)
{
	int ret = LCD_KIT_OK;

	switch(type)
	{
		case LCD_KIT_VCI:
		case LCD_KIT_IOVCC:
			ret = lcd_kit_pmu_ctrl(type, 0);
			break;
		case LCD_KIT_VSP:
		case LCD_KIT_VSN:
		case LCD_KIT_BL:
			ret = lcd_kit_charger_ctrl(type, 0);
			break;
		default:
			LCD_KIT_ERR("regulator type:%d not support\n", type);
			break;
	}
	return ret;
}

struct lcd_kit_adapt_ops adapt_ops = {
	.mipi_tx = lcd_kit_dsi_cmds_tx,
	.mipi_rx = lcd_kit_dsi_cmds_rx,
	.gpio_enable = lcd_kit_gpio_enable,
	.gpio_disable = lcd_kit_gpio_disable,
	.regulator_enable = lcd_kit_regulator_enable,
	.regulator_disable = lcd_kit_regulator_disable,
	.buf_trans = lcd_kit_buf_trans,
	.get_data_by_property = lcd_kit_get_data_by_property,
};

int lcd_kit_adapt_init(void)
{
	int ret = LCD_KIT_OK;
	ret = lcd_kit_adapt_register(&adapt_ops);
	return ret;
}

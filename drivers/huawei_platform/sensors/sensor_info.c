/*
* Copyright (C) huawei company
*
*/
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>


#define INFO_NAME_BUF_LEN    20
#define GPIO_OUT_ZERO 0
#define GPIO_OUT_ONE  1

static ssize_t sensor_show_ACC_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_ACCELEROMETER, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_MAG_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_MAGNETIC, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_MAGNETIC, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_GYRO_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_GYROSCOPE, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_ALS_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_LIGHT, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_PRE_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_SAR_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_SAR, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_SAR, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_PS_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_PROXIMITY, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static DEVICE_ATTR(acc_info, S_IRUGO, sensor_show_ACC_info, NULL);
static DEVICE_ATTR(mag_info, S_IRUGO, sensor_show_MAG_info, NULL);
static DEVICE_ATTR(gyro_info, S_IRUGO, sensor_show_GYRO_info, NULL);
static DEVICE_ATTR(als_info, S_IRUGO, sensor_show_ALS_info, NULL);
static DEVICE_ATTR(pre_info, S_IRUGO, sensor_show_PRE_info, NULL);
static DEVICE_ATTR(sar_info, S_IRUGO, sensor_show_SAR_info, NULL);
static DEVICE_ATTR(ps_info, S_IRUGO, sensor_show_PS_info, NULL);

static struct attribute *sensor_attributes[] = {
    &dev_attr_acc_info.attr,
    &dev_attr_mag_info.attr,
    &dev_attr_gyro_info.attr, 
    &dev_attr_als_info.attr,
    &dev_attr_pre_info.attr, 
    &dev_attr_sar_info.attr,
	&dev_attr_ps_info.attr,
	NULL
};

static const struct attribute_group sensor_node = {
    .attrs = sensor_attributes,
};
 
 static struct platform_device sensor_input_info = {
    .name = "huawei_sensor",
    .id = -1,
};

static int __init sensor_input_info_init(void)
{
    int ret = 0;
	int ven_gpio=0;
    int gpio_err = 0;
    struct device_node *psensor_node =NULL;
    pr_err("[%s] ++ \n", __func__);
    ret = platform_device_register(&sensor_input_info);
    if (ret) {
        pr_err("%s: platform_device_register failed, ret:%d.\n", __func__, ret);
        goto REGISTER_ERR;
    }
 
    ret = sysfs_create_group(&sensor_input_info.dev.kobj, &sensor_node);
    if (ret) {
        pr_err("sensor_input_info_init sysfs_create_group error ret =%d. \n", ret);
        goto SYSFS_CREATE_CGOUP_ERR;
    }
    psensor_node=of_find_compatible_node(NULL, NULL, "mediatek,psensor");
    if(psensor_node){
    pr_info("%s :find psensor node!!!\n",__func__);
    /*get gpio config*/
        ven_gpio=of_get_named_gpio_flags(psensor_node, "psensor,ldo_ven", 0,NULL);
		if (ven_gpio < 0) {
			pr_err( "fail to get ldo ven\n");
			return 0;
		}
    pr_info("%s :find psensor node  ven_gpio= %d !!!\n",__func__,ven_gpio);
    gpio_err=gpio_request(ven_gpio,"ldo_ven");
    if(gpio_err<0){
        pr_err( "ret = %d : could not req gpio reset\n", gpio_err);
    }
    gpio_err=gpio_direction_output(ven_gpio, GPIO_OUT_ZERO);
    if (gpio_err) {
                pr_err("Could not set psensor GPIO170 as output.\n");
        }
    gpio_set_value(ven_gpio, GPIO_OUT_ONE);
    }else{
        pr_err("[%s] --can't find psensor node!!!\n", __func__);
    }
    pr_err("[%s] --\n", __func__);
    return 0;
 SYSFS_CREATE_CGOUP_ERR:
    platform_device_unregister(&sensor_input_info);
 REGISTER_ERR:
    return ret;
}

 module_init(sensor_input_info_init);
 MODULE_DESCRIPTION("sensor input info");
 MODULE_AUTHOR("huawei sensor driver group");
 MODULE_LICENSE("GPL");

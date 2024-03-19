#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <video/mmp_disp.h>
#include "lcm_drv.h"
#include "kd_imgsensor.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "sec_boot_lib.h"

static struct kobject *efuseinfo_kobj = NULL;

static ssize_t efuse_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
    return sprintf(buf, "%d\n", sec_schip_enabled()? 1:0);
}

static struct kobj_attribute sboot_efuse_info_attr = {
    .attr = {
        .name = "hw_efuse_info",
        .mode = 0444,
    },
    .show =&efuse_info_show,
};

static struct attribute * g_sboot_efuse_info_attr[] = {
    &sboot_efuse_info_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = g_sboot_efuse_info_attr,
};

static int __init efuseinfo_init(void)
{
    int ret = -ENOMEM;

    efuseinfo_kobj = kobject_create_and_add("sboot_efuse_info", NULL);

    if (efuseinfo_kobj == NULL) {
        pr_debug("efuseinfo_init: kobject_create_and_add failed\n");
        goto fail;
    }

    ret = sysfs_create_group(efuseinfo_kobj, &attr_group);
    if (ret) {
        pr_debug("efuseinfo_init: sysfs_create_group failed\n");
        goto sys_fail;
    }

    return ret;
sys_fail:
    kobject_del(efuseinfo_kobj);
fail:
    return ret;

}

static void __exit efuseinfo_exit(void)
{
    if (efuseinfo_kobj) {
        sysfs_remove_group(efuseinfo_kobj, &attr_group);
        kobject_del(efuseinfo_kobj);
    }
}

module_init(efuseinfo_init);
module_exit(efuseinfo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Boot information collector");

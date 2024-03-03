#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <huawei_platform/log/hw_log.h>

#define HWLOG_TAG TS_KIT
HWLOG_REGIST();

#define RUNMODE_FLAG_NORMAL     0
#define RUNMODE_FLAG_FACTORY    1


static unsigned int runmode_factory = RUNMODE_FLAG_NORMAL;

static int __init early_parse_runmode_cmdline(char *p)
{
    if (p) {
        if (!strncmp(p, "factory", strlen("factory"))) {
            runmode_factory = RUNMODE_FLAG_FACTORY;
        }
        hwlog_err("runmode is %s, runmode_factory = %d\n", p, runmode_factory);
    }

    return 0;
}

early_param("androidboot.swtype", early_parse_runmode_cmdline);

/* the function interface to check factory/normal mode in kernel */
unsigned int runmode_is_factory(void)
{
    return runmode_factory;
}
EXPORT_SYMBOL(runmode_is_factory);
/*
 *  linux/drivers/mmc/core/rwlog.c
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  MMC sysfs/driver model support.
 */
#include <linux/debugfs.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/module.h>
#include <linux/init.h>

static struct dentry *dentry_mmclog;
u64 rwlog_enable_flag = 0;   /* 0 : Disable , 1: Enable */

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static unsigned int blk_addr = 0;
#endif

static int rwlog_enable_set(void *data, u64 val)
{
	rwlog_enable_flag = val;
	return 0;
}
static int rwlog_enable_get(void *data, u64 *val)
{
	*val = rwlog_enable_flag;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(rwlog_enable_fops,rwlog_enable_get, rwlog_enable_set, "%llu\n");

void host_request_rwlog(struct mmc_host *host, struct mmc_request *mrq)
{
	if ((rwlog_enable_flag==1) && !(mrq->cmd->opcode == MMC_SWITCH
			|| mrq->cmd->opcode == MMC_ERASE_GROUP_START
			|| mrq->cmd->opcode == MMC_ERASE_GROUP_END
			|| mrq->cmd->opcode == MMC_ERASE
			|| mrq->cmd->opcode == MMC_SLEEP_AWAKE
			|| mrq->cmd->opcode == MMC_SELECT_CARD
			|| mrq->cmd->opcode == MMC_SEND_STATUS
			|| mrq->cmd->opcode == MMC_SET_QUEUE_CONTEXT
			|| mrq->cmd->opcode == MMC_QUEUE_READ_ADDRESS
			|| mrq->cmd->opcode == MMC_READ_REQUESTED_QUEUE
			|| mrq->cmd->opcode == MMC_WRITE_REQUESTED_QUEUE) ){
		printk("HUAWEI-lifetime: [CMD%d] [0x%x] [FLAGS0x%x]\n", mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags);
	}
}
EXPORT_SYMBOL(host_request_rwlog);

void mmc_start_request_rwlog(struct mmc_host *host, struct mmc_request *mrq)
{
	if(1 == rwlog_enable_flag){
		if (mrq->cmd->opcode == MMC_SWITCH
			|| mrq->cmd->opcode == MMC_ERASE_GROUP_START
			|| mrq->cmd->opcode == MMC_ERASE_GROUP_END
			|| mrq->cmd->opcode == MMC_ERASE
			|| mrq->cmd->opcode == MMC_SLEEP_AWAKE
			|| mrq->cmd->opcode == MMC_SELECT_CARD
			|| mrq->cmd->opcode == MMC_SEND_STATUS) {
			printk("HUAWEI-lifetime: [CMD%d] [0x%x] [FLAGS0x%x]\n", (int)mrq->cmd->opcode, mrq->cmd->arg, mrq->cmd->flags);
		}
	}
}
EXPORT_SYMBOL(mmc_start_request_rwlog);

void mmc_start_cmdq_request_rwlog(struct mmc_host *host, struct mmc_request *mrq)
{
	if(1 == rwlog_enable_flag){
		if (mrq->data) {
			if (mrq->data->flags & MMC_DATA_READ) {
				printk("HUAWEI-lifetime: [CMDQR] [0x%x] [%d] [DAT_TAG%d] [PRIO%d]\n",
				blk_addr, mrq->data->blocks, 0, 0);
			}
			if (mrq->data->flags & MMC_DATA_WRITE) {
				printk("HUAWEI-lifetime: [CMDQW] [0x%x] [%d] [DAT_TAG%d] [PRIO%d]\n",
				blk_addr, mrq->data->blocks, 0, 0);
			}
		}
	}
}
EXPORT_SYMBOL(mmc_start_cmdq_request_rwlog);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
void mmc_set_cmdq_blkaddr_rwlog(struct mmc_host *host, struct mmc_request *mrq)
{
	if(1 == rwlog_enable_flag){
		if(mrq->cmd){
			if(mrq->cmd->arg){
				blk_addr = mrq->cmd->arg;
			}else{
				blk_addr = 1;
			}
		}else{
			blk_addr = 2;
		}
	}
}
EXPORT_SYMBOL(mmc_set_cmdq_blkaddr_rwlog);
#endif


static int __init rwlog_init(void)
{
	dentry_mmclog = debugfs_create_dir("hw_mmclog", NULL);
	if(dentry_mmclog ) {
		debugfs_create_file("rwlog_enable", S_IFREG|S_IRWXU|S_IRGRP|S_IROTH,
			dentry_mmclog, NULL, &rwlog_enable_fops);
	}
	return 0;
}

subsys_initcall(rwlog_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("huawei rwlog");

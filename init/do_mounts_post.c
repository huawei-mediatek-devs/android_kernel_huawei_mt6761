/*
 * Many of the syscalls used in this file expect some of the arguments
 * to be __user pointers not __kernel pointers.  To limit the sparse
 * noise, turn off sparse checking for this file.
 */
#ifdef __CHECKER__
#undef __CHECKER__
#warning "Sparse checking disabled for this file"
#endif

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/genhd.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>

#include "do_mounts.h"

struct namecmp {
	const char *name;
	int len;
};

/**
 * match_dev_by_name - callback for finding a partition using its name
 * @dev:	device passed in by the caller
 * @data:	opaque pointer to the desired struct namecmp to match
 *
 * Returns 1 if the device matches, and 0 otherwise.
 */
static int match_dev_by_name(struct device *dev, const void *data)
{
	const struct namecmp *cmp = data;
	struct hd_struct *part = dev_to_part(dev);

	if (!part->info)
		goto no_match;

	if (strncasecmp(cmp->name, part->info->volname, cmp->len))
		goto no_match;

	return 1;
no_match:
	return 0;
}

/**
 * devt_from_partname - looks up the dev_t of a partition by its name
 * @name_str:	char array containing ascii name
 *
 * The function will return the first partition which contains a matching
 * name value in its partition_meta_info struct.
 *
 * Returns the matching dev_t on success or 0 on failure.
 */
static dev_t devt_from_partname(const char *name_str)
{
	dev_t res = 0;
	struct namecmp cmp;
	struct device *dev = NULL;

	cmp.name = name_str;
	cmp.len = strlen(name_str);

	dev = class_find_device(&block_class, NULL, &cmp,
				&match_dev_by_name);
	if (!dev)
		goto out;

	res = dev->devt;
	put_device(dev);
out:
	return res;
}

struct mount_map {
	char *name;
	char *dev_name;
	char *mountpoint;
} mount_maps[] = {
	{"eng_system", "/dev/eng_system", "/root/eng"},
	{"eng_vendor", "/dev/eng_vendor", "/root/eng/vendor"},
};

void __init mount_block_root_post(void)
{
	dev_t ENG_DEV;
	int i, err;
	char *name, *dev_name, *mountpoint;
	int mount_flags = MS_RDONLY | MS_SILENT;
	for (i = 0; i < ARRAY_SIZE(mount_maps); i++) {
		name = mount_maps[i].name;
		dev_name = mount_maps[i].dev_name;
		mountpoint = mount_maps[i].mountpoint;

		/* create device node */
		ENG_DEV = devt_from_partname(name);
		err = create_dev(dev_name, ENG_DEV);
		if (err < 0) {
			pr_info("Failed to create %s: %d\n", dev_name, err);
			break;
		}

		/* mount */
		err = sys_mount(dev_name, mountpoint, "ext4", mount_flags, NULL);
		if (err) {
			pr_info("Failed to mount %s: %d\n", dev_name, err);
			break;
		}

		pr_info("VFS: Mounted %s on device %s(%u:%u).\n",
		       mountpoint, dev_name, MAJOR(ENG_DEV), MINOR(ENG_DEV));
	}

	err = sys_mount("/root/eng/init", "/root/init", NULL, MS_BIND, NULL);
	if (err) {
		pr_info("Failed to bind mount: %d\n", err);
		return;
	}

	return;
}

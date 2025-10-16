/*
 * nxp_simtemp.c - NXP Simulated Temperature Sensor Driver
 * 
 * This driver simulates a temperature sensor for the NXP Systems
 * Software Engineer Challenge.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>

#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"

/* Sample structure returned to user space */
struct simtemp_sample {
    __u64 timestamp_ns;
    __s32 temp_mC;
    __u32 flags;
} __attribute__((packed));

/* Device private data */
struct simtemp_device {
    struct platform_device *pdev;
    struct miscdevice mdev;
    u32 sampling_ms;
    s32 threshold_mC;
};

/* Global device pointer (single instance for now) */
static struct simtemp_device *simtemp_dev;

/*
 * Character device operations
 */

static int simtemp_open(struct inode *inode, struct file *filp)
{
    pr_info("simtemp: Device opened\n");
    return 0;
}

static int simtemp_release(struct inode *inode, struct file *filp)
{
    pr_info("simtemp: Device closed\n");
    return 0;
}

static ssize_t simtemp_read(struct file *filp, char __user *buf,
                            size_t count, loff_t *f_pos)
{
    struct simtemp_sample sample;
    int ret;

    pr_info("simtemp: Read requested, count=%zu\n", count);

    /* For now, return a static sample */
    sample.timestamp_ns = ktime_get_ns();
    sample.temp_mC = 42000; /* 42.0°C */
    sample.flags = 0x01;    /* NEW_SAMPLE flag */

    if (count < sizeof(sample))
        return -EINVAL;

    ret = copy_to_user(buf, &sample, sizeof(sample));
    if (ret)
        return -EFAULT;

    pr_info("simtemp: Sent sample: temp=%d.%03d°C\n",
            sample.temp_mC / 1000, sample.temp_mC % 1000);

    return sizeof(sample);
}

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .release = simtemp_release,
    .read = simtemp_read,
};

/*
 * Platform driver probe/remove
 */

static int simtemp_probe(struct platform_device *pdev)
{
    struct simtemp_device *dev;
    int ret;

    pr_info("simtemp: Probing device\n");

    /* Allocate device structure */
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->pdev = pdev;
    platform_set_drvdata(pdev, dev);

    /* Parse Device Tree properties (with defaults) */
    of_property_read_u32(pdev->dev.of_node, "sampling-ms", &dev->sampling_ms);
    if (dev->sampling_ms == 0)
        dev->sampling_ms = 100; /* Default 100ms */

    of_property_read_u32(pdev->dev.of_node, "threshold-mC", &dev->threshold_mC);
    if (dev->threshold_mC == 0)
        dev->threshold_mC = 45000; /* Default 45.0°C */

    pr_info("simtemp: Configuration: sampling_ms=%u, threshold_mC=%d\n",
            dev->sampling_ms, dev->threshold_mC);

    /* Register misc device */
    dev->mdev.minor = MISC_DYNAMIC_MINOR;
    dev->mdev.name = DEVICE_NAME;
    dev->mdev.fops = &simtemp_fops;
    dev->mdev.parent = &pdev->dev;

    ret = misc_register(&dev->mdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register misc device\n");
        return ret;
    }

    simtemp_dev = dev;

    pr_info("simtemp: Device registered successfully at /dev/%s\n", DEVICE_NAME);

    return 0;
}

static void simtemp_remove(struct platform_device *pdev)
{
    struct simtemp_device *dev = platform_get_drvdata(pdev);

    pr_info("simtemp: Removing device\n");

    misc_deregister(&dev->mdev);
    simtemp_dev = NULL;

    pr_info("simtemp: Device removed successfully\n");

}

/* Device Tree match table */
static const struct of_device_id simtemp_of_match[] = {
    { .compatible = "nxp,simtemp", },
    { }
};
MODULE_DEVICE_TABLE(of, simtemp_of_match);

/* Platform driver structure */
static struct platform_driver simtemp_driver = {
    .probe = simtemp_probe,
    .remove = simtemp_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = simtemp_of_match,
    },
};

/*
 * Module init/exit
 */

static struct platform_device *simtemp_pdev;

static int __init nxp_simtemp_init(void)
{
    int ret;

    pr_info("simtemp: Initializing NXP simulated temperature sensor driver\n");

    /* Register platform driver */
    ret = platform_driver_register(&simtemp_driver);
    if (ret) {
        pr_err("simtemp: Failed to register platform driver\n");
        return ret;
    }

    /* 
     * For testing without Device Tree, create a platform device manually.
     * In production, this would come from Device Tree.
     */
    simtemp_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(simtemp_pdev)) {
        pr_err("simtemp: Failed to register platform device\n");
        platform_driver_unregister(&simtemp_driver);
        return PTR_ERR(simtemp_pdev);
    }

    pr_info("simtemp: Driver initialized successfully\n");

    return 0;
}

static void __exit nxp_simtemp_exit(void)
{
    pr_info("simtemp: Exiting driver\n");

    platform_device_unregister(simtemp_pdev);
    platform_driver_unregister(&simtemp_driver);

    pr_info("simtemp: Driver exited successfully\n");
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edgar Valencia");
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor Driver");
MODULE_VERSION("0.1");

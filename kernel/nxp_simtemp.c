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
#include <linux/random.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>

#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"

/* Ring buffer size (must be power of 2 for efficiency) */
#define RING_BUFFER_SIZE 64

/* Sample flags */
#define SIMTEMP_FLAG_NEW_SAMPLE         0x01
#define SIMTEMP_FLAG_THRESHOLD_EXCEEDED 0x02

/* Sample structure returned to user space */
struct simtemp_sample {
    __u64 timestamp_ns;
    __s32 temp_mC;
    __u32 flags;
} __attribute__((packed));

/* Ring buffer structure */
struct simtemp_ring_buffer {
    struct simtemp_sample samples[RING_BUFFER_SIZE];
    unsigned int head;  /* Write position */
    unsigned int tail;  /* Read position */
    spinlock_t lock;    /* Protects buffer access */
};

/* Device private data */
struct simtemp_device {
    struct platform_device *pdev;
    struct miscdevice mdev;
    u32 sampling_ms;
    s32 threshold_mC;
    s32 base_temp_mC;
    u32 temp_variation_mC;
    
    /* Ring buffer for samples */
    struct simtemp_ring_buffer ring_buf;
    
    /* High-resolution timer for periodic sampling */
    struct hrtimer timer;
    ktime_t timer_interval;
    
    /* Wait queue for blocking reads and poll/select */
    wait_queue_head_t wait_queue;
};

/* Global device pointer (single instance for now) */
static struct simtemp_device *simtemp_dev;

/*
 * Ring buffer operations
 */

/**
 * ring_buffer_init - Initialize ring buffer
 * @ring_buf: Ring buffer to initialize
 */
static void ring_buffer_init(struct simtemp_ring_buffer *ring_buf)
{
    ring_buf->head = 0;
    ring_buf->tail = 0;
    spin_lock_init(&ring_buf->lock);
    memset(ring_buf->samples, 0, sizeof(ring_buf->samples));
}

/**
 * ring_buffer_is_empty - Check if ring buffer is empty
 * @ring_buf: Ring buffer to check
 * 
 * Returns: true if empty, false otherwise
 * Note: Must be called with lock held
 */
static bool ring_buffer_is_empty(struct simtemp_ring_buffer *ring_buf)
{
    return ring_buf->head == ring_buf->tail;
}

/**
 * ring_buffer_is_full - Check if ring buffer is full
 * @ring_buf: Ring buffer to check
 * 
 * Returns: true if full, false otherwise
 * Note: Must be called with lock held
 */
static bool ring_buffer_is_full(struct simtemp_ring_buffer *ring_buf)
{
    return ((ring_buf->head + 1) & (RING_BUFFER_SIZE - 1)) == ring_buf->tail;
}

/**
 * ring_buffer_put - Add sample to ring buffer
 * @ring_buf: Ring buffer
 * @sample: Sample to add
 * 
 * Returns: 0 on success, -ENOSPC if buffer is full
 */
static int ring_buffer_put(struct simtemp_ring_buffer *ring_buf,
                           struct simtemp_sample *sample)
{
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&ring_buf->lock, flags);

    if (ring_buffer_is_full(ring_buf)) {
        /* Buffer full, drop oldest sample (overwrite tail) */
        ring_buf->tail = (ring_buf->tail + 1) & (RING_BUFFER_SIZE - 1);
        pr_debug("simtemp: Ring buffer full, dropping oldest sample\n");
    }

    /* Add new sample at head */
    memcpy(&ring_buf->samples[ring_buf->head], sample, sizeof(*sample));
    ring_buf->head = (ring_buf->head + 1) & (RING_BUFFER_SIZE - 1);

    spin_unlock_irqrestore(&ring_buf->lock, flags);

    return ret;
}

/**
 * ring_buffer_get - Get sample from ring buffer
 * @ring_buf: Ring buffer
 * @sample: Output sample
 * 
 * Returns: 0 on success, -EAGAIN if buffer is empty
 */
static int ring_buffer_get(struct simtemp_ring_buffer *ring_buf,
                           struct simtemp_sample *sample)
{
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&ring_buf->lock, flags);

    if (ring_buffer_is_empty(ring_buf)) {
        ret = -EAGAIN;
        goto out;
    }

    /* Get sample from tail */
    memcpy(sample, &ring_buf->samples[ring_buf->tail], sizeof(*sample));
    ring_buf->tail = (ring_buf->tail + 1) & (RING_BUFFER_SIZE - 1);

out:
    spin_unlock_irqrestore(&ring_buf->lock, flags);
    return ret;
}

/**
 * ring_buffer_has_data - Check if buffer has data (lockless check)
 * @ring_buf: Ring buffer
 * 
 * Returns: true if data available, false otherwise
 */
static bool ring_buffer_has_data(struct simtemp_ring_buffer *ring_buf)
{
    unsigned long flags;
    bool has_data;

    spin_lock_irqsave(&ring_buf->lock, flags);
    has_data = !ring_buffer_is_empty(ring_buf);
    spin_unlock_irqrestore(&ring_buf->lock, flags);

    return has_data;
}

/*
 * Temperature generation logic
 */

/**
 * simtemp_generate_sample - Generate a simulated temperature sample
 * @dev: Device structure
 * @sample: Output sample structure
 * 
 * Generates a realistic temperature value with random variation
 * and checks against threshold.
 */
static void simtemp_generate_sample(struct simtemp_device *dev,
                                    struct simtemp_sample *sample)
{
    u32 random_val;
    s32 variation;

    /* Get current timestamp */
    sample->timestamp_ns = ktime_get_ns();

    /* Generate random variation: [-temp_variation_mC, +temp_variation_mC] */
    random_val = get_random_u32();
    variation = (s32)(random_val % (2 * dev->temp_variation_mC + 1)) 
                - dev->temp_variation_mC;

    /* Calculate temperature: base + variation */
    sample->temp_mC = dev->base_temp_mC + variation;

    /* Set flags */
    sample->flags = SIMTEMP_FLAG_NEW_SAMPLE;

    /* Check threshold */
    if (sample->temp_mC > dev->threshold_mC) {
        sample->flags |= SIMTEMP_FLAG_THRESHOLD_EXCEEDED;
        pr_debug("simtemp: Temperature threshold exceeded: %d.%03d°C > %d.%03d°C\n",
                sample->temp_mC / 1000, abs(sample->temp_mC % 1000),
                dev->threshold_mC / 1000, abs(dev->threshold_mC % 1000));
    }

    pr_debug("simtemp: Generated sample: temp=%d.%03d°C, flags=0x%02x\n",
             sample->temp_mC / 1000, abs(sample->temp_mC % 1000),
             sample->flags);
}

/*
 * Timer callback
 */

/**
 * simtemp_timer_callback - High-resolution timer callback
 * @timer: Timer that expired
 * 
 * Called periodically to generate and store temperature samples
 * 
 * Returns: HRTIMER_RESTART to continue periodic execution
 */
static enum hrtimer_restart simtemp_timer_callback(struct hrtimer *timer)
{
    struct simtemp_device *dev;
    struct simtemp_sample sample;

    dev = container_of(timer, struct simtemp_device, timer);

    /* Generate new temperature sample */
    simtemp_generate_sample(dev, &sample);

    /* Store in ring buffer */
    ring_buffer_put(&dev->ring_buf, &sample);

    /* Wake up any waiting readers */
    wake_up_interruptible(&dev->wait_queue);

    /* Schedule next timer */
    hrtimer_forward_now(timer, dev->timer_interval);

    return HRTIMER_RESTART;
}

/*
 * Character device operations
 */

static int simtemp_open(struct inode *inode, struct file *filp)
{
    pr_info("simtemp: Device opened\n");
    filp->private_data = simtemp_dev;
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
    struct simtemp_device *dev = filp->private_data;
    struct simtemp_sample sample;
    int ret;

    if (!dev) {
        pr_err("simtemp: Device not initialized\n");
        return -ENODEV;
    }

    pr_debug("simtemp: Read requested, count=%zu\n", count);

    if (count < sizeof(sample))
        return -EINVAL;

    /* Try to get sample from ring buffer */
    ret = ring_buffer_get(&dev->ring_buf, &sample);
    if (ret) {
        /* Buffer empty */
        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        
        /* Blocking read: wait for data */
        pr_debug("simtemp: Buffer empty, waiting for data...\n");
        ret = wait_event_interruptible(dev->wait_queue,
                                       ring_buffer_has_data(&dev->ring_buf));
        if (ret)
            return ret; /* Interrupted by signal */
        
        /* Try again after waking up */
        ret = ring_buffer_get(&dev->ring_buf, &sample);
        if (ret) {
            /* Still no data (shouldn't happen) */
            pr_warn("simtemp: Woke up but buffer still empty\n");
            return -EAGAIN;
        }
    }

    /* Copy to user space */
    ret = copy_to_user(buf, &sample, sizeof(sample));
    if (ret)
        return -EFAULT;

    pr_debug("simtemp: Sent sample: temp=%d.%03d°C, flags=0x%02x\n",
            sample.temp_mC / 1000, abs(sample.temp_mC % 1000),
            sample.flags);

    return sizeof(sample);
}

static __poll_t simtemp_poll(struct file *filp, poll_table *wait)
{
    struct simtemp_device *dev = filp->private_data;
    __poll_t mask = 0;

    if (!dev) {
        pr_err("simtemp: Device not initialized\n");
        return POLLERR;
    }

    pr_debug("simtemp: Poll called\n");

    /* Add our wait queue to the poll table */
    poll_wait(filp, &dev->wait_queue, wait);

    /* Check if data is available */
    if (ring_buffer_has_data(&dev->ring_buf)) {
        mask |= POLLIN | POLLRDNORM; /* Data available for reading */
        pr_debug("simtemp: Poll: data available\n");
    }

    return mask;
}

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .release = simtemp_release,
    .read = simtemp_read,
    .poll = simtemp_poll,
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

    of_property_read_u32(pdev->dev.of_node, "base-temp-mC", &dev->base_temp_mC);
    if (dev->base_temp_mC == 0)
        dev->base_temp_mC = 35000; /* Default 35.0°C */

    of_property_read_u32(pdev->dev.of_node, "temp-variation-mC", 
                         &dev->temp_variation_mC);
    if (dev->temp_variation_mC == 0)
        dev->temp_variation_mC = 10000; /* Default ±10.0°C */

    pr_info("simtemp: Configuration:\n");
    pr_info("  sampling_ms=%u\n", dev->sampling_ms);
    pr_info("  threshold_mC=%d (%d.%03d°C)\n", 
            dev->threshold_mC,
            dev->threshold_mC / 1000, abs(dev->threshold_mC % 1000));
    pr_info("  base_temp_mC=%d (%d.%03d°C)\n",
            dev->base_temp_mC,
            dev->base_temp_mC / 1000, abs(dev->base_temp_mC % 1000));
    pr_info("  temp_variation_mC=%u (±%u.%03u°C)\n",
            dev->temp_variation_mC,
            dev->temp_variation_mC / 1000, dev->temp_variation_mC % 1000);

    /* Initialize ring buffer */
    ring_buffer_init(&dev->ring_buf);

    /* Initialize wait queue */
    init_waitqueue_head(&dev->wait_queue);
    pr_info("simtemp: Wait queue initialized\n");

    /* Initialize and start high-resolution timer */
    hrtimer_init(&dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    dev->timer.function = simtemp_timer_callback;
    dev->timer_interval = ktime_set(0, dev->sampling_ms * 1000000ULL); /* ms to ns */
    
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

    /* Start the timer */
    hrtimer_start(&dev->timer, dev->timer_interval, HRTIMER_MODE_REL);
    pr_info("simtemp: Timer started with %u ms interval\n", dev->sampling_ms);

    pr_info("simtemp: Device registered successfully at /dev/%s\n", DEVICE_NAME);

    return 0;
}

static void simtemp_remove(struct platform_device *pdev)
{
    struct simtemp_device *dev = platform_get_drvdata(pdev);

    pr_info("simtemp: Removing device\n");

    /* Stop and cleanup timer */
    hrtimer_cancel(&dev->timer);
    pr_info("simtemp: Timer stopped\n");

    /* Wake up any waiting readers before unregistering */
    wake_up_interruptible(&dev->wait_queue);

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
MODULE_VERSION("0.4");

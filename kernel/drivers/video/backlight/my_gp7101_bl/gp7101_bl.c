#include "linux/stddef.h"
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/input/mt.h>
#include <linux/random.h>
#include <linux/backlight.h>

#if 1
#define MY_DEBUG(fmt,arg...)  printk("gp7101_bl:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
#else
#define MY_DEBUG(fmt,arg...)
#endif

#define BACKLIGHT_NAME "gp7101-backlight"

/* 背光控制器设备数据结构 */
struct gp7101_backlight_data {
    /* 指向一个i2c_client结构体的指针*/
    struct i2c_client *client;
    /*......其他成员后面有用到再添加........*/
};

static int gp7101_bl_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    MY_DEBUG("locat");
    return 0;
}

static int gp7101_bl_remove(struct i2c_client *client)
{
    MY_DEBUG("locat");
    return 0;
}

static const struct of_device_id gp7101_bl_of_match[] = {
    { .compatible = BACKLIGHT_NAME, },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gp7101_bl_of_match);

static struct i2c_driver gp7101_bl_driver = {
    .probe      = gp7101_bl_probe,
    .remove     = gp7101_bl_remove,
    .driver = {
        .name     = BACKLIGHT_NAME,
     .of_match_table = of_match_ptr(gp7101_bl_of_match),
    },
};

static int __init my_init(void)
{
    MY_DEBUG("locat");
    return i2c_add_driver(&gp7101_bl_driver);
}

static void __exit my_exit(void)
{
    MY_DEBUG("locat");
    i2c_del_driver(&gp7101_bl_driver);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GP7101 I2C Backlight Driver");
MODULE_AUTHOR("SakoroYou");
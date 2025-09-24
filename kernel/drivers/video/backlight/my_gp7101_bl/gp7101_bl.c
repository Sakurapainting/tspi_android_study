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

/* i2c背光控制器寄存器定义 */
#define BACKLIGHT_REG_CTRL_8    0x03
#define BACKLIGHT_REG_CTRL_16   0x02

/* 背光控制器设备数据结构 */
struct gp7101_backlight_data {
    /* 指向一个i2c_client结构体的指针*/
    struct i2c_client *client;
    /*......其他成员后面有用到再添加........*/
};

/* 设置背光亮度函数 */
static int gp7101_backlight_set(struct backlight_device *bl)
{
    int ret = 0;
    struct gp7101_backlight_data *data = bl_get_data(bl);   // 获取自定义的背光数据结构
    struct i2c_client *client = data->client;               // 获取i2c设备指针
    u8 addr[1] = {BACKLIGHT_REG_CTRL_8};                    // 定义i2c地址数组
    u8 buf[1] = {bl->props.brightness};                     // 定义数据缓冲区，存储亮度值

    MY_DEBUG("pwm:%d\n", bl->props.brightness);             // 输出背光亮度值
    
    // 将背光亮度值写入设备
    ret = i2c_smbus_write_byte_data(client, addr[0], buf[0]);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set brightness, err %d\n", ret);
        return ret;
    }

    return 0;
}

static struct backlight_ops gp7101_backlight_ops = {
    .update_status  = gp7101_backlight_set, // 用于更新背光状态的函数指针
};

static int gp7101_bl_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct backlight_device *bl;                    // backlight_device结构用于表示背光设备
    struct gp7101_backlight_data *data;             // 自定义的背光数据结构
    struct backlight_properties props;              // 用于描述背光设备的属性
    struct device_node* np = client->dev.of_node;   // 设备树节点 

    MY_DEBUG("locat");

    // 为背光数据结构动态分配内存
    data = devm_kzalloc(&client->dev, sizeof(struct gp7101_backlight_data), GFP_KERNEL);
    if(data == NULL) {
        dev_err(&client->dev, "Failed to allocate GFP_KERNEL memory for gp7101_backlight_data\n");
        return -ENOMEM;
    }

    // 初始化背光属性结构
    memset(&props, 0, sizeof(props));
    props.type = BACKLIGHT_RAW; // 设置背光类型为原始类型
    props.max_brightness = 255; // 设置最大亮度值为255

    // 从设备树中读取最大亮度级别
    of_property_read_u32(np, "max-brightness-level", &props.max_brightness);

    // 从设备树中读取默认亮度级别
    of_property_read_u32(np, "default-brightness-level", &props.brightness);

    // 确保亮度值在有效范围内
    if(props.max_brightness > 255 || props.max_brightness < 0) {
        props.max_brightness = 255;
    }
    if(props.max_brightness < 1 || props.brightness > props.max_brightness) {
        props.brightness = props.max_brightness;
    }

    // 注册背光设备
    bl = devm_backlight_device_register(&client->dev, BACKLIGHT_NAME,
            &client->dev, data, &gp7101_backlight_ops, &props);
    if (IS_ERR(bl)) {
        dev_err(&client->dev, "Failed to register backlight device\n");
        return PTR_ERR(bl);
    }

    data->client = client; // 将i2c_client指针存储在自定义数据结构中
    i2c_set_clientdata(client, data); // 将自定义数据结构与i2c_client关联

    MY_DEBUG("max_brightness:%d brightness:%d\n",props.max_brightness, props.brightness);   // 打印最大亮度和当前亮度值
    backlight_update_status(bl); // 更新背光设备状态

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
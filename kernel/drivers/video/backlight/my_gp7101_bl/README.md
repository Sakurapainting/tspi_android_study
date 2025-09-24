# GP7101 背光驱动

## MIPI 驱动

先打开 MIPI 头文件注释

```c
//【开/关】mipi 显示屏幕配置，用户可以基于此复制自己的屏幕，注意EDP与MIPI屏幕互斥，因为共用了VOP如果需要同显自行修改
#include "tspi-rk3566-dsi-v10.dtsi"
```

## GP7101驱动 - 设备树

- 调试屏幕我们一般会先把背光点亮如果使用的是泰山派的背光电路那直接使用代码里面默认的背光PWM驱动就行，但为了保护屏幕背光我们选择的是扩展板上的板载背光电路给3.1寸屏幕背光供电，扩展板板载背光电路PWM脚是通过 **GP7101 i2C转PWM** 芯片实现。所以我们需要编写一个GP7101驱动。

- 从原理图中（**原理图中（GP7101.pdf），SDA数据图，地址位圈出了8位（8位就是包含读写位），标了0xb0**）可知GP7101和触摸共同挂在道I2C下，从数据手册中我们可以得知GP7101的I2C地址是0XB0，0xB0是包含了读写位的所以我们实际填写中还需要右移一位（**设备树需要的：7位地址（不包含读写位）**）最终地址为0X58。

（**0xB0 >> 1 = 0x58**）

- 在tspi-rk3566-dsi-v10.dtsi中添加GP7101相关设备树驱动，首先引用I2C1并往设备树I2C1节点中添加GP7101子节点并指定I2C地址、最大背光，默认背光等。

```c
&i2c1 {              // 引用名为i2c1的节点  
    status = "okay"; // 状态为"okay"，表示此节点是可用和配置正确的  
    GP7101@58 {      // 定义一个子节点，名字为GP7101，地址为58  
        compatible = "gp7101-backlight";   // 该节点与"gp7101-backlight"兼容，  
        reg = <0x58>;                      // GP7101地址0x58  
        max-brightness-levels = <255>;     // 背光亮度的最大级别是255  
        default-brightness-level = <100>;  // 默认的背光亮度级别是100  
    };  
};
```

但是copilot 说 我现在已经有一个 `&i2c1` 了，建议我把新加的这个合进来，变成：

```c

&i2c1 {
	status = "okay";
	ts@5d {
		pinctrl-0 = <&touch_gpio>;
		compatible = "goodix,gt9xx";
		reg = <0x5d>;
		tp-size = <970>;
		max-x = <1080>;
		max-y = <1920>;
		touch-gpio = <&gpio1 RK_PA0 IRQ_TYPE_LEVEL_LOW>;
		reset-gpio = <&gpio1 RK_PA1 GPIO_ACTIVE_HIGH>;
	};

	GP7101@58 {      // 定义一个子节点，名字为GP7101，地址为58
		compatible = "gp7101-backlight";   // 该节点与"gp7101-backlight"兼容
		reg = <0x58>;                      // GP7101地址0x58
		max-brightness-levels = <255>;     // 背光亮度的最大级别是255
		default-brightness-level = <100>;  // 默认的背光亮度级别是100
	};
};
```

## GP7101驱动 - iic框架

- 一般背光驱动都放在 `/kernel/drivers/video/backlight` 目录下，所以我们在此路径下创建一个 `my_gp7101_bl` 目录用来存放 `Makefile` 和 `gp7101_bl.c` 文件。

- my_gp7101_bl/Makefile中把gp7101_bl.c编译到内核中，当然也可以选择obj-m编译成模块。

```makefile
obj-y   += gp7101_bl.o
```

- 要想my_gp7101_bl下的Makefile生效还需要在 `上一层目录` 的Makefile中添加my_gp7101_bl目录，所以我们需要在backlight目录下Makefile中加入：

```makefile
obj-y += my_gp7101_bl/
```

### 示例中 MY_DEBUG 宏

用于在内核模块运行时输出调试信息到内核日志中，帮助开发者追踪代码执行流程和调试问题。

```c
#define MY_DEBUG(fmt,arg...)  printk("gp7101_bl:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
```

#### 参数说明：

- fmt: 用户自定义的格式化字符串（类似 printf 的格式字符串）
- arg...: 可变参数列表
- ##arg: GCC 扩展，处理可变参数

#### 使用示例:
从您的代码中可以看到几个使用例子：

```c
static int gp7101_bl_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    MY_DEBUG("locat");  // 输出: "gp7101_bl:gp7101_bl_probe 32 locat"
    return 0;
}
```

- probe函数：系统检测到匹配的设备时会自动调用。
- remove函数：同probe理。

#### 条件编译控制 - 启用/禁用 调试输出

```c
#if 1
#define MY_DEBUG(fmt,arg...)  printk("gp7101_bl:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
#else
#define MY_DEBUG(fmt,arg...)  // 空定义，不输出任何内容
#endif
```

- 当 #if 1 时：启用调试输出
- 当 #if 0 时：禁用调试输出，宏展开为空，不影响性能

### gp7101_backlight_data

```c
/* 背光控制器设备数据结构 */
struct gp7101_backlight_data {
    /* 指向一个i2c_client结构体的指针*/
    struct i2c_client *client;
    /*......其他成员后面有用到再添加........*/
};
```

### probe 函数

- 为背光数据结构动态分配内存
- 初始化背光属性结构
- 从设备树中读取最大亮度、默认亮度级别
- 确保亮度值在有效范围内
- 注册背光设备
- 设置i2c_client的客户端数据(将自定义数据结构与i2c_client关联)
- 更新背光设备的状态

```c
// gp7101_bl_probe - 探测函数，当I2C总线上的设备与驱动匹配时会被调用
static int gp7101_bl_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct backlight_device *bl; // backlight_device结构用于表示背光设备
    struct gp7101_backlight_data *data; // 自定义的背光数据结构
    struct backlight_properties props; // 背光设备的属性
    struct device_node *np = client->dev.of_node; // 设备树中的节点

    MY_DEBUG("locat"); // 打印调试信息

    // 为背光数据结构动态分配内存
    data = devm_kzalloc(&client->dev, sizeof(struct gp7101_backlight_data), GFP_KERNEL);
    if (data == NULL){
        dev_err(&client->dev, "Alloc GFP_KERNEL memory failed."); // 内存分配失败，打印错误信息
        return -ENOMEM; // 返回内存分配错误码
    }

    // 初始化背光属性结构
    memset(&props, 0, sizeof(props));
    props.type = BACKLIGHT_RAW; // 设置背光类型为原始类型
    props.max_brightness = 255; // 设置最大亮度为255

    // 从设备树中读取最大亮度级别
    of_property_read_u32(np, "max-brightness-levels", &props.max_brightness);

    // 从设备树中读取默认亮度级别
    of_property_read_u32(np, "default-brightness-level", &props.brightness);

    // 确保亮度值在有效范围内
    if(props.max_brightness>255 || props.max_brightness<0){
        props.max_brightness = 255;
    }
    if(props.brightness>props.max_brightness || props.brightness<0){
        props.brightness = props.max_brightness;
    }

    // 注册背光设备
    bl = devm_backlight_device_register(&client->dev, BACKLIGHT_NAME, &client->dev, data, &gp7101_backlight_ops,&props);
    if (IS_ERR(bl)) {
        dev_err(&client->dev, "failed to register backlight device\n"); // 注册失败，打印错误信息
        return PTR_ERR(bl); // 返回错误码
    }
    data->client = client; // 保存i2c_client指针
    i2c_set_clientdata(client, data); // 设置i2c_client的客户端数据

    MY_DEBUG("max_brightness:%d brightness:%d",props.max_brightness, props.brightness); // 打印最大亮度和当前亮度
    backlight_update_status(bl); // 更新背光设备的状态

    return 0; // 返回成功
}
```

- 其中，gfp_t gfp，作用: "Get Free Page" 标志，它用来控制内存分配器的行为：

```c
 // 为背光数据结构动态分配内存
    data = devm_kzalloc(&client->dev, sizeof(struct gp7101_backlight_data), GFP_KERNEL);
    if (data == NULL){
        dev_err(&client->dev, "Alloc GFP_KERNEL memory failed."); // 内存分配失败，打印错误信息
        return -ENOMEM; // 返回内存分配错误码
    }
```

最常用标志:
- GFP_KERNEL: 这是最常用的标志。它表示分配过程可以“睡眠”（阻塞），等待系统回收内存页。这只能在允许睡眠的进程上下文中使用（例如，在 probe 函数中）。
- GFP_ATOMIC: 用于不能睡眠的上下文，例如中断处理函数或持有自旋锁的时候。它告诉分配器必须立即完成分配，不能等待。如果内存不足，会立即失败。
- 隐式行为: devm_kzalloc 的名字中的 z 代表 "zero"。它隐式地包含了 __GFP_ZERO 标志，所以分配到的内存会被自动清零，你不需要再手动调用 memset。

#### dev_err和MY_DEBUG区别

- dev_err (Device Error)
  - 用途: 用于报告设备相关的错误。这是驱动程序中报告严重问题（导致功能无法正常工作的错误）的标准方式。
日志级别: 它使用 KERN_ERR 日志级别。这意味着这个消息非常重要，通常会被记录到系统的主要日志文件中，即使用户没有特意开启调试选项。
  - 格式: dev_err(struct device *dev, const char *fmt, ...)
  - 它会自动在输出中包含驱动和设备的名称（例如 i2c-GP7101@58），这使得定位问题来源非常容易。
  - 何时使用:
    - 内存分配失败 (devm_kzalloc 返回 NULL)。
    - 硬件通信失败 (例如 i2c_smbus_write_byte 返回负值)。
    - 注册子系统失败 (例如 devm_backlight_device_register 返回错误)。
任何阻止驱动正常工作的关键性失败。
- MY_DEBUG (自定义调试宏)
  - 用途: 用于输出调试信息或流程追踪信息。这些信息在正常运行时不是必需的，但在开发和调试阶段非常有用。
  - 日志级别: 您的宏直接使用了 printk，它默认的日志级别是 KERN_WARNING。但更重要的是，您的宏可以通过 #if 0 完全关闭，在最终发布的版本中不会产生任何代码，没有性能开销。
  - 格式: MY_DEBUG(const char *fmt, ...)
  - 您的宏会输出函数名和行号，非常适合追踪代码执行路径。
  - 何时使用:
    - 在函数入口和出口打印信息，确认函数是否被调用 (MY_DEBUG("probe entry"); )。
    - 打印某些关键变量的值 (MY_DEBUG("brightness = %d", level); )。
    - 标记代码执行到了某个特定的分支。

#### devm_backlight_device_register 函数原型

```c
struct backlight_device *devm_backlight_device_register(
    struct device *dev, const char *name, struct device *parent,
    void *devdata, const struct backlight_ops *ops,
    const struct backlight_properties *props);
```

参数说明：

- dev：指向父设备的指针，通常是一个 struct i2c_client 或 struct platform_device。
- name：背光设备的名称。
- parent：背光设备的父设备，通常与 dev 参数相同。
- devdata：私有数据，会被传递给背光操作函数。
- ops：指向 backlight_ops 结构的指针，这个结构定义了背光设备的行为，包括设置亮度、获取亮度等操作。
- props：指向 backlight_properties 结构的指针，这个结构包含了背光设备的属性，如最大亮度、当前亮度等。

### ops 和 gp7101_backlight_set

```c
static struct backlight_ops gp7101_backlight_ops = {
    .update_status = gp7101_backlight_set,
};
```

- gp7101_backlight_set 函数是更新背光的核心函数，每次背光被改动的时候系统都会回调这个函数，在函数中我们通过i2c1去写gp7101实现修改背光。
- gp7101两种操作方法，一种是8位PWM，一种是16位PWM，刚好我们背光是0~255，所以我们选择8位PWM，八位PWM需要写寄存器0x03

```c
/* I2C 背光控制器寄存器定义 */
#define BACKLIGHT_REG_CTRL_8  0x03
#define BACKLIGHT_REG_CTRL_16 0x02

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
```

### 注释backlight

因为我们之前的背光驱动也是用的"backlight"节点，为了不去修改上层我们自己写的驱动也是用的"backlight"节点所以两个节点会冲突，所以我们在tspi-rk3566-dsi-v10.dtsi中把之前的屏蔽掉留下我们自己写的驱动。 屏蔽原有背光设备树节点。

```c
/ {

    /*backlight: backlight {
        compatible = "pwm-backlight";
        pwms = <&pwm5 0 25000 0>;
        brightness-levels = <
              0  20  20  21  21  22  22  23
             23  24  24  25  25  26  26  27
             27  28  28  29  29  30  30  31
             31  32  32  33  33  34  34  35
             35  36  36  37  37  38  38  39
             40  41  42  43  44  45  46  47
             48  49  50  51  52  53  54  55
             56  57  58  59  60  61  62  63
             64  65  66  67  68  69  70  71
             72  73  74  75  76  77  78  79
             80  81  82  83  84  85  86  87
             88  89  90  91  92  93  94  95
             96  97  98  99 100 101 102 103
            104 105 106 107 108 109 110 111
            112 113 114 115 116 117 118 119
            120 121 122 123 124 125 126 127
            128 129 130 131 132 133 134 135
            136 137 138 139 140 141 142 143
            144 145 146 147 148 149 150 151
            152 153 154 155 156 157 158 159
            160 161 162 163 164 165 166 167
            168 169 170 171 172 173 174 175
            176 177 178 179 180 181 182 183
            184 185 186 187 188 189 190 191
            192 193 194 195 196 197 198 199
            200 201 202 203 204 205 206 207
            208 209 210 211 212 213 214 215
            216 217 218 219 220 221 222 223
            224 225 226 227 228 229 230 231
            232 233 234 235 236 237 238 239
            240 241 242 243 244 245 246 247
            248 249 250 251 252 253 254 255
        >;
        default-brightness-level = <255>;
    };*/
};
```

在使用到 `<&backlight>` 时也需要屏蔽掉否则找不到引用节点编译时候会报错。

在dsi1：

```c
&dsi1 {
    status = "okay";
    rockchip,lane-rate = <1000>;
    dsi1_panel: panel@0 {
        /*省略*/
        // backlight = <&backlight>;
        /*省略*/
    }；
}；
```

## 屏参数调试

tspi-rk3566-dsi-v10.dtsi大多数设备树都是已经定义好的，我们这次适配3.1寸触摸屏只需修改以下几个参数，其他保持默认即可。

- 修改lanes数
- 配置初始化序列
- 配置屏幕时序

### 修改lanes数

3.1寸屏幕硬件上只用了2lanes的差分对，设备树中默认配置的是4lanes所以我们需要把lanes修改为2

```c
dsi,lanes  = <2>;
```

### 配置初始化序列

```c
panel-init-sequence = [
    // init code
    05 78 01 01
    05 78 01 11
    39 00 06 FF 77 01 00 00 11
    15 00 02 D1 11
    15 00 02 55 B0 // 80 90 b0
    39 00 06 FF 77 01 00 00 10
    39 00 03 C0 63 00
    39 00 03 C1 09 02
    39 00 03 C2 37 08
    15 00 02 C7 00 // x-dir rotate 0:0x00,rotate 180:0x04
    15 00 02 CC 38
    39 00 11 B0 00 11 19 0C 10 06 07 0A 09 22 04 10 0E 28 30 1C
    39 00 11 B1 00 12 19 0D 10 04 06 07 08 23 04 12 11 28 30 1C
    39 00 06 FF 77 01 00 00 11 // enable  bk fun of  command 2  BK1
    15 00 02 B0 4D
    15 00 02 B1 60 // 0x56  0x4a  0x5b
    15 00 02 B2 07
    15 00 02 B3 80
    15 00 02 B5 47
    15 00 02 B7 8A
    15 00 02 B8 21
    15 00 02 C1 78
    15 00 02 C2 78
    15 64 02 D0 88
    39 00 04 E0 00 00 02
    39 00 0C E1 01 A0 03 A0 02 A0 04 A0 00 44 44
    39 00 0D E2 00 00 00 00 00 00 00 00 00 00 00 00
    39 00 05 E3 00 00 33 33
    39 00 03 E4 44 44
    39 00 11 E5 01 26 A0 A0 03 28 A0 A0 05 2A A0 A0 07 2C A0 A0
    39 00 05 E6 00 00 33 33
    39 00 03 E7 44 44
    39 00 11 E8 02 26 A0 A0 04 28 A0 A0 06 2A A0 A0 08 2C A0 A0
    39 00 08 EB 00 01 E4 E4 44 00 40
    39 00 11 ED FF F7 65 4F 0B A1 CF FF FF FC 1A B0 F4 56 7F FF
    39 00 06 FF 77 01 00 00 00
    15 00 02 36 00 //U&D  Y-DIR rotate 0:0x00,rotate 180:0x10
    15 00 02 3A 55
    05 78 01 11
    05 14 01 29
];
```

### 配置屏幕时序

```c
disp_timings1: display-timings {
    native-mode = <&dsi1_timing0>;
    dsi1_timing0: timing0 {
        clock-frequency = <27000000>;
        hactive = <480>;       //与 LCDTiming.LCDH 对应
        vactive = <800>;       //与 LCDTiming.LCDV 对应
        hfront-porch = <32>;   //与 LCDTiming.HFPD 对应
        hsync-len = <4>;       //与 LCDTiming.HSPW 对应
        hback-porch = <32>;    //与 LCDTiming.HBPD 对应
        vfront-porch = <9>;    //与 LCDTiming.VEPD 对应
        vsync-len = <4>;       //与 LCDTiming.VsPW 对应
        vback-porch = <3>;     //与 LCDTiming.VBPD 对应
        hsync-active = <0>;
        vsync-active = <0>;
        de-active = <0>;
        pixelclk-active = <0>;
    };
};
```


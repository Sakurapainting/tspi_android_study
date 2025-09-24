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


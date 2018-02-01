# GPIO模拟I2C

-----

#前言

**概述**

本文档是在rk3328平台下使用GPIO口替代I2C的方案。

**作者**

林鼎强

**修订记录**

| **日期**     | **版本** | **作者** | **修改说明**           |
| ---------- | ------ | ------ | ------------------ |
| 2017.08.21 | V1.0   | 林鼎强    | 配置dts、移植GPIO_I2C代码 |
| 2017.08.28 | V2.0   | 林鼎强    | 添加I2C时序            |
| 2017.09.04 | V3.0   | 林鼎强    | I2C device mode    |
| 2017.09.11 | V4.0   | 林鼎强    | I2C bus mode       |

-----

目录

[TOC]

-----

#软件框架

##整体框架

![image](http://12487706.s21i-12.faiusr.com/2/ABUIABACGAAgjbvDzQUo4Iq1oAIw0A84zQ8.jpg)

> 1.整体框架，绿色为驱动设计过程中需要修改的部分

##I2C框架

![image](http://images.cnitblog.com/blog/536940/201309/02225054-2c2abb8ed8da431390a03bcbfd6563df.png)

> 图2.I2C框架（引用自http://images.cnitblog.com/）



#编程代码

##device端

###pinctrl

**选择io**

> 因为RK产品的sdk是在pintcrl中进行GPIO的资源分配，所以选择没有被其他外设mux的GPIO，然后在pintcrl中完成资源分配。

| -    | 管脚序列 | GPIO     | IOMUX       |
| ---- | ---- | -------- | ----------- |
| sda  | 42   | gpio1_b2 | gmac_rxd1m1 |
| scl  | 43   | gpio1_b3 | gmac_rxd0m1 |

> 表1.dts配置（iomux、i2c-ldq）
>
> 具体可以借鉴：
>
> Z:\rk-linux\kernel-t\Documentation\devicetree\bindings\pinctrl\pinctrl-bindings.txt 
> Z:\rk-linux\kernel-t\Documentation\devicetree\bindings\pinctrl\rockchip,pinctrl.txt

**dtsi完成IOMUX**

```

&pinctrl {
	i2c-ldq {
		i2c_ldq_gpio: i2c-ldq-gpio {
			rockchip,pins =
				<1 RK_PC2 RK_FUNC_GPIO &pcfg_pull_none>,
				<1 RK_PC3 RK_FUNC_GPIO &pcfg_pull_none>;
		};
	};
};
```

### device

**dts添加设备节点**

```
	i2c-ldq{
		compatible = "rockchip,i2c-ldq";
		gpios = <&gpio1 RK_PB2 GPIO_ACTIVE_HIGH
			 &gpio1 RK_PB3 GPIO_ACTIVE_HIGH
			>;
		timming = <50>;
	};

```

> 代码段1.dts节点



##driver端（GPIO2I2C）

![image](http://12487706.s21i-12.faiusr.com/4/ABUIABAEGAAgxtbTzQUo4LLlhgYwzQo4vQM.png)



> 图3.模拟出的I2C时序



```
struct i2c_gpio_ldq {
        struct i2c_adapter adap;
        unsigned int	sda_pin;
        unsigned int	scl_pin;
        int		udelay;
};

...
static const struct i2c_algorithm ldq_i2c_algorithm = {
	.master_xfer		= ldq_i2c_xfer,
	.functionality		= ldq_i2c_func,
};

...
static int i2c_gpio_probe(struct platform_device *pdev)
{
        ...
        adap->algo = &ldq_i2c_algorithm; //算法替换
        adap->algo_data = priv; //私有数据替换
        
        ...
        
        ret = i2c_add_adapter(&priv->adap);//添加自己的适配器
        
        ...
        
        platform_set_drvdata(pdev, priv);//私有数据内存关联设备
}

```

> 代码段2.总线驱动 i2c-ldq



##driver端（EEPROM）

> 通过EEPROM验证I2C总线驱动可行，了解i2c总线和设备驱动怎么传输

```
static int __init my_i2c_init(void)
{
        //添加设备，选择适配器（根据编号）
    	struct i2c_board_info eeprom_ldq = {
    		I2C_BOARD_INFO(DEVICE_NAME, 0xa0 >> 1),
    		.platform_data = NULL,
    	};
    
    	i2c_adap = i2c_get_adapter(5);
    	my_client = i2c_new_device(i2c_adap, &eeprom_ldq);

    	//添加设备驱动
    	...
    	res = i2c_add_driver(&my_i2c_driver);
    	...
}

```



##APPLICATION

> 调用eeprom的以下几个函数

```
read();
write();
open();

```



#验证

## 功能性验证

![1](GPIO模拟I2C\1.png)

> 图1.test.c读写结果(读写读)

![2](GPIO模拟I2C\2.png)

> 图2.第一次读波形



![3](GPIO模拟I2C\3.png)

> 图3.写波形

![4](E:\2_my_document\circuit\GPIO_simu_I2C\GPIO模拟I2C\4.png)

> 图4.第二次读波形

功能实现。



## 性能分析

***

**频率**

timming 设置为10k时（也就是delay在50us时），实际是8.3khz，这里误差主要原因是因为gpio的api内部做验证，有延时，微妙级别，而这里每个gpio操作有5us左右延时，尤其是in out变换过程，耗时相对多，但是因为i2c是在scl高电平时数据有效，所以保证sycle的情况下，一定的不规整不影响功能，越接近100kHz，波形越不规整，但功能正常。如果要改善这个问题，我想可以从下面两个方面入手：

- 可以在pinctrl中将部分gpio资源释放，自己申请resource，做remap
- 获取gpio框架下的chip的base地址，自己做read write

**电平**

因为是i2c是OD输出，所以slave eeprom输出高电平时，pin需要接上拉电阻到3.5伏高电平，而我选用开发板上拉到3.3v左右的电阻上，所以read时候电平相对低一些。（不接上拉电阻无法驱动到高电平）而stop后电压不稳，这点我不大了解，没有深入考究，如果需要优化，可以考虑一下方法：

- 选择合适电源中的上拉电阻



#改进点

- 频率提升
- 电平稳定
- 中断相关补充
- timeout和阻塞的情况分析
- slave传输



# 附录


/* drivers/i2c/busses/i2c-ldq.c
 *
 * Copyright (C) 2017 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include "i2c-ldq.h"

static void setsda(struct i2c_gpio_ldq *data, int state)
{
	gpio_set_value(data->sda_pin, state);
}

static void setscl(struct i2c_gpio_ldq *data, int state)
{
	gpio_set_value(data->scl_pin, state);
}

static void set_sdadir(struct i2c_gpio_ldq *data, int state)
{
	if (state)
		gpio_direction_output(data->sda_pin, 0);
	else
		gpio_direction_input(data->sda_pin);
}

static int getsda(struct i2c_gpio_ldq *data)
{
	return gpio_get_value(data->sda_pin);
}

static void sdalo(struct i2c_gpio_ldq *data)
{
	setsda(data, 0);
	udelay((data->udelay + 1) / 2);
}

static void scllo(struct i2c_gpio_ldq *data)
{
	setscl(data, 0);
	udelay(data->udelay / 2);
}

static void sclhi(struct i2c_gpio_ldq *data)
{
	setscl(data, 1);
	udelay(data->udelay);
}

static void i2c_start(struct i2c_gpio_ldq *data)
{
	setsda(data, 0);
	udelay(data->udelay);
	scllo(data);
}

static void i2c_stop(struct i2c_gpio_ldq *data)
{
	sdalo(data);
	sclhi(data);
	setsda(data, 1);
	udelay(data->udelay);
}

static int i2c_write_byte(struct i2c_gpio_ldq *data, unsigned char c)
{
	int i;
	int sb;
	int ack;

	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 1;
		setsda(data, sb);
		udelay((data->udelay + 1) / 2);
		sclhi(data);
		scllo(data);
	}
	set_sdadir(data, 0);
	sclhi(data);
	ack = !getsda(data);
	set_sdadir(data, 1);
	scllo(data);

	return ack;
}

static unsigned char i2c_read_byte(struct i2c_gpio_ldq *data)
{
	int i;
	int indata = 0;

	set_sdadir(data, 0);
	for (i = 0; i < 8; i++) {
		setscl(data, 1);
		udelay((data->udelay) / 2);
		indata *= 2;
		if (gpio_get_value(data->sda_pin))
			indata |= 0x01;
		udelay((data->udelay) / 2);
		setscl(data, 0);
		udelay(i == 7 ? data->udelay / 2 : data->udelay);
	}
	set_sdadir(data, 1);

	return (unsigned char)indata;
}

static void acknak(struct i2c_gpio_ldq *data)
{
	setsda(data, 0);
	udelay((data->udelay + 1) / 2);
	sclhi(data);
	scllo(data);
}

static int i2c_ldq_master_recv(struct i2c_gpio_ldq *data, struct i2c_msg *pmsg)
{
	unsigned char slave;
	int ret;
	int count = pmsg->len;
	unsigned char *tmp = pmsg->buf;

	if (count > 8192)
		count = 8192;

	i2c_start(data);
	slave = ((pmsg->addr << 1) | 1);
	ret = i2c_write_byte(data, slave);
	if (ret <= 0)
		goto ack_err;
	while (count) {
		*tmp = i2c_read_byte(data);
		acknak(data);
		count--;
		tmp++;
	}
	i2c_stop(data);

	return count;

ack_err:
	pr_err("ack_err\n");
	i2c_stop(data);

	return ret;
}

static int i2c_ldq_master_send(struct i2c_gpio_ldq *data, struct i2c_msg *pmsg)
{
	unsigned char slave;
	int ret;
	int count = pmsg->len;
	unsigned char *tmp = pmsg->buf;

	if (count > 8192)
		count = 8192;
	slave = (pmsg->addr << 1) | 0;
	i2c_start(data);
	ret = i2c_write_byte(data, slave);
	if (ret <= 0)
		goto ack_err;
	while (count) {
		ret = i2c_write_byte(data, *tmp);
		if (ret <= 0)
			goto ack_err;
		count--;
		tmp++;
	}
	i2c_stop(data);

	return count;

ack_err:
	pr_err("ack_err\n");

	return ret;
}

static int ldq_i2c_xfer(struct i2c_adapter *i2c_adap,
			struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_gpio_ldq *data = i2c_adap->algo_data;
	int i, ret;

	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		if (pmsg->flags & I2C_M_RD)
			ret = i2c_ldq_master_recv(data, pmsg);
		else
			ret = i2c_ldq_master_send(data, pmsg);
	}
	ret = i;

	return ret;
}

static unsigned long ldq_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm ldq_i2c_algorithm = {
	.master_xfer		= ldq_i2c_xfer,
	.functionality		= ldq_i2c_func,
};

static int i2c_gpio_probe(struct platform_device *pdev)
{
	struct i2c_gpio_ldq *priv;
	struct i2c_adapter *adap;
	unsigned int sda_pin, scl_pin;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	sda_pin = of_get_gpio(np, 0);
	scl_pin = of_get_gpio(np, 1);
	if (!gpio_is_valid(sda_pin) || !gpio_is_valid(scl_pin))
		return -ENODEV;

	if (gpio_request(sda_pin, "sda") || gpio_request(scl_pin, "scl"))
		return -ENODEV;
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	adap = &priv->adap;

	if (np) {
		priv->sda_pin = sda_pin;
		priv->scl_pin = scl_pin;
	} else {
		return -1;
	}

	if (!of_property_read_u32(np, "timming", &priv->udelay))
		priv->udelay = 50;
	adap->owner = THIS_MODULE;

	strlcpy(adap->name, dev_name(&pdev->dev), sizeof(adap->name));
	gpio_direction_output(priv->sda_pin, 1);
	gpio_direction_output(priv->scl_pin, 1);
	adap->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;
	adap->algo = &ldq_i2c_algorithm;
	adap->algo_data = priv;

	adap->nr = pdev->id;
	ret = i2c_add_adapter(&priv->adap);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "using pins %u (SDA) and %u (SCL)\n",
		 priv->sda_pin, priv->scl_pin);

	return 0;
}

static int i2c_gpio_remove(struct platform_device *pdev)
{
	struct i2c_gpio_ldq *priv;
	struct i2c_adapter *adap;

	priv = platform_get_drvdata(pdev);
	adap = &priv->adap;
	gpio_free(priv->sda_pin);
	gpio_free(priv->scl_pin);
	i2c_del_adapter(adap);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id i2c_gpio_dt_ids[] = {
	{ .compatible = "rockchip,i2c-ldq", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, i2c_gpio_dt_ids);
#endif

static struct platform_driver i2c_gpio_driver = {
	.driver		= {
		.name	= "i2c-gpio",
		.of_match_table	= of_match_ptr(i2c_gpio_dt_ids),
	},
	.probe		= i2c_gpio_probe,
	.remove		= i2c_gpio_remove,
};

static int __init i2c_gpio_init(void)
{
	int ret;

	ret = platform_driver_register(&i2c_gpio_driver);
	if (ret)
		pr_err("i2c-gpio: probe failed: %d\n", ret);

	return ret;
}
subsys_initcall(i2c_gpio_init);

static void __exit i2c_gpio_exit(void)
{
	platform_driver_unregister(&i2c_gpio_driver);
}
module_exit(i2c_gpio_exit);

MODULE_AUTHOR("Haavard Skinnemoen (Atmel)");
MODULE_DESCRIPTION("Platform-independent bitbanging I2C driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:i2c-gpio");

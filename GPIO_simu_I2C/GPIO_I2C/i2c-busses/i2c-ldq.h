/* drivers/hello/i2c_ldq.h
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
#ifndef _I2C_LDQ_H
#define _I2C_LDQ_H

struct i2c_gpio_ldq {
	struct i2c_adapter adap;
	unsigned int	sda_pin;
	unsigned int	scl_pin;
	int		udelay;
};

#endif

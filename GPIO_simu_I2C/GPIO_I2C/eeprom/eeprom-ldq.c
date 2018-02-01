/* drivers/mics/eeprom/eeprom-ldq
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define DEV_NAME	"eeprom-ldq"
#define DEVICE_NAME	"eeprom"

static int major;
static struct class *my_dev_class;
static struct i2c_client *my_client;
static struct i2c_driver my_i2c_driver;

static struct i2c_device_id my_ids[] = {
	{ DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, my_ids);

int i2c_ldq_release(struct inode *inode, struct file *fd)
{
	fd->private_data = NULL;

	return 0;
}

ssize_t i2c_ldq_read(struct file *fd, char __user *buf,
		     size_t count, loff_t *f_pos)
{
	int ret;
	char *tmp;
	struct i2c_msg msg;
	struct i2c_client *client = fd->private_data;

	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		goto out;

	msg.addr = client->addr;
	msg.flags = 1;
	msg.len = count;
	msg.buf = tmp;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		goto out;
	ret = copy_to_user(buf, tmp, count);
	if (ret < 0)
		goto out;
	msleep(20);
	kfree(tmp);

	return ret;

out:
	kfree(tmp);
	return -1;
}

ssize_t i2c_ldq_write(struct file *fd,
		      const char __user *buf,
		      size_t count,
		      loff_t *f_pos)
{
	int ret;
	char *tmp;
	struct i2c_msg msg;
	struct i2c_client *client = fd->private_data;

	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		goto out;
	ret = copy_from_user(tmp, buf, count);
	if (ret < 0)
		goto out;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = count;
	msg.buf = tmp;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		goto out;
	kfree(tmp);

	return ret;

out:
	kfree(tmp);
	return -1;
}

static int i2c_ldq_open(struct inode *inode, struct file *fd)
{
	fd->private_data = (void *)my_client;
	return 0;
}

static const struct file_operations i2c_ldq_fops = {
	.owner = THIS_MODULE,
	.read = i2c_ldq_read,
	.write = i2c_ldq_write,
	.open = i2c_ldq_open,
	.release = i2c_ldq_release,
};

static int  my_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver my_i2c_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.remove = my_i2c_remove,
	.id_table = my_ids,
};

static int __init my_i2c_init(void)
{
	int res;
	struct i2c_adapter *i2c_adap;

	struct i2c_board_info eeprom_ldq = {
		I2C_BOARD_INFO(DEVICE_NAME, 0xa0 >> 1),
		.platform_data = NULL,
	};

	i2c_adap = i2c_get_adapter(5);
	my_client = i2c_new_device(i2c_adap, &eeprom_ldq);

	major = register_chrdev(0, DEV_NAME, &i2c_ldq_fops);
	my_dev_class = class_create(THIS_MODULE, DEV_NAME);
	device_create(my_dev_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);

	res = i2c_add_driver(&my_i2c_driver);
	return 0;
}

static void __exit my_i2c_exit(void)
{
	device_destroy(my_dev_class, MKDEV(major, 0));
	unregister_chrdev(major, DEV_NAME);
	class_destroy(my_dev_class);

	i2c_unregister_device(my_client);
	i2c_del_driver(&my_i2c_driver);
}

MODULE_AUTHOR("itspy<itspy.wei@gmail.com>");
MODULE_DESCRIPTION("i2c client driver demo");
MODULE_LICENSE("GPL");
module_init(my_i2c_init);
module_exit(my_i2c_exit);

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>

#include <linux/pinctrl/consumer.h>
#include <linux/wait.h>



#define DRIVER_NAME "i2c_td3"

#define FIRST_MINOR 0
#define MINOR_COUNT 1
#define DEV_NAME "mpu6050_td3"

#define MAX_INSTANCES 1

/* Prototipos de las funciones */
static int td3_i2c_remove(struct platform_device *pd_td3_i2c);
static int td3_i2c_probe(struct platform_device *pd_td3_i2c);
static int init_i2c2(void);
static int stop_i2c2(void);

void i2c_writeBuffer (uint8_t *writeData, int writeData_size);
uint8_t i2c_readByte(uint8_t address);
uint8_t i2c_readBuffer(uint8_t address, uint16_t bytes_count);
uint8_t i2c_readRegister(uint8_t register_address);
void i2c_writeRegister(uint8_t register_address, uint8_t data);
static irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs);

uint8_t *i2c_rxData;
uint8_t *i2c_txData;
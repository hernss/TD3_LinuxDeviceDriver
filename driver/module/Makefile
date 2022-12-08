INCLUDES = $(PWD)/inc/

MODULO = td3_i2c_module
MOD_PATH = src/$(MODULO)

obj-m += $(MOD_PATH).o

OUT = src/$(MODULO).ko
SOURCES = src/$(MODULO).c

KERNEL_SOURCE = /lib/modules/$(shell uname -r)/build
KERNEL_ARM = /home/hernss/Imagen_BBB/bb-kernel/KERNEL/

#Reglas para compilar local
all: 
	make -C $(KERNEL_SOURCE) M=$(PWD) -I$(INCLUDES) modules

clean: 
	make -C $(KERNEL_SOURCE) M=$(PWD) -I$(INCLUDES) clean

#Reglas para cross compilar
arm:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERNEL_ARM) SUBDIRS=$(PWD) -I$(INCLUDES) modules

arm_clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERNEL_ARM) SUBDIRS=$(PWD) -I$(INCLUDES) clean



upload: arm
	scp $(OUT) ubuntu@192.168.0.14:/home/ubuntu/driver/


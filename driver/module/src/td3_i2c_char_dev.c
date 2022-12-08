
#include "../inc/td3_i2c_module.h"
#include "../inc/MPU6050.h"
#include "../inc/I2C.h"

/********************************************************************
 *    Register 	       Address
 * ---------------------------------
 *  ACCEL_XOUT_H 	    0x3B
 *  ACCEL_XOUT_L 	    0x3C    
 *  ACCEL_YOUT_H 	    0x3D
 *  ACCEL_YOUT_L 	    0x3E
 *  ACCEL_ZOUT_H 	    0x3F
 *  ACCEL_ZOUT_L 	    0x40
 *  GYRO_XOUT_H 	    0x43
 *  GYRO_XOUT_L 	    0x44
 *  GYRO_YOUT_H 	    0x45
 *  GYRO_YOUT_L 	    0x46
 *  GYRO_ZOUT_H 	    0x47
 *  GYRO_ZOUT_L 	    0x48
 * 
 **********************************************************************/

/* Prototipos de las funciones de archivo */
ssize_t mpu6050_read(struct file *FILE, char *user_buff, size_t bytes_count, loff_t *offset);
int mpu6050_release(struct inode *INODE, struct file *FILE);
int mpu6050_open(struct inode *INODE, struct file *FILE);

/* VARIABLES GLOBALES */
typedef struct MPU6050_REGS { 
    int16_t gyro_xout; 
    int16_t gyro_yout; 
    int16_t gyro_zout; 
    int16_t temp_out;
    int16_t accel_xout; 
    int16_t accel_yout; 
    int16_t accel_zout; 
} accel_data; 

static int8_t instances = 0;

static dev_t dev_id;
static struct cdev *mpu6050_cdev;
static struct file_operations file_op;
static struct class *mpu6050_class;

/* Funcion que establece permisos de usuario en el char dev */
static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

/* Metodo que inicializa el char device con sus file operations. Return 0 en caso de salir todo bien */
static int inicializar_char_device(void){
    
    struct device *dev_ret;

    mpu6050_cdev = cdev_alloc();
    if(NULL == mpu6050_cdev){
        pr_alert("Device TD3-I2C: [%s] No se pudo reservar memoria para la estructura cdev.\n", __func__);
        return -1;
    }

    if(0 != alloc_chrdev_region(&dev_id, FIRST_MINOR, MINOR_COUNT, DEV_NAME)){
        pr_alert("Device TD3-I2C: [%s] Error al registrar la char region.\n", __func__);
        kfree(mpu6050_cdev);
        return -1;
    }

    mpu6050_cdev->owner = THIS_MODULE;
    mpu6050_cdev->dev = dev_id;

    file_op.owner = THIS_MODULE;
    file_op.release = mpu6050_release;
    file_op.open = mpu6050_open;
    file_op.read = mpu6050_read;


    mpu6050_class = class_create(THIS_MODULE, DEV_NAME);
    if (NULL == mpu6050_class)
    {   
        pr_alert("Device TD3-I2C: [%s] Error al crear la clase.\n", __func__);
        kfree(mpu6050_cdev);
        unregister_chrdev_region(FIRST_MINOR, 1);
        return -1;
    }

    mpu6050_class->dev_uevent = change_permission_cdev;


    dev_ret = device_create(mpu6050_class, NULL, dev_id, NULL, DEV_NAME);
    if (NULL == dev_ret)
    {   
        pr_alert("Device TD3-I2C: [%s] Error al crear el dispositivo.\n", __func__);
        kfree(mpu6050_cdev);
        class_destroy(mpu6050_class);
        unregister_chrdev_region(FIRST_MINOR, 1);
        return -1;
    }

    cdev_init(mpu6050_cdev, &file_op);
    
    if(0 != cdev_add(mpu6050_cdev, dev_id, MINOR_COUNT)){
        pr_alert("Device TD3-I2C: [%s] Error al agregar el dispositivo al kernel.\n", __func__);
        kfree(mpu6050_cdev);
        device_destroy(mpu6050_class, FIRST_MINOR);
        class_destroy(mpu6050_class);
        unregister_chrdev_region(dev_id, MINOR_COUNT);
        return -1;
    }
    pr_info("Device TD3-I2C: [%s] Char Device agregado exitosamente. MAJOR=%d, MINOR=%d.\n", __func__, MAJOR(dev_id), MINOR(dev_id));
    return 0;
}


/* Metodo que remueve el char device del kernel. Return 0 en caso de salir todo bien */
static int remover_char_device(void){
    cdev_del(mpu6050_cdev);
    device_destroy(mpu6050_class, dev_id);
    class_destroy(mpu6050_class);
    unregister_chrdev_region(dev_id, MINOR_COUNT);
    kfree(mpu6050_cdev);
    return 0;
}

/* Metodo read de la file operation. Cuando un usuario llame al read al dispositivo el kernel me direcciona el llamado a esta funcion */
ssize_t mpu6050_read(struct file *FILE, char *user_buff, size_t bytes_count, loff_t *offset){

    int res,i;
    uint8_t data;
    uint16_t fifo_count=0, bytes_remaining=0, next_data=0;
    accel_data* sensor_data;

    /* Limito la cantidad de bytes para leer al tamaño de una pagina */
    if(bytes_count > PAGE_SIZE) bytes_count = PAGE_SIZE;

    /* Me fijo si es multiplo de 14, sino lo fuerzo */
    if(bytes_count % sizeof(accel_data)){
        bytes_count = sizeof(accel_data) * (bytes_count / sizeof(accel_data));
    }

    /* Chequeo que el puntero que me envio el usuario sea capaz de almacenar la informacion que me pide */
    if(access_ok(VERIFY_WRITE, user_buff, bytes_count) == 0 ){
        /* Error de acceso a los datos de usuario */
        return -EINVAL;
    }
    
    /* Reservo memoria para le resultado */
    if( (sensor_data = (accel_data*)kmalloc(bytes_count, GFP_KERNEL)) < 0){
        return -EBUSY;
    }

    /* Limpio la FIFO */
    i2c_writeRegister(MPU6050_RA_USER_CTRL, 0x00);
    msleep(1);
    i2c_writeRegister(MPU6050_RA_USER_CTRL, 0x04);
    msleep(1);
    i2c_writeRegister(MPU6050_RA_USER_CTRL, 0x40);
    msleep(1);

    bytes_remaining = bytes_count;
    res = 0;
    while((bytes_remaining>0) && (res<5)){
        if(res++) msleep(1); /*La primera vez no me demoro */

        /* Leo la cantidad de datos que tengo en la fifo */
        data = i2c_readRegister(MPU6050_RA_FIFO_COUNTH);
    
        fifo_count = 0;
        fifo_count = data;
        fifo_count = fifo_count<<8;

        data = i2c_readRegister(MPU6050_RA_FIFO_COUNTL);

        fifo_count |= data;

        if(fifo_count < 14) continue;

        /* Si en la fifo tengo mas de lo que necesito, entonces limito a leer solo lo que necesito */
        if(fifo_count > bytes_remaining){
            fifo_count = bytes_remaining;
        }

        /* Leo la cantidad de bytes en fifo count */
        i2c_readBuffer(MPU6050_RA_FIFO_R_W, fifo_count);

        /* Se los descuento a los bytes que me faltan leer.. */
        bytes_remaining -= fifo_count;

        for(i=0;i<fifo_count;i+=sizeof(accel_data)){
            /* Empaqueto la informacion */
            sensor_data[next_data].gyro_xout  = (0xFF&i2c_rxData[i+0])<<8  | (0xFF&i2c_rxData[i+1]);
            sensor_data[next_data].gyro_yout  = (0xFF&i2c_rxData[i+2])<<8  | (0xFF&i2c_rxData[i+3]);
            sensor_data[next_data].gyro_zout  = (0xFF&i2c_rxData[i+4])<<8  | (0xFF&i2c_rxData[i+5]);
            sensor_data[next_data].temp_out   = (0xFF&i2c_rxData[i+6])<<8  | (0xFF&i2c_rxData[i+7]);
            sensor_data[next_data].accel_xout = (0xFF&i2c_rxData[i+8])<<8  | (0xFF&i2c_rxData[i+9]);
            sensor_data[next_data].accel_yout = (0xFF&i2c_rxData[i+10])<<8 | (0xFF&i2c_rxData[i+11]);
            sensor_data[next_data].accel_zout = (0xFF&i2c_rxData[i+12])<<8 | (0xFF&i2c_rxData[i+13]);

            next_data++;    
        }
        pr_info("FileOP TD3-I2C: [%s] Read.. Bytes remaining=%d Last FIFO=%d.\n", __func__, bytes_remaining, fifo_count);
    }

    /* Me fijo si los datos diponibles me alcanzan para satisfacer al cliente */
    if (bytes_remaining > 0){
        pr_info("FileOP TD3-I2C: [%s] No hay cantidad de datos suficiente en la FIFO. Ask: %d Available: %d\n", __func__, bytes_count, bytes_count - bytes_remaining);
    }

    /* Copio lo leido al buffer del usuario */
    res = copy_to_user(user_buff, sensor_data, bytes_count - bytes_remaining);
    
    if(0 != res){
        pr_info("FileOP TD3-I2C: [%s] Error al copiar el buffer de kernel al buffer de usuario.\n", __func__);
        kfree(sensor_data);
        return -EBUSY;
    }
    
    /* Libero a willy y retorno la cantidad leida */
    kfree(sensor_data);
    return bytes_count - bytes_remaining;
}

/* Metodo release de la file operation. Cuando un usuario llame a close en el dispositivo el kernel me direcciona el llamado a esta funcion */
int mpu6050_release(struct inode *INODE, struct file *FILE){

    if(0 == instances){
        pr_alert("FileOP TD3-I2C: [%s] No hay ninguna instancia del dispositivo abierta\n", __func__);
        return -EINVAL;
    }

    instances--;

    if(0 == instances){
        /* Era la ultima instancia del dispositivo, entonces limpio las variables */
        stop_i2c2();
    }

    pr_info("FileOP TD3-I2C: [%s] Char Device Cerrado...\n", __func__);

    return 0;
}

/* Metodo open de la file operation. Cuando un usuario llame a open en el dispositivo el kernel me direcciona el llamado a esta funcion */
int mpu6050_open(struct inode *INODE, struct file *FILE){

    
    if(instances >= MAX_INSTANCES){
        pr_alert("FileOP TD3-I2C: [%s] Se alcanzo la maxima cantidad de instancias del dispositivo\n", __func__);
        return -EBUSY;
    }

    if(0 == instances){
        /* No hay ninguna instancia del dispositivo, entonces tengo que inicializar le dispositivo */
        init_i2c2();
    }

    instances++;
    pr_info("FileOP TD3-I2C: [%s] Char Device Abierto...\n", __func__);
    return 0;
}
#include "../inc/td3_i2c_module.h"
#include "../inc/MPU6050.h"
#include "../inc/I2C.h"
#include "reg.c"
#include "td3_i2c_char_dev.c"


/* VARIABLES GLOBALES */
static int irq_n;
static int wk_condition;
wait_queue_head_t wqueue = __WAIT_QUEUE_HEAD_INITIALIZER(wqueue);
wait_queue_head_t wqueue_uninterruptuble = __WAIT_QUEUE_HEAD_INITIALIZER(wqueue_uninterruptuble);

static int i2c_txData_size = 0;
static int i2c_txData_byteCount = 0;

static int i2c_rxData_size = 0;
static int i2c_rxData_byteCount = 0;

static const struct of_device_id driver_of_match[] = {   
    { .compatible = DRIVER_NAME },
    { },
};


static struct platform_driver td3_i2c_platform_driver = {
    .probe =  td3_i2c_probe,
    .remove = td3_i2c_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(driver_of_match),
    },
};

static void *i2c_base_address, *cm_per_base_address, *i2c2_control_register, *control_module_base_address;

static int16_t address_ack = 0;

MODULE_DEVICE_TABLE(of, driver_of_match);

/* FIN VARIABLES GLOBALES */

/* Funcion Probe del diver, esta funcion es llamada cuando el kernel tiene un match de nuestro dispositivo en el device tree */
static int td3_i2c_probe(struct platform_device *pd_td3_i2c){
    int res = 0;

    /* Mapeo la direccion base del dispositivo */
    i2c_base_address = of_iomap(pd_td3_i2c->dev.of_node,0);

    /* Mapeo el registro CM_PER */ 
    control_module_base_address = ioremap(CONTROL_MODULE_BASE, CONTROL_MODULE_LENGTH);
    if(NULL == control_module_base_address){
        /* No pudo mapear el registro CM_PER. Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Error al mapear el CONTROL_MODULE\n", __func__);
        iounmap(i2c_base_address);
        return 1;
    }

    /* Mapeo el registro CM_PER */ 
    cm_per_base_address = ioremap(CM_PER_BASE, CM_PER_LENGTH);
    if(NULL == cm_per_base_address){
        /* No pudo mapear el registro CM_PER. Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Error al mapear el CM_PER\n", __func__);
        iounmap(i2c_base_address);
        iounmap(control_module_base_address);
        return 1;
    }

    /* Habilito el modulo i2c2 */
    set_register(cm_per_base_address, CM_PER_I2C2_CLKCTRL, MODULEMODE_ENABLED);
    iowrite32(0x8000, i2c_base_address + I2C_CON);

    /* Pequeña demora para activar el I2C2 */
    msleep(10);

    iowrite32(0x33, control_module_base_address + CONTROL_MODULE_UAR1_CTSN);
    iowrite32(0x33, control_module_base_address + CONTROL_MODULE_UAR1_RTSN);

    /* Mapeo el Registro I2C2 */
    i2c2_control_register = ioremap(I2C2_BASE, I2C2_LENGTH);
    if(NULL == i2c2_control_register){
        /* No pudo mapear el registro I2C2. Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Error al mapear el I2C2 Control Register\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(control_module_base_address);
        return 1;
    }

    /* Pequeña demora para activar el I2C2 */
    msleep(10);

    /* Me fijo si el modulo esta inicializado */
    if(!test_bit_register(i2c2_control_register,I2C_SYSS, RDONE)){
        /* Dispositivo ongoing(?). Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Dispositivo no disponible, reset is on-going\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(i2c2_control_register);
        iounmap(control_module_base_address);
        return 1;
    }

    /* Le pido al kernel el numero de irq */
    irq_n = platform_get_irq(pd_td3_i2c,0);
    if(irq_n < 0){
        /* No se pudo obtener el irq. Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Error al obtener el vIRQ\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(i2c2_control_register);
        iounmap(control_module_base_address);
        return 1;
    }

    /* Registro el handler de la irq */
    if(0 != request_irq(irq_n, (irq_handler_t)i2c_irq_handler, IRQF_TRIGGER_RISING, "td3_i2c_irq_handler", NULL)){
        /* No se pudo vincular el handler al vIRQ. Me voy silbando bajito.. */
        pr_alert("Device TD3-I2C: [%s] Error al vincular el handler con el vIRQ\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(i2c2_control_register);
        iounmap(control_module_base_address);
        return 1;
    }


    if((i2c_rxData = (uint8_t *) __get_free_page(GFP_KERNEL))<0){
        pr_alert("Device TD3-I2C: [%s] Error al reservar memoria para el buffer de lectura\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(i2c2_control_register);
        iounmap(control_module_base_address);
        free_irq(irq_n, NULL);
        return 1;
    }

    res = inicializar_char_device();

    if(res){
        pr_alert("Device TD3-I2C: [%s] Error al inicializar el char device.\n", __func__);
        iounmap(i2c_base_address);
        iounmap(cm_per_base_address);
        iounmap(i2c2_control_register);
        iounmap(control_module_base_address);
        free_irq(irq_n, NULL);
        free_page((long unsigned int)i2c_rxData);
    }else{
        
        pr_info("Device TD3-I2C: [%s] Probe Completo, device name=%s\n", __func__, pd_td3_i2c->name);
    }
    return res;
}

/* Funcion Remove del diver, esta funcion es llamada cuando el kernel cuando remueve el dispositivo */
static int td3_i2c_remove(struct platform_device *pd_td3_i2c){
    remover_char_device();
    iounmap(i2c_base_address);
    iounmap(cm_per_base_address);
    iounmap(i2c2_control_register);
    iounmap(control_module_base_address);
    free_irq(irq_n, NULL);
    free_page((long unsigned int)i2c_rxData);
    pr_info("Device TD3-I2C: [%s] Removido exitosamente.\n", __func__);
    return 0;
}

/* FUNCIONES DEL DISPOSITIVO */

static int init_i2c2(void){

    // >------------------ Setting up I2C Registers -------------------< //
    // Disable I2C2.
    iowrite32(0x0000, i2c_base_address + I2C_CON);

    // Prescaler configured at 24Mhz
    iowrite32(0x03, i2c_base_address + I2C_PSC);

    // Configure SCL to 1MHz
    iowrite32(0x35, i2c_base_address + I2C_SCLL);
    iowrite32(0x37, i2c_base_address + I2C_SCLH);

    // Random Own Address
    iowrite32(0x77, i2c_base_address + I2C_OA);

    // I2C_SYSC has 0h value on reset, don't need to be configured.
    iowrite32(0x00, i2c_base_address + I2C_SYSC);

    // Slave Address
    iowrite32(MPU6050_I2C_ADDR, i2c_base_address + I2C_SA);    

    // configure register -> ENABLE & MASTER & RX & STOP
    // iowrite32(0x8400, i2c2_base + I2C_CON);
    iowrite32(0x8600, i2c_base_address + I2C_CON);

        
    /* Iniciando el dispositivo */
    //pr_info("Device TD3-I2C: [%s] MPU-6050 I2C Iniciado correctamente, vamos por el MPU.\n", __func__);

    /* Configuro el fondo de escala del giroscopo en 250 grados (?)
    Type: Read/Write 
    Register (Hex)   Bit7   Bit6    Bit5    Bit4 Bit3     Bit2 Bit1 Bit0 
        1B           XG_ST  YG_ST   ZG_ST   FS_SEL[1:0]     -    -    -

    FS_SEL Full Scale Range 
        0 ± 250 °/s 
        1 ± 500 °/s 
        2 ± 1000 °/s 
        3 ± 2000 °/s 
    */
    /* REG = MPU6050_RA_GYRO_CONFIG (0x1B)  DATA = MPU6050_GYRO_FS_250 (0x00) */
    i2c_writeRegister(MPU6050_RA_GYRO_CONFIG, MPU6050_GYRO_FS_250);    
    
    /* Configuro el fondo de escala del acelerometro en +-2g 
    Type: Read/Write 
    Register (Hex) Bit7   Bit6    Bit5    Bit4 Bit3        Bit2 Bit1 Bit0 
        1C         XA_ST  YA_ST   ZA_ST   AFS_SEL[1:0]      -     -    -
  
    AFS_SEL Full Scale Range 
        0 ± 2g 
        1 ± 4g 
        2 ± 8g 
        3 ± 16g 
    */
    /* REG = MPU6050_RA_ACCEL_CONFIG (0x1C)  DATA = MPU6050_ACCEL_FS_2 (0x00) */
    i2c_writeRegister(MPU6050_RA_ACCEL_CONFIG, MPU6050_ACCEL_FS_2);

    /* Activo la FIFO interna del dispositivo */
    /* TEMP_FIFO_EN | XG_FIFO_EN | YG_FIFO_EN | ZG_FIFO_EN | ACCEL_FIFO_EN */
    i2c_writeRegister(MPU6050_RA_FIFO_EN, 0xF8);

    i2c_writeRegister(MPU6050_RA_USER_CTRL, 0x40); /* FIFO_EN */

    i2c_writeRegister(MPU6050_RA_SMPLRT_DIV,0x20); //fs = 31.25 hz

    /* Configuro el Clock y Salgo del modo sleep
        Upon power up, the MPU-60X0 clock source defaults to the internal oscillator. However,  it is highly 
        recommended  that  the  device  be  configured  to  use  one  of  the  gyroscopes  (or  an  external  clock 
        source) as the clock reference for improved stability. 
    */
    /* REG = MPU6050_RA_PWR_MGMT_1 (0x6B)   DATA = MPU6050_CLOCK_PLL_XGYRO(0x01) */
    i2c_writeRegister(MPU6050_RA_PWR_MGMT_1,MPU6050_CLOCK_PLL_XGYRO); 

    pr_info("Device TD3-I2C: [%s] MPU-6050 Iniciado correctamente.\n", __func__);
    return 0;
}

/* Esta funcion deshabilita el i2c2 y el MPU6050 para entrar en modo bajo consumo */
static int stop_i2c2(void){
    pr_info("Device TD3-I2C: [%s] Stopping MPU-6050.\n", __func__);
    
    /*Apago el MPU6050 */
    i2c_writeRegister(MPU6050_RA_PWR_MGMT_1, 0x40);

    // Disable I2C2.
    iowrite32(0x0000, i2c_base_address + I2C_CON);

    return 0;
}

/* Handler de la interrupcion del dispositivo I2C */
static irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs){
    
    uint32_t irq_status;
    uint32_t aux_regValue;

    /*
    I2C_IRQSTATUS is shown in Figure 21-20 and described in Table 21-13.
    This register provides core status information for interrupt handling, showing all active and enabled events
    and masking the others. The fields are read-write. Writing a 1 to a bit will clear it to 0, that is, clear the
    IRQ. Writing a 0 will have no effect, that is, the register value will not be modified. Only enabled, active
    events will trigger an actual interrupt request on the IRQ output line. For all the internal fields of the
    I2C_IRQSTATUS register, the descriptions given in the I2C_IRQSTATUS_RAW subsection are valid.
    */

    irq_status = ioread32(i2c_base_address + I2C_IRQSTATUS);

    //pr_info("Device TD3-I2C: [%s] Interrupcion: IRQ_STATUS=%#X\tFLAG_ACK=%d, FLAG_RX=%d, FLAG_TX=%d.\n", __func__, irq_status, (irq_status & I2C_IRQSTATUS_ARDY)?1:0, (irq_status & I2C_IRQSTATUS_RRDY)?1:0, (irq_status & I2C_IRQSTATUS_XRDY)?1:0);

    /* Me fijo si me llego una interrupcion por lectura o por escritura */
    if(irq_status & I2C_IRQSTATUS_RRDY){
        /* Fue lectura, entonces leo lo que haya que leer */
        i2c_rxData[i2c_rxData_byteCount++] = ioread32(i2c_base_address + I2C_DATA);

        //pr_info("Device TD3-I2C: [%s] Interrupcion: RECEIVED=%#X, LEFT=%d\n", __func__, i2c_rxData[i2c_rxData_byteCount-1], ioread32(i2c_base_address + I2C_CNT));

        if(i2c_rxData_byteCount == i2c_rxData_size){
            /* Borro los flags de las interrupciones. Page 4612-4613 */
            /* 0000 0000 0010 0111 b = 0x27 */
            aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS);
            aux_regValue |= 0x27;
            iowrite32(aux_regValue, i2c_base_address + I2C_IRQSTATUS);

            /* Como ya atendi la interrupcion, la desactivo. Page 4616 */
            aux_regValue = ioread32(i2c_base_address + I2C_IRQENABLE_CLR);
            aux_regValue |= I2C_IRQSTATUS_RRDY;         /* BIT 3 RRDY_IE */
            iowrite32 (aux_regValue, i2c_base_address + I2C_IRQENABLE_CLR);

            /* Cambio la condicion para despertar el driver y lo desencolo */
            wk_condition = 1;
            wake_up_interruptible(&wqueue);
        }
        // Clear all flags.
        // 0000 0000 0011 1110 b = 0x3E
        irq_status = ioread32(i2c_base_address + I2C_IRQSTATUS);
        irq_status |= 0x3E;
        iowrite32(irq_status, i2c_base_address + I2C_IRQSTATUS);  
    }else if(irq_status & I2C_IRQSTATUS_XRDY){
        /* Es escritura.. entonces escribo en el bus */
        /*
            i2c_txData tiene los datos totales a escribir
            i2c_txData_byteCount tiene la cantidad escrita
        */

        iowrite32((uint32_t)i2c_txData[i2c_txData_byteCount], i2c_base_address + I2C_DATA);

        /* pr_info("Device TD3-I2C: [%s] Interrupcion: WRITTEN=%#X.\n", __func__, i2c_txData[i2c_txData_byteCount]); */
        i2c_txData_byteCount++;

        /* Me fijo si ya escribi todo lo que tenia que escribir, sino me quedo esperando la proxima interrupcion para enviar el byte siguiente */
        if(i2c_txData_byteCount == i2c_txData_size){
            /* Termine, borro los flags y desactivo las interrupciones */
            i2c_txData_byteCount = 0;

            /* Borro los flags de las interrupciones. Page 4612-4613 */
            /* 0000 0000 0011 0110 b = 0x36 */
            aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS);
            aux_regValue |= 0x36;
            iowrite32(aux_regValue, i2c_base_address + I2C_IRQSTATUS);

            /* Como ya atendi la interrupcion, la limpio. Page 4616 */
            aux_regValue = ioread32(i2c_base_address + I2C_IRQENABLE_CLR);
            aux_regValue |= I2C_IRQSTATUS_XRDY;         /* BIT 4 XRDY_IE */
            iowrite32 (aux_regValue, i2c_base_address + I2C_IRQENABLE_CLR);

            /* Cambio la condicion para despertar el driver y lo desencolo */
            wk_condition = 1;
            wake_up(&wqueue_uninterruptuble);
        }
        // Clear all flags.
        // 0000 0000 0011 1110 b = 0x3E
        irq_status = ioread32(i2c_base_address + I2C_IRQSTATUS);
        irq_status |= 0x3E;
        iowrite32(irq_status, i2c_base_address + I2C_IRQSTATUS); 
        
    }else if(irq_status & I2C_IRQSTATUS_ARDY){
        //pr_info("Device TD3-I2C: [%s] Interrupcion: ACK.\n", __func__);
        
        if(!address_ack) {
            /* Si la variable es cero es porque es el primer ack de la transmicion, el que corresponde luego de enviar la direccion del dispositivo */
            address_ack++;
            /* Limpio el flag de la interrupcion */
            iowrite32(I2C_IRQSTATUS_ARDY, i2c_base_address + I2C_IRQENABLE_CLR);
            /* Vuelvo a activar la interrupcion por ACK */
            iowrite32(I2C_IRQSTATUS_ARDY, i2c_base_address + I2C_IRQENABLE_SET);
            /* Limpio todas las interrupciones */
            iowrite32(0x6FFF, i2c_base_address + I2C_IRQSTATUS);
            return IRQ_HANDLED;
        }else {
            /* Si entro por aca es porque es el segudo ACK, este se produce luego de que se escribio la direccion del registro que quiero leer */
            /* Ahora cambio a modo lectura para leer desde este registro */

            /* Limpio el flag de la interrupcion */
            iowrite32(I2C_IRQSTATUS_ARDY, i2c_base_address + I2C_IRQENABLE_CLR);

            /* Escribo en el registro CNT la cantidad que quiero leer */
            iowrite32(i2c_rxData_size, i2c_base_address + I2C_CNT);

            /* Activo la interrupcion por lectura */
            iowrite32(I2C_IRQSTATUS_RRDY, i2c_base_address + I2C_IRQENABLE_SET);

            /* Setero modo lectura y mando un nuevo bit de start */
            iowrite32(0x8400 | I2C_CON_START, i2c_base_address + I2C_CON);

            /* Limpio todas las interrupciones */
            iowrite32(0x6FFF, i2c_base_address + I2C_IRQSTATUS);

            address_ack=0;
        }
    }

    return (irqreturn_t)IRQ_HANDLED;
}

/* Funcion que escribe un buffer de bytes de largo writeData_size en el puerto i2c */
void i2c_writeBuffer (uint8_t *writeData, int writeData_size){
    uint32_t i = 0;
    uint32_t aux_regValue = 0;

    /* Limpio las interrupciones */
    //iowrite32(0x6FFF, i2c_base_address + I2C_IRQENABLE_CLR);
    //iowrite32(0x6FFF, i2c_base_address + I2C_IRQSTATUS);

    /* Controlo el estado del bus para ver si esta ocupado */
    /* 
        0h = Bus is free
        1h = Bus is occupied
        Page 4607
    */
    aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS_RAW);

    while(aux_regValue & (1<<12)){
        msleep(100);     
        i++;

        /* Le doy 4 intentos para liberar el buffer */
        if(i==4){
            aux_regValue = ioread32(i2c_base_address + I2C_CON);
            pr_info("Device TD3-I2C: [%s] El bus esta muy ocupado. I2C_CON=%#X\n", __func__, aux_regValue);
            return;
        }
        aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS_RAW);
    }

    /* Cargo el dato que quiero escribir en variables globales */
    i2c_txData = writeData;
    i2c_txData_size = writeData_size;

    /* Seteo la cantidad de bytes a transmitir */
    /* 16-bit countdown counter decrements by 1 for every byte received or sent through the I2C interface. Page 4632 */
    iowrite32((u16)i2c_txData_size, i2c_base_address + I2C_CNT);

    /* Seteo el registro de control I2C_CON para transmitir como maestro */
    /* BIT 15 I2C_EN   I2C module enable.
       BIT 10 MST R/W  Master/slave mode (I2C mode only). 0h = Slave mode 1h = Master mode
       BIT  9 TRX R/W  Transmitter/receiver mode (i2C master mode only). 0h = Receiver mode 1h = Transmitter mode 
    */
    aux_regValue = ioread32(i2c_base_address + I2C_CON);
    aux_regValue |= 0x8600;
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);

    /* I2C_IRQENABLE_SET es un registro de mascaras para habilitar las interrupcion */
    /* I2C_IRQSTATUS_XRDY(0x10) habilita la interrupcion por finalizacion de escritura. Page 4615 */
    iowrite32(I2C_IRQSTATUS_XRDY, i2c_base_address + I2C_IRQENABLE_SET);

    /* Inicio la transmicion generando una condicion de start en el registro de control. Page 4636*/
    /* Activo el bit de start, limpio el bit I2C_CON_STP por las dudas. Page 4634*/ 
    aux_regValue = ioread32(i2c_base_address + I2C_CON);
    aux_regValue &= 0xFFFFFFFC;                 /* Bit STP (1) en 0, el resto sin cambios */
    aux_regValue |= I2C_CON_START;              /* Bit STT (0) en 1 */
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);

    /* Duermo el driver hasta que termine de escribir en el dispositivo */
    wait_event(wqueue_uninterruptuble, wk_condition > 0);
    
    wk_condition = 0;

    /* Termine de transmitir entonces envio la señal de STOP */
    /* Para esto seteo el BIT 1 STP y me aseguro que el BIT 0 de start este en nivel bajo (Se supone que el hardware me lo deberia haber limpiado este bit) */
    /* BIT 1 STP R/W 0h Stop condition (I2C master mode only).
       BIT 0 STT R/W 0h Start condition (I2C master mode only).
    */
    aux_regValue = ioread32(i2c_base_address + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;         /* Limpio el STT */
    aux_regValue |= I2C_CON_STOP;       /* Seteo el STP */
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);

    //pr_info("Device TD3-I2C: [%s] Escritura finalizada. Status=%d, Data Write=%#X.\n", __func__, status, writeData[0]);
}

/* Funcion que lee un bytes desde el puerto i2c */
uint8_t i2c_readByte(uint8_t address){
    i2c_readBuffer(address, 1);
    return i2c_rxData[0];
}

/* Funcion que lee bytes_count bytes desde el i2c y lo almacena en la variable global i2c_rxData */
uint8_t i2c_readBuffer(uint8_t address, uint16_t bytes_count){
    uint32_t i = 0;
    uint32_t aux_regValue = 0;
    uint32_t status = 0;
    uint8_t readData;

    /* Limpio las interrupciones */
    //iowrite32(0x6FFF, i2c_base_address + I2C_IRQENABLE_CLR);
    //iowrite32(0x6FFF, i2c_base_address + I2C_IRQSTATUS);

    /* Controlo el estado del bus para ver si esta ocupado */
    /* 
        0h = Bus is free
        1h = Bus is occupied
        Page 4607
    */
    aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS_RAW);
    
    while(aux_regValue & (1<<12)){
        msleep(100);     
        i++;

        if(i==5){
            aux_regValue = ioread32(i2c_base_address + I2C_CON);
            pr_info("Device TD3-I2C: [%s] El bus esta muy ocupado. I2C_CON=%#X\n", __func__, aux_regValue);
            return -1;
        } 
        aux_regValue = ioread32(i2c_base_address + I2C_IRQSTATUS_RAW);
    }

    /* Seteo la cantidad de bytes a leer*/
    iowrite32(1, i2c_base_address + I2C_CNT);
    iowrite32(address, i2c_base_address + I2C_DATA);

    /* Esta variable se incremente con cada byte recibido */
    i2c_rxData_byteCount = 0;

    /* En esta variable tengo el total de bytes que tengo que leer */
    i2c_rxData_size = bytes_count;

    /* Flag para saber si ya recibi el primer ACK de la transmicion */
    address_ack = 0;

    /* Seteo el I2C en modo maestro y en modo byte. Page 4634*/
    /* BIT 15 I2C_EN    R/W 0h I2C module enable.
       BIT 10 MST       R/W 0h Master/slave mode (I2C mode only).
       bit 9 transmit
       El resto de los bits en 0
    */ 
    aux_regValue = 0x8600;
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);

    /* Activo la interrupcion por fin de lectura. Page 4615 */
    iowrite32(I2C_IRQSTATUS_ARDY, i2c_base_address + I2C_IRQENABLE_SET);

    /* Activo el bit de start, limpio el bit I2C_CON_STP por las dudas. Page 4634*/ 
    aux_regValue = ioread32(i2c_base_address + I2C_CON);
    aux_regValue &= 0xFFFFFFFC;                 /* Bit STP (1) en 0, el resto sin cambios */
    aux_regValue |= I2C_CON_START;              /* Bit STT (0) en 1 */
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);

    /* Duermo el driver hasta que termine de leer en el dispositivo */
    status = wait_event_interruptible(wqueue, wk_condition > 0);
    if(status < 0){
        /* Se produjo un error al encolar el driver */
        wk_condition = 0;
        pr_alert("Device TD3-I2C: [%s] Error al encolar el driver en la cola de espera.\n", __func__);
    }

    wk_condition = 0;

    // Stop condition queried. (Must clear I2C_CON_STT).
    aux_regValue = ioread32(i2c_base_address + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c_base_address + I2C_CON);
    // Retrieve data.
    readData = i2c_rxData[0];

    //pr_info("Device TD3-I2C: [%s] Lectura finalizada. Status=%d, Data read=%#X.\n", __func__, status, readData);

    return status;
}

/* Funcion que lee el valor de un registro a travez del bus i2c */
uint8_t i2c_readRegister(uint8_t register_address){

    
    return i2c_readByte(register_address);          /* Leo el dato del registro */
}

/* Funcion que escribe el valor data en un registro a travez del bus i2c */
void i2c_writeRegister(uint8_t register_address, uint8_t data){

    uint8_t buff[2];
    buff[0] = register_address;
    buff[1] = data;
    i2c_writeBuffer(buff, 2);      /* Escribo la direccion del registro a escribir y el dato*/

    return ;
}
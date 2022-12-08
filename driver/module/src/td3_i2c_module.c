#include "../inc/td3_i2c_module.h"
#include "td3_i2c_init.c"


extern struct platform_driver td3_i2c_platform_driver;


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hernan M. Travado - 131.453-1");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Modulo de driver de I2C.");

/* Esta funcion es llamada por el kernel cuando instalamos el modulo en el sistema */
static int __init modulo_td3_start(void){
    int res;
    
    res = platform_driver_register(&td3_i2c_platform_driver);
    if(res < 0){
        pr_alert("Module TD3-I2C: [%s] Error al registrar el driver.\n", __func__);
        platform_driver_unregister(&td3_i2c_platform_driver);
        return -1;
    }
    pr_info("Module TD3-I2C: [%s] Inicializacion completa.\n", __func__);
    return 0;
}

/* Esta funcion es llamada por el kernel cuando removemos el modulo del sistema */
static void __exit modulo_td3_end(void){
    platform_driver_unregister(&td3_i2c_platform_driver);
    pr_info("Module TD3-I2C: [%s] Finalizado\n", __func__);
}

module_init(modulo_td3_start);
module_exit(modulo_td3_end);

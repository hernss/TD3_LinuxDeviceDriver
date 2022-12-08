#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xd1ebd131, "module_layout" },
	{ 0x161a4445, "platform_driver_unregister" },
	{ 0xfa20e62d, "__platform_driver_register" },
	{ 0xf4fa543b, "arm_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x49970de8, "finish_wait" },
	{ 0x647af474, "prepare_to_wait_event" },
	{ 0x1000e51, "schedule" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x6c07d933, "add_uevent_var" },
	{ 0x2b64dcf0, "cdev_add" },
	{ 0xeda215f1, "cdev_init" },
	{ 0x7d6bb920, "device_create" },
	{ 0x320a5cbb, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xe25fff5c, "cdev_alloc" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0xd6b8e852, "request_threaded_irq" },
	{ 0xe19cc6c3, "platform_get_irq" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe97c4103, "ioremap" },
	{ 0xbdfda070, "of_iomap" },
	{ 0x3dcf1ffa, "__wake_up" },
	{ 0x822137e2, "arm_heavy_mb" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x7c32d0f0, "printk" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xc1514a3b, "free_irq" },
	{ 0xedc03953, "iounmap" },
	{ 0x37a0cba, "kfree" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xb7bbd79b, "class_destroy" },
	{ 0xe91b229b, "device_destroy" },
	{ 0xf309d9b, "cdev_del" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Ci2c_td3");
MODULE_ALIAS("of:N*T*Ci2c_td3C*");

MODULE_INFO(srcversion, "76A1BC2DB383BCDD006EFAB");

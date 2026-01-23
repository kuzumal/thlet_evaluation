#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
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
__used __section("__versions") = {
	{ 0xc38d863d, "module_layout" },
	{ 0x5001e0a7, "cdev_alloc" },
	{ 0xfc451d7, "cdev_del" },
	{ 0xb01b4b88, "kmalloc_caches" },
	{ 0x359d4465, "cdev_init" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0x9f8fe305, "cdev_add" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x92997ed8, "_printk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x927d80ce, "kmem_cache_alloc_trace" },
	{ 0x37a0cba, "kfree" },
	{ 0x85198fd9, "remap_pfn_range" },
	{ 0x6228c21f, "smp_call_function_single" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "EA2A6BAEEE857953553CB81");

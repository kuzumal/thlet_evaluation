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
	{ 0x17198eab, "module_layout" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x305a9ec9, "cdev_del" },
	{ 0x37a0cba, "kfree" },
	{ 0xfc74f6a8, "cdev_add" },
	{ 0x454e608c, "cdev_init" },
	{ 0x43d4836, "cdev_alloc" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0x7641376f, "remap_pfn_range" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x689aedd5, "kmem_cache_alloc_trace" },
	{ 0x6cd6996b, "kmalloc_caches" },
	{ 0x92997ed8, "_printk" },
	{ 0x6228c21f, "smp_call_function_single" },
	{ 0xd063d77, "cpu_thlet_stats" },
	{ 0x5b8239ca, "__x86_return_thunk" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "40200E8D7E2B9564027C887");

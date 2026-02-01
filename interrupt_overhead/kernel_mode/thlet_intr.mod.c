#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

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



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x1fa3ffd3, "cdev_alloc" },
	{ 0x9906502f, "cdev_init" },
	{ 0x89a8314d, "cdev_add" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x37a0cba, "kfree" },
	{ 0x4f6a57d3, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0x9f3bb84d, "kmalloc_caches" },
	{ 0x2201a8f6, "__kmalloc_cache_noprof" },
	{ 0xb3b16c0b, "remap_pfn_range" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x179cc4f4, "cpu_thlet_stats" },
	{ 0x1fb30341, "cpu_thlet_stats_summary" },
	{ 0x6228c21f, "smp_call_function_single" },
	{ 0x122c3a7e, "_printk" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x42009cfd, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "AFB5CEF6A4E33FE9BB2B3DB");

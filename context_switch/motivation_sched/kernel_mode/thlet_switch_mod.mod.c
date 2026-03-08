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
	{ 0x760a0f4f, "yield" },
	{ 0xf80f9454, "cpu_thlet_switch_start" },
	{ 0xe3ff2c41, "get_random_u64" },
	{ 0xb19a5453, "__per_cpu_offset" },
	{ 0x75a83ea1, "cpu_thlet_switch_stats_sum" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xf901390e, "wake_up_process" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x299403d2, "const_pcpu_hot" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xe8426866, "kthread_bind" },
	{ 0xb12537d8, "kthread_create_on_node" },
	{ 0xcd90e577, "sched_set_fifo" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x42009cfd, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "EBE83B951C9FE0D0510069D");

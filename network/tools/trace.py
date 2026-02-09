from bcc import BPF
from time import sleep

tracing_func = ["icenet_rx_isr", "icenet_poll", "__schedule", "finish_task_switch", "context_switch", "netif_receive_skb", "__sys_recvfrom", 
                "irq_exit", "irq_exit_rcu", "__irq_exit_rcu",
                "do_softirq", "__do_softirq", "raise_softirq", "net_rx_action",
                "pick_next_task_idle", "pick_next_task_fair", "__pick_next_task_fair", "pick_next_task_rt", "pick_next_task_dl", "pick_next_task_stop"]

tracing_func_real = ["icenet_rx_isr", "icenet_poll", "__schedule", "finish_task_switch.isra.0", "__switch_to", "netif_receive_skb", "__sys_recvfrom", 
                    "irq_exit", "irq_exit_rcu", "__irq_exit_rcu",
                    "do_softirq", "__do_softirq", "raise_softirq", "net_rx_action",
                    "pick_next_task_idle", "pick_next_task_fair", "__pick_next_task_fair", "pick_next_task_rt", "pick_next_task_dl", "pick_next_task_stop"]

traing_in = [True, True, True, True, True, True, True, 
             True, True, True,
             True, True, True, True,
             True, True, True, True, True, True]

traing_ret = [True, True, True, False, True, True, True, 
              True, True, True, 
              True, True, True, True,
              True, True, True, True, True, True]

prog = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

#define FUNC_NAME_LEN 25

struct data_t {
    u32 pid;
    u32 cpu;
    u64 ts;
    char comm[TASK_COMM_LEN];
    char func_name[FUNC_NAME_LEN];
};
BPF_PERF_OUTPUT(events);

#define S_S_HARD_INTERRUPT_START     0
#define S_S_HARD_INTERRUPT_FINISHED  1
#define S_S_SCHEDULING_START         2
#define S_S_SCHEDULING_FINISHED      3
#define S_S_SOFT_INTERRUPT_START     4
#define S_S_SOFT_INTERRUPT_FINISHED  5
#define S_S_CS_START                 6
#define S_S_CS_FINISHED              7

static inline void hardware_log(int stage, uint64_t data) {
	asm volatile ("nop");
	asm volatile (".insn r 0x0B, 0, 0xe, x0, %0, %1" ::"r"(stage), "r"(data));
	asm volatile ("nop");
}

static inline void __built_in_hardware_log(size_t id, u64 software_time) {
    size_t i = id >> 1;
    if (id & 1) {
        if (i == 1)
            hardware_log(S_S_SOFT_INTERRUPT_START, software_time);
        else if (i == 2)
            hardware_log(S_S_SCHEDULING_START, software_time);
        else if (i == 4)
            hardware_log(S_S_CS_START, software_time);
    } else {
        if (i == 1)
            hardware_log(S_S_SOFT_INTERRUPT_FINISHED, software_time);
        else if (i == 2)
            hardware_log(S_S_SCHEDULING_FINISHED, software_time);
        else if (i == 4)
            hardware_log(S_S_CS_FINISHED, software_time);
    }
}

static void __built_in_tracing(struct pt_regs *ctx, const char* name, size_t id) {
    struct data_t data = {};

    data.pid = bpf_get_current_pid_tgid();
    data.cpu = bpf_get_smp_processor_id();
    data.ts = bpf_ktime_get_ns();
    bpf_get_current_comm(&data.comm, sizeof(data.comm));
    __builtin_memcpy(data.func_name, name, FUNC_NAME_LEN);
    // __built_in_hardware_log(id, data.ts);

    events.perf_submit(ctx, &data, sizeof(data));
}

int tracing_rpc_in(struct pt_regs *ctx) { __built_in_tracing(ctx, "user_rpc_in", 0); return 0; }

int tracing_rpc_ret(struct pt_regs *ctx) { __built_in_tracing(ctx, "user_rpc_ret", 0); return 0; }

"""

def generate_func(func, id):
    return "int tracing_%s(struct pt_regs *ctx) { __built_in_tracing(ctx, \"%s\", %d); return 0; }\n" % (func, func, id)

# generate all traing function
for i in range(len(tracing_func)):
    func_name = tracing_func[i]
    prog += generate_func(func_name + "_in", i << 1) + generate_func(func_name + "_ret", (i << 1) & 1)

b = BPF(text = prog)

for i in range(len(tracing_func)):
    func_name = tracing_func[i]
    func_name_real = tracing_func_real[i]

    b.attach_kprobe(event = func_name_real, fn_name = "tracing_" + func_name + "_in")
    b.attach_kretprobe(event = func_name_real, fn_name = "tracing_" + func_name + "_ret")

# attach server probe
b.attach_uprobe(name="/root/benchmark/threadlet/new/fpga_server", sym="handle_rpc", fn_name="tracing_rpc_in")
b.attach_uretprobe(name="/root/benchmark/threadlet/new/fpga_server", sym="handle_rpc", fn_name="tracing_rpc_ret")

print("%-18s %-4s %-16s %-6s %-10s" % ("TIME(ns)", "CPU", "COMM", "PID", "NAME"))

start = 0
def print_event(cpu, data, size):
    global start
    event = b["events"].event(data)
    if start == 0:
        start = event.ts
    time_s = (float(event.ts - start)) / 1000000000
    print("%-18.9f %-4d %-16s %-6d %s" % (float(time_s), event.cpu, event.comm, event.pid, event.func_name))

b["events"].open_perf_buffer(print_event)

try:
    while True:
        b.perf_buffer_poll()
except KeyboardInterrupt:
    exit()
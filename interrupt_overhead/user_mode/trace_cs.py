from bcc import BPF
import subprocess
import sys
import re

lat_ctx = "/home/qxh/benchmark/threadlet/interrupt_overhead/user_mode/a.out"

prog = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <linux/kernel.h>
#include <linux/percpu.h>
#include <asm/irq_vectors.h>
#include <linux/irq.h>

struct data_t {
    u64 ts;
    u32 vector;
};

BPF_PERF_OUTPUT(events);

int test_in(struct pt_regs *ctx) {
    struct data_t data = {};
    data.ts = bpf_ktime_get_ns();

    data.vector = irq_to_desc(11)->irq_data.hwirq;
    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}
"""

def parse(input):
    ovr_match = re.search(r"ovr=([\d.]+)", input)
    ovr_value = float(ovr_match.group(1)) if ovr_match else 0

    lines = re.findall(r"^(\d+)\s+([\d.]+)", input, re.MULTILINE)
    lat_value = 0
    if lines:
        last_procs, last_lat = lines[-1]
        lat_value = float(last_lat)
    return ovr_value * 1000, lat_value * 1000

tracing_func = ["cs_mm", "cs_finished"]
linux_real = ["switch_mm_irqs_off", "finish_task_switch.isra.0"]

b = BPF(text = prog)

# for i in range(len(tracing_func)):
#     name = tracing_func[i]
#     linux_name = linux_real[i]

#     b.attach_kprobe(event = linux_name, fn_name = "tracing_" + name + "_in")
#     b.attach_kretprobe(event = linux_name, fn_name = "tracing_" + name + "_ret")

b.attach_uprobe(name=lat_ctx, sym="test", fn_name="test_in")

start = 0
def print_event(cpu, data, size):
    global start
    event = b["events"].event(data)
    if start == 0:
        start = event.ts

    print("%-18.9f" % (float(event.ts - start)))

b["events"].open_perf_buffer(print_event)

test_process = subprocess.Popen(
    ["taskset", "-c", "21", lat_ctx],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)
stdout_data, stderr_data = test_process.communicate()
print(stdout_data)
print(stderr_data)

try:
    while True:
        b.perf_buffer_poll()
except KeyboardInterrupt:
    exit()
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

#define FUNC_NAME_LEN 25

struct data_t {
    u32 pid;
    u64 ts;
    char comm[TASK_COMM_LEN];
    char func_name[FUNC_NAME_LEN];
};
BPF_PERF_OUTPUT(events);


static void __built_in_tracing(struct pt_regs *ctx, const char* name) {
    struct data_t data = {};

    data.pid = bpf_get_current_pid_tgid();
    data.ts = bpf_ktime_get_ns();
    bpf_get_current_comm(&data.comm, sizeof(data.comm));
    __builtin_memcpy(data.func_name, name, FUNC_NAME_LEN);

    events.perf_submit(ctx, &data, sizeof(data));
}

#define ADD_TRACING(NAME) \
static int tracing_##NAME(struct pt_regs *ctx) { \
    __built_in_tracing(ctx, #NAME); \
    return 0; \
}

// ADD_TRACING(icenet_poll)
// ADD_TRACING(icenet_rx_isr)
// ADD_TRACING(__schedule)
// ADD_TRACING(finish_task_switch)

int tracing_icenet_poll_in(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "icenet_poll_in");
    return 0;
}
int tracing_icenet_poll_ret(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "icenet_poll_ret");
    return 0;
}

int tracing_icenet_rx_isr_in(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "icenet_rx_isr_in");
    return 0;
}
int tracing_icenet_rx_isr_ret(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "icenet_rx_isr_ret");
    return 0;
}

int tracing___schedule_in(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "__schedule_in");
    return 0;
}
int tracing___schedule_ret(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "__schedule_ret");
    return 0;
}

int tracing_finish_task_switch_in(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "finish_task_switch_in");
    return 0;
}
int tracing_finish_task_switch_ret(struct pt_regs *ctx) {
    __built_in_tracing(ctx, "finish_task_switch_ret");
    return 0;
}



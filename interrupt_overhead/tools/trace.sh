#!/bin/bash

if [ "$EUID" -ne 0 ]; then 
  echo "Running on sudo"
  exit
fi

BENCHMARK_CMD="../user_mode/recv"
TRACE_DIR="/sys/kernel/debug/tracing"

# initialize
echo 0 > $TRACE_DIR/tracing_on
echo function_graph > $TRACE_DIR/current_tracer
echo "" > $TRACE_DIR/set_ftrace_filter
echo "" > $TRACE_DIR/set_graph_function

echo __netif_receive_skb_list_core > $TRACE_DIR/set_graph_function

sudo taskset -c 24 $BENCHMARK_CMD &
BPID=$!

echo $BPID > $TRACE_DIR/set_ftrace_pid
echo 1 > $TRACE_DIR/tracing_on
echo 2 > $TRACE_DIR/max_graph_depth

wait $BPID

echo 0 > $TRACE_DIR/tracing_on

cat $TRACE_DIR/trace > schedule_detail.log

echo > $TRACE_DIR/set_ftrace_pid
echo nop > $TRACE_DIR/current_tracer

# sudo -s
# cd /sys/kernel/debug/tracing
# # 1. 停止当前追踪
# echo 0 > tracing_on

# # 2. 清除所有过滤器和追踪数据
# echo > set_ftrace_filter
# echo > set_graph_function
# echo > trace

# # 3. 设置追踪目标函数
# echo __netif_receive_skb_list > set_graph_function

# # 4. 设置使用 function_graph 追踪器
# echo function_graph > current_tracer

# # 5. 可选：记录子函数的绝对时间戳
# echo funcgraph-abstime > trace_options

# # 6. 可选：限制追踪深度（若只想看最上层，设为1；想看内部细节，设为5）
# echo 5 > max_graph_depth

# # 7. 正式开启追踪
# echo 1 > tracing_on

# # 此时让系统产生一些网络流量...

# # 8. 停止追踪并读取结果
# echo 0 > tracing_on
# cat trace | head -n 50
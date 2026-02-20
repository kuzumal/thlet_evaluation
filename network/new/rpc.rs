const KEY_SIZE: usize = 10;
const VALUE_SIZE: usize = 100;

#[repr(u32)]
#[derive(Debug, PartialEq)]
enum RpcType {
  Put = 0,
  Get = 1,
}

#[repr(C, packed)]
struct Rpc {
  magic: u64,
  timestamp: u64,
  rpc_type: u32,
  key: [u8; KEY_SIZE],
  value: [u8; VALUE_SIZE],
}

impl Rpc {
  pub fn handle(&mut self) {
    if self.rpc_type == RpcType::Put {

    } else if self.rpc_type == RpcType::Get {
      
    }
  }
}

const RING_BUFFER_SIZE: usize = 1024;

#[repr(align(64))]
struct RingBuffer {
  buffer: [Option<Rpc>; RING_BUFFER_SIZE],
  head: usize,
  tail: usize,
}

impl RingBuffer {
  pub const fn new() -> Self {
    const EMPTY_RPC: Option<Rpc> = None;
    Self {
      buffer: [EMPTY_RPC; RING_BUFFER_SIZE],
      head: 0,
      tail: 0,
    }
  }

  pub fn push(&mut self, rpc: Rpc) -> bool {
    let next_head = (self.head + 1) % RING_BUFFER_SIZE;
    if next_head == self.tail {
      return false;
    }
    self.buffer[self.head] = Some(rpc);
    self.head = next_head;
    true
  }

  pub fn pop(&mut self) -> Option<Rpc> {
    if self.head == self.tail {
      return None;
    }
    let rpc = self.buffer[self.tail].take();
    self.tail = (self.tail + 1) % RING_BUFFER_SIZE;
    rpc
  }
}

static QUEUE_0: Mutex<RingBuffer> = Mutex::new(RingBuffer::new());
static QUEUE_1: Mutex<RingBuffer> = Mutex::new(RingBuffer::new());

fn kernel_kvrpc_server_threadlet() {
  use crate::net::socket::Socket;

  let arg = ostd::arch::riscv::threadlet::threadlet_get_a0() as usize;
  let port = (arg & 0xFFFF) as u16;

  let mut ip = Ipv4Address::new(172, 16, 0, 2);
  if let Some(args) = kernel_cmdline().get_module_args("icenet") {
    for a in args {
      if let ModuleArg::KeyVal(name, value) = a {
        if name.as_bytes() == b"addr" {
          if let Ok(s) = core::str::from_utf8(value.as_bytes()) {
            let mut it = s.split('/');
            if let Some(ip_s) = it.next() {
              let parts: alloc::vec::Vec<&str> = ip_s.split('.').collect();
              if parts.len() == 4 {
                  if let (Ok(a), Ok(b), Ok(c), Ok(d)) = (
                    parts[0].parse::<u8>(),
                    parts[1].parse::<u8>(),
                    parts[2].parse::<u8>(),
                    parts[3].parse::<u8>(),
                  ) {
                    ip = Ipv4Address::new(a, b, c, d);
                  }
              }
            }
          }
        }
      }
    }
  }

  let sock = crate::net::socket::ip::datagram::DatagramSocket::new(false);

  let addr = SocketAddr::IPv4(ip, port);
  match sock.bind(addr) {
      Ok(()) => {
        ostd::early_println!("[kvrpc-threadlet] bound UDP {}:{}", ip, port);
      }
      Err(e) => {
        ostd::early_println!(
          "[kvrpc-threadlet] bind failed {}:{} err={:?}",
          ip,
          port,
          e
        );
        return;
      }
  }

  // Log CPU and threadlet id where this waiter runs.
  let preempt_guard = ostd::task::disable_preempt();
  let cpu_id = preempt_guard.current_cpu();
  drop(preempt_guard);
  let tid = ostd::arch::riscv::threadlet::threadlet_current();
  ostd::early_println!(
    "[kvrpc-threadlet] started (listening on {}:{}) on CPU {} threadlet_id={} arg={:#x}",
    ip,
    port,
    cpu_id.as_usize(),
    tid,
    arg
  );

  let mut buf = [0u8; 2048];
  let mut rc = 0;

  loop {
    let mut writer = VmWriter::from(&mut buf[..]).to_fallible();
    match sock.recvmsg(&mut writer, SendRecvFlags::empty()) {
        Ok((n, mh)) => {    
          // ostd::arch::riscv::threadlet::threadlet_syn_print(5, 16);
          if n < core::mem::size_of::<Rpc>() {
            continue;
          }
          let rpc_ptr = buf.as_ptr() as *const Rpc;
          let rpc = unsafe { &*rpc_ptr };
          let magic = u64::from_be(rpc.magic);
          if magic != 0x7777 {
            continue;
          }

          let target_queue = if rc % 2 == 0 { &QUEUE_0 } else { &QUEUE_1 };
          rc += 1;
          {
            let mut rb = target_queue.lock();
            let next_head = (rb.head + 1) % RB_SIZE;
            
            if next_head != rb.tail {
              rb.data[rb.head] = Some(rpc);
              rb.head = next_head;
            }
          }

          // ostd::early_println!(
          //     "[kudp-threadlet][cpu{}] I/O wakeup: recv {} bytes from {:?} preview='{}'",
          //     cpu_id.as_usize(),
          //     n,
          //     mh.addr(),
          //     preview
          // );
        }
        Err(e) => {
          ostd::early_println!("[kudp-threadlet] recv error: {:?}", e);
          break;
        }
      }
  }
}

fn kernel_kvrpc_worker_threadlet() {
  let worker_id = ostd::arch::riscv::threadlet::threadlet_get_a0() as usize;
  let rpc_queue = if worker_id == 0 { &QUEUE_0 } else { &QUEUE_1 };

  loop {
    let mut rpc_to_process: Option<Rpc> = None;

    {
      let mut rb = rpc_queue.lock();
      if rb.head != rb.tail {
        rpc_to_process = rb.data[rb.tail].take();
        rb.tail = (rb.tail + 1) % RB_SIZE;
      }
    }

    if let Some(rpc) = rpc_to_process {
      let rpc_type = rpc.rpc_type;
      let key = &rpc.key;
          
      if rpc_type == RpcType::Get as u32 {
        // get_logic(key) -> value
      } else if rpc_type == RpcType::Put as u32 {
        // put_logic(key, &rpc.value)
      }
    } else {
      ostd::arch::riscv::threadlet::threadlet_yield();
    }
  }
}

fn handle_kv_operation(rpc: &Rpc) {
  let now = ostd::arch::riscv::read_cycle();
  let diff = now - u64::from_be(rpc.timestamp);
}

fn rpc_test() {
  ThreadOptions::threadlet_new_auto_with_arg(kernel_kvrpc_worker_threadlet, 3, 0, true);
  ThreadOptions::threadlet_new_auto_with_arg(kernel_kvrpc_worker_threadlet, 3, 1, true);
  ThreadOptions::threadlet_new_auto_with_arg(kernel_kvrpc_server_threadlet, 3, 5556, true);
}
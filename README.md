# Design Notes
The MSX system is based on hardware that follows a standard architecture. It is relatively simple and was designed to boot consumer software, mainly games.
Halfway between an eccentric experiment and a personal challenge, we want to carry out a project that makes an MSX system provide:

* A communication channel with external systems, enabling data transfer between the MSX expansion port and a USB port
* Process execution through time multiplexing based on the native MSX interrupt signal (VDP) every 20 ms (50 Hz): a real-time operating system (RTOS)

We split the project into two development phases that can be carried out independently. On one side, the software that will provide the proposed features; on the other, the hardware interface development (and its associated software) that will enable the intended communication.

Ultimately, the project aims to establish a platform to evaluate MSX hardware issues, execute and monitor tasks, and transfer data with modern external systems.

## MSX Architecture
First, we need to understand the basic operating principles of an MSX system. A superficial look highlights obvious aspects such as operational simplicity. However, this apparent simplicity imposes strong constraints when trying to achieve the proposed goals.

These systems were designed to execute programs directly and quickly. After power-on, the MSX executes a startup phase from code located in ROM. This processor always reads the first instruction at address `0x0000`. In other words, the first code it reads is at the first addresses. That code contains routines that run only during startup, and others that can run later for other programs. Tasks performed during this initial phase are mainly hardware initialization and RAM tests.

MSX systems appeared in 1983 and were based on a Zilog Z80 processor. This processor has an 8-bit data bus and a 16-bit address bus. Consequently, it can access 65,536 memory addresses. MSX systems shipped with between 16 KB and 64 KB of RAM, but could be expanded with additional resources and therefore access more than 64 KB. The engineers who created the MSX introduced a feature that exposes memory address ranges _visible_ to the processor on external hardware. They created the concepts of _slots_ and _pages_. The idea is to present the Z80 with an **address space** as a single interface for exchanging information (instructions or data) with the outside world. We should not confuse this address space with a specific physical memory, but rather see it as an abstraction layer. This space is divided into 4 subspaces called _pages_, each covering a contiguous 16 KB address range:

| Page | Address Range |
|:---|:---|
| 0 | `0x0000-0x3FFF` |
| 1 | `0x4000-0x7FFF` |
| 2 | `0x8000-0xBFFF` |
| 3 | `0xC000-0xFFFF` |

[The MSX standard](https://map.grauw.nl/resources/system/msxtech.pdf) does not precisely define the concept of _slot_. In this document we define it as **a set of hardware resources that can be accessed by the Z80 through a 16-bit address**. [The MSX standard](https://map.grauw.nl/resources/system/msxtech.pdf) allows up to 4 _slots_, indexed from 0 to 3. Slots can also be divided into addressable 16 KB subspaces (pages). Through slots, the processor can perform:

* Memory access operations on any slot
* I/O operations to external devices associated with a slot

Through a procedure called _Memory Switching_, it is possible to bind pages to slots using I/O calls to another native MSX device called the PPI. This hardware part establishes communication channels between address-space pages and resource pages. We can think of _Memory Switching_ as setting up channels between them. **Channels can only be established between pages with the same index**. Once done, the processor can continue with instruction fetch.

Both the process that performs _Memory Switching_ and the one that runs user-oriented tasks must satisfy certain conditions. For example, if _Memory Switching_ is executed while the Z80 PC or SP registers point to an address in a switched page, the Z80 will not be able to continue execution of the original program. **It is the programmer's responsibility to execute _Memory Switching_ on pages not referenced by the PC and SP context registers.**

The standard assumes a _slot 0_, which includes certain resources internally/integrated in the system. Its exact configuration is not specified, but it appears to include at least 32 KB of ROM. Slots other than 0 may have a physical projection as an expansion port. In this regard, the standard was flexible and each manufacturer could enable different hardware configurations. To make the idea concrete, below is an example of a _Memory Switching_ procedure.

If the Z80 executes:
```
out (0xAB), 0x82
```
This instruction sets the operating mode of the PPI subsystem. It has three internal ports connected to different groups of internal hardware: A, B and C. The ports can transfer data between hardware and the Z80. The relevant port during _Memory Switching_ is A, linked to any device other than the keyboard and cassette unit. Specifically, here we configure port A in _normal_ mode, with data direction from the Z80 to the outside (output).

Although the configurations of ports B and C are not relevant in this project, we briefly describe them. Port B is linked to keyboard hardware. Specifically, we set it in _normal_ mode, with direction from keyboard to Z80. Port C is linked to the cassette unit and the Caps Lock LED. In fact, half of this port bus is dedicated to cassette and the other half to the LED. It is also configured in _normal_ mode with direction from Z80 to the outside.

If the Z80 executes:

```
out (0xA8), 0x00
```
_Memory Switching_ takes place. Channel mapping between pages is done through the _Primary Slot Register_ (PSR), i.e. an 8-bit value divided into 4 groups of 2 bits. Each pair of 2 bits (4 possible values) represents the slot number connected to each page: `address space <-> slot`. The position of the 2-bit pair represents the page number. In this case, the _Primary Slot Register_ is `0x00`. That means:

```
Bit order: 7  6  5  4  3  2  1  0
      PSR: 0  0  0  0  0  0  0  0
      ---------------------------
     Slot: 0     0     0     0
     Page: 3     2     1     0
```
So _Memory Switching_ maps all address-space pages to the same slot. This is in fact the default hardware configuration at power-on start.

It is important to note that slot hardware configuration depends on the specific MSX model. For example, the Sony MSX HB-55P included this _slot 0_ configuration:
| Page | Address Range | Resource |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | ROM |
| 1 | `0x4000-0x7FFF` | ROM Basic |
| 2 | `0x8000-0xBFFF` | ROM Personal Data Bank |
| 3 | `0xC000-0xFFFF` | RAM |

That is, the HB-55P had 16 KB of native RAM. The MSX HB-75P instead had this _slot 0_ configuration:

| Page | Address Range | Resource |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | ROM |
| 1 | `0x4000-0x7FFF` | ROM Basic |
| 2 | `0x8000-0xBFFF` | ROM Personal Data Bank |
| 3 | `0xC000-0xFFFF` | unassigned |

In this case, Sony reserved a full internal slot (_slot 2_) for RAM to reach 64 KB:

| Page | Address Range | Resource |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | RAM |
| 1 | `0x4000-0x7FFF` | RAM |
| 2 | `0x8000-0xBFFF` | RAM |
| 3 | `0xC000-0xFFFF` | RAM |


## Operating System Design
One of the first project decisions is replacing the original ROM with a new one that includes basic boot routines. To facilitate development cycles on original hardware, we design a new expansion board that includes both flash memory and RAM, since EPROM/EEPROM memories are significantly more expensive than flash and store less data. This board is installed in a cartridge slot. But if we want this flash to provide startup code to the Z80, we must remove the original ROM and intercept the ROM selection signal, because _slot 0_ is active when the MSX experiences power-on. The code stored in flash can perform memory switching to expose RAM pages from the same board. In other words, it becomes possible to run all required software without relying on integrated RAM.

This new board is based on the SST39SF010A multipurpose CMOS flash memory. It has a capacity of 1,048,576 bits with an 8-bit data access bus. The address space is 17 bits, allowing two adjacent spaces of 65,536 bits (64 KB) each. Specifically:

* From `0x00000` to `0x0FFFF`: `bootloader` selected when `ROM_OE` is asserted
* From `0x10000` to `0x1FFFF`: `startup`, RTOS and user processes

# `zlink`
The MSX system has limitations for transferring and receiving I/O data with the Z80. It does not include a hardware interface able to detect incoming data and trigger specialized interrupts. To solve this, we use a link-layer protocol called `zlink`, under `zbus`, to handle RX without `DATA_READY`-type signals.

To detect availability of a new frame versus one already processed, `zlink` uses a sequence number (`SEQ`). It also multiplexes multiple `TTY` channels, specifically from `TTY0` to `TTY15`.

The `zlink` header uses these bytes:

- `B0 = SOF5|TYPE3`
  - `SOF5` (bits `7..3`) = `0b10101`
  - `TYPE` (bits `2..0`):
    - `0` `POLL`
    - `1` `EMPTY`
    - `2` `DATA`
    - `3` `ACK`
    - `4` `NACK`
    - `5..7` reserved
- `B1 = TTY8`
  - `TTY` (`0..15`; upper bits reserved)
- `B2 = LEN8`
  - `LEN` (`0..64`)
- `B3 = SEQ` (`0..255`, modulo 256)
- `PAYLOAD = LEN bytes`
- `CRC8` (1 byte) over `B0..B(3+LEN)`, polynomial `0x07`, init `0x00`, xorout `0x00`

Total frame length is between 5 and 69 bytes. The MSX starts each receive cycle by sending a `POLL` frame with no payload. The host responds immediately with `EMPTY` if there is no pending data, or `DATA` if there is. When the MSX receives a valid `DATA` frame, it delivers the payload to the indicated `TTY` and returns `ACK` with the same `SEQ`. If a duplicate `DATA` arrives (`SEQ` equal to the last one already accepted for that `TTY`), the MSX does not process it again and responds `ACK` to avoid re-execution. `NACK` is emitted only on the `DATA` receive path when the frame is readable but fields are unacceptable for delivery (e.g. out-of-range `TTY` or invalid `LEN`); in other frame read/validation errors, data is dropped. `zlink` only transports frames and multiplexes `TTY`; assignment of each `TTY` to tasks/processes is handled by `zbus`.

`zlink` is deliberately asymmetric. The MSX side cannot directly observe bridge availability signals (`RXF#`/`TXE#`) and has no `DATA_READY`-type signal, so host -> MSX reception is based on periodic MSX polling (`POLL -> DATA|EMPTY`). Even if the host may have those signals, this information is not visible to the MSX and therefore the protocol prioritizes simplicity and robustness on the MSX side instead of enforcing full symmetry.

# `zbus`
`zbus` is the layer above `zlink`: it handles channel semantics, task isolation, and RX/TX queues. While `zlink` only transports frames, `zbus` decides who owns each `TTY`, when data can be queued, and how it is delivered to consumers.

Explicit layer behavior:

- **Channel assignment (`TTY`)**
  - `zbus` keeps a `TTY` table (up to `ZBUS_MAX_TTY=10`, from `TTY0` to `TTY9`) associated with an owner task.
  - Each `TTY` can be `attach`ed/`detach`ed, and read/write operations are valid only for the owner task.
- **TX path (MSX -> host)**
  - `zbus_write_tty()` queues frames up to `64` bytes in one queue per `TTY` (`ZBUS_TX_QUEUE_SIZE=8`).
  - If the queue is full, the frame is dropped and `tx_drop` is incremented.
  - `zbus_tick()` sends data in round-robin across active `TTY`s, capped at `ZBUS_TX_CHUNK=2` frames per tick, using `zlink_send_data()`.
- **RX path (host -> MSX)**
  - On each tick, `zbus` polls `zlink` (`zlink_poll_once()`), capped at `ZBUS_RX_CHUNK=1` frame per tick.
  - If the frame targets a valid user `TTY` and `rx_polling_enabled` is true, payload is appended to the RX circular buffer (`ZBUS_BUFFER_SIZE=96`).
  - If the RX buffer is full, remaining bytes are dropped and `rx_overflow` is incremented.
  - Tasks retrieve bytes with `zbus_read_tty()` (or `zbus_read()` in legacy mode).
- **Kernel control channel (`tty=15`)**
  - `TTY15` is reserved and not mapped to any user task.
  - Frames received on `tty=15` trigger control commands (`GET_STATS`, `GET_TASK_INFO`, `GET_TASK_LIST`, `GET_STACK_WM`) and `zbus` responds on the same channel with `RSP_*`.
- **Integrity and statistics**
  - `zbus` aggregates its own counters (`tx_drop`, `rx_overflow`, `attach_fail`) and `zlink` diagnostics (`rx_crc_err`, `rx_dup`, etc.).
  - Critical sections run with `CPU_DI()/CPU_EI()` to protect shared tables and queues.

`tty=15` is reserved for the kernel. Queries through `tty=15` form the kernel control plane. Unlike user `TTY`s, these frames are not delivered to application tasks: the kernel interprets them as diagnostic/inspection commands and returns a structured response (`RSP_*`) on the same `tty=15`. This enables monitoring and automation (stats, task state, stack watermark) without mixing this traffic with shell or normal process data.

The following sections detail kernel control messages exchanged via `tty=15` over `zbus`/`zlink`.

## Statistics
Lets you obtain transport health counters (`zbus`/`zlink`) to detect loss, errors, and saturation.

- Request host->MSX (`DATA`, `tty=15`): payload `01` (`GET_STATS`)
- Response MSX->host (`DATA`, `tty=15`): payload
  - `81` (`RSP_STATS`)
  - `status` (`00=OK`, `01=BAD_CMD`, `02=BAD_LEN`)
  - 8 `uint16 little-endian` counters (if `status=00`):
    - `tx_drop`
    - `rx_overflow`
    - `attach_fail`
    - `zlink_rx_frames_ok`
    - `zlink_rx_crc_err`
    - `zlink_rx_dup`
    - `zlink_rx_type_err`
    - `zlink_rx_len_err`
- In `openmsx/zlink.tcl`, use `zlink_dev::get_stats` (or alias `zlink::get_stats`) to send the request and view the decoded response.
- For script-oriented JSON output, use `zlink_dev::get_stats_json` (or `zlink::get_stats_json`).
  - JSON format:
    - `type` = `"kernel_stats"`
    - `status`
    - `tx_drop`, `rx_overflow`, `attach_fail`
    - `zlink_rx_frames_ok`, `zlink_rx_crc_err`, `zlink_rx_dup`, `zlink_rx_type_err`, `zlink_rx_len_err`
    - on error: `len`, `payload_hex`

## Task Data
Lets you inspect a specific task (current or by `task_id`) to know state, `tty`, `SP`, and name.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `02` (`GET_TASK_INFO`, current task), or
  - payload `02 <task_id>` (`GET_TASK_INFO` for a specific task)
- Response MSX->host (`DATA`, `tty=15`): payload
  - `82` (`RSP_TASK_INFO`)
  - `status` (`00=OK`, `02=BAD_LEN`, `03=BAD_TASK`)
  - `task_id`
  - `task_state`
  - `task_tty`
  - `task_sp_lo task_sp_hi` (`uint16 little-endian`)
  - `task_name_len`
  - `task_name[8]`
- In `openmsx/zlink.tcl`, use:
  - `zlink_dev::get_task_info` / `zlink::get_task_info`
  - `zlink_dev::get_task_info_json` / `zlink::get_task_info_json`
  - JSON format:
    - `type` = `"kernel_task_info"`
    - `status`
    - `id`, `state`, `sp` (hexadecimal, e.g. `"0x1234"`), `name_len`, `name`
    - on error: `len`, `payload_hex`

## Task List
Lets you obtain a summary of all active tasks with identifier, `tty`, and name.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `03` (`GET_TASK_LIST`)
- Response MSX->host (`DATA`, `tty=15`): one or more `RSP_TASK_LIST` frames
  - `83` (`RSP_TASK_LIST`)
  - `status` (`00=OK_FINAL`, `80=OK_MORE`, `02=BAD_LEN`)
  - `count` entries in this frame
  - `count` entries of 11 bytes:
    - `task_id`
    - `task_tty`
    - `task_name_len`
    - `task_name[8]`
- `openmsx/zlink.tcl` assembles all fragments transparently and the JSON helper returns the full aggregated list.
- In `openmsx/zlink.tcl`, use:
  - `zlink_dev::get_task_list` / `zlink::get_task_list`
  - `zlink_dev::get_task_list_json` / `zlink::get_task_list_json`
  - JSON format:
    - `type` = `"kernel_task_list"`
    - `status`
    - `count`
    - `tasks`: list of objects with `id`, `tty`, `name_len`, `name`
    - on error: `len`, `payload_hex`

## Stack Watermark

Lets you measure stack usage (current and peak) to size stacks and prevent stack overflow.
Only the `07` opcode is supported here; the legacy `04` opcode has been removed.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `07` (`GET_STACK_WM`, current task), or
  - payload `07 <task_id>` (`GET_STACK_WM` for a specific task)
- Response MSX->host (`DATA`, `tty=15`): payload
  - fixed 10-byte payload: `87 status task_id task_state stack_size_lo stack_size_hi peak_used_lo peak_used_hi current_used_lo current_used_hi`
  - `87` (`RSP_STACK_WM`)
  - `status` (`00=OK`, `02=BAD_LEN`, `03=BAD_TASK`)
  - `task_id`
  - `task_state`
  - `stack_size_lo stack_size_hi` (`uint16 little-endian`)
  - `peak_used_lo peak_used_hi` (`uint16 little-endian`, watermark)
  - `current_used_lo current_used_hi` (`uint16 little-endian`)
- In `openmsx/zlink.tcl`, use:
  - `zlink_dev::get_stack_wm` / `zlink::get_stack_wm`
  - `zlink_dev::get_stack_wm <task_id>` / `zlink::get_stack_wm <task_id>`

# IPC
The goal of IPC is to provide a minimal, predictable, low-cost mechanism for coordinating tasks and exchanging data within the system, while avoiding busy waiting and keeping the system responsive under load.

The IPC module is built around two primitives:

- `ipc_semaphore_t`: blocking synchronization between tasks (`wait/signal`).
  - Used to protect shared resources or wait for events.
  - If the resource is unavailable, the task does not spin in a loop: it blocks until another task performs `signal`.
- `ipc_queue_t`: fixed-size FIFO for producer/consumer messaging.
  - Internally uses two semaphores:
    - `items`: number of available elements to receive.
    - `slots`: free space available to send.
  - This enforces flow control: you cannot send when full, and cannot receive when empty.

Operational behavior (producer/consumer model):

1. `send`: the producer waits on `slots`, writes to the queue, and increments `items`.
2. `recv`: the consumer waits on `items`, reads from the queue, and increments `slots`.
3. When progress is not possible (full or empty queue), the task waits in the kernel and resumes when appropriate.

Integration with the scheduler:

- IPC blocking states are reflected in task state (`wait_sem`, `wait_q_send`, `wait_q_recv`).
- This allows the scheduler to run other ready tasks while one task waits on IPC.
- The result is lower global latency and better CPU utilization than polling.

In summary, this IPC is the foundation of internal system messaging: simple enough to be robust on Z80, and expressive enough to build channels between tasks without tight coupling.


# Build
The project uses SDCC (`sdcc`) and the `sdasz80` assembler. SDCC is a standard C compiler suite (ANSI C89, ISO C99, ISO C11, ISO C23), retargetable and optimizing, focused on Intel MCS51-based microprocessors (8031, 8032, 8051, 8052, etc.), Maxim DS80C390 variants (formerly Dallas), Freescale HC08-based microcontrollers (formerly Motorola) (hc08, s08), Zilog Z80-based MCUs (Z80, Z80N, Z180, SM83, Rabbit 2000, 2000A, 3000A, SM83, TLCS-90, eZ80, R800), Padauk (pdk14, pdk15), STMicroelectronics STM8, MOS 6502, and WDC 65C02.

ASxxxx assemblers are a family of microprocessor assemblers written in C. This collection contains cross-assemblers for series 1802, S2650, SC/MP, 4040(4004), MPS430, 6100, 61860, 6500, 6800(6802/6808), 6801(6803/HD6303), 6804, 6805, 68HC(S)08, 6809, 68HC11, 68HC(S)12, 68HC16, 68CF 68K, 740, 78K/0, 78K/0S, 8008, 8008S, 8048(8041/8022/8021), 8051, 8085(8080), AT89LP, 8X300(8X305), COP4, COP8, DS8XCXXX, AVR, EZ8, EZ80, F2MC8L/FX, F8/3870, GameBoy(Z80), H8/3xx, Cypress PSoC(M8C), PDP11, PIC, Rabbit 2000/3000, RS08, ST6, ST7, ST8, ST9, SX, TLCS90, Z8, Z80(HD64180, ZXN, 8080, 8085), and Z280.

You can find hardware/platform manifests (targets) under `targets/*.mk`. These files define both build layout and boot parameters. To build the default target (`ztick`):

```bash
make bootstrap
```
You can also build a specific target:

```bash
make TARGET=ztick bootstrap
make TARGET=ztick-unitcard bootstrap
make TARGET=hb-55p bootstrap
```
To remove object code created in previous steps:

```bash
make clean
```
You can include these parameters to customize the build:

* `TARGET`: target name referring to `targets/<target>.mk`
* `IMAGE_LAYOUT`: determines ROM image flow. Default values are defined in the target manifest, but can be overridden:
  * `flat64`: a single 64 KB ROM.
  * `flash2x64`: two 64 KB ROMs concatenated into one final 128 KB image.

Example with explicit override:

```bash
make TARGET=hb-55p IMAGE_LAYOUT=flash2x64 bootstrap
```

There are two image formats or layouts. The choice between one and the other depends on whether we use a physical MSX with the bootstrapping board.

The `flat64` layout is used in targets such as `ztick` and generates a single image: `bin/<target>/<ROM_IMAGE_NAME>`. It is `65536` bytes and boot code enters directly through `startup.s`.

The `flash2x64` layout is used for physical targets such as `hb-55p`. It generates two primary images and one composite image built from them:

* `bin/<target>/bootloader.rom` (64 KB)
* `bin/<target>/startup.rom` (64 KB)
* `bin/<target>/<ROM_IMAGE_NAME>` (concatenation, 128 KB)

The concatenated image can then be programmed into SST39SF010A.

# Running with openMSX
OpenMSX is a free and open-source emulator for MSX, MSX2, MSX2+, MSX turboR, and related hardware. Its repository motto is “the MSX emulator that aims for perfection”, i.e. an emulator focused on high fidelity and accuracy.

For complete `zlink` tests (RX/TX), using upstream `openMSX 21` is recommended because this version includes the `ProgrammableDevice` extension. This is a programmable virtual MSX device that you can connect to a list of I/O ports. It acts as a bridge between the emulated Z80 and the openMSX host environment using Tcl callbacks. The official manual describes it as a virtual device connectable “on the fly” to user I/O ports and useful for creating bidirectional communication between the virtual MSX and the host system.

We can use precompiled binaries without installing [openMSX](https://github.com/openMSX/openMSX/releases) on the system or replacing the existing version:

```bash
mkdir -p ~/opt
cd ~/opt
curl -L -o openmsx-21.0-linux-x86_64-bin.zip \
  https://github.com/openMSX/openMSX/releases/download/RELEASE_21_0/openmsx-21.0-linux-x86_64-bin.zip
unzip -q openmsx-21.0-linux-x86_64-bin.zip -d openmsx-21.0
```

After building, you can start openMSX with the project script. To use the system version:

```bash
./scripts/setup_openmsx.sh ztick
./scripts/setup_openmsx.sh ztick-unitcard
./scripts/setup_openmsx.sh hb-55p
```

`ztick-unitcard` simulates a staged boot flow for a ROM+RAM card in primary slot 1:

* bootloader executes from slot 0 pages 0-1
* startup executes from slot 1 pages 0-1
* task/data RAM is in slot 1 pages 2-3

If we want to force local version 21:

```bash
export OPENMSX_BIN="$HOME/opt/openmsx-21.0/bin/openmsx"
./scripts/setup_openmsx.sh --target ztick
```

If you want to pause execution at the xsh entry point (`_main_xsh`), enable the breakpoint explicitly:

```bash
./scripts/setup_openmsx.sh ztick --bp-xsh
```

By default, `setup_openmsx.sh` runs a small diagnostic self-check (`get_task_list`, `get_stack_wm`, and `shell_cmd help`) right after installing `zlink`. If you prefer a clean start:

```bash
./scripts/setup_openmsx.sh ztick --no-self-check
```

## Shell `xsh`
The shell (`xsh`) runs in the task registered as `xsh` over its `tty` (`zbus`). Its entry point is `_main_xsh`. At startup, it shows this prompt:

```text
Z-Tick xsh
ztick> 
```

Implemented functionality (current code state):

* Auto-attaches to the current `tty` via `zbus_tty_get_current()`.
* Interactive input with echo:
  * printable ASCII characters (`32..126`)
  * `Backspace`/`Delete` with visual erase
  * `CR`/`LF` to execute command
* Maximum input line length: `55` useful characters (`XSH_LINE_MAX=56`, 1 byte reserved for `\0`).
* Simple space-separated parser (no quoting/escaping), with at most 3 total tokens (`XSH_ARGV_MAX=3`).
* If a command does not exist: `unknown command`.
* If syntax is invalid: shows command `usage` line.

Available commands:

* `help`: Shows `commands: help cfg tasks start stop weight heap stack stats`
* `cfg`: Shows compiled configuration (`max_tasks`, `task_heap`, `zbus` parameters, etc.).
* `tasks [task_id]`: Without args: counts active tasks and shows `task <id> name=<name>`. With `task_id`: shows details `state`, `tty`, `w` (weight), `b` (budget), `sp`, `name`.
* `start <task_name> [weight]`: Starts a task from the registry (`task_registry`), currently `b`, `c` and `rchk`. Optional `weight` in range `1..3`.
* `stop <task_name>`: Requests stop for a running task.
* `weight <task_id> <1..3>`: Changes scheduling weight of a task.
* `heap [task_id]`: Shows per-task heap status: `free`, `free_blocks`, `used_blocks`.
* `stack [task_id]`: Shows stack metrics: `size`, `peak`, `free_peak`, `current`.
* `stats`: Shows 3 blocks:
  * `zbus`: `tx_drop`, `rx_overflow`, `attach_fail`
  * `zlink`: `ok`, `crc`, `dup`, `type`, `len`
  * `ipc`: `q_used`, `q_cap`

After boot, tasks are created from the target manifest via `BOOT_AUTOSTART` (currently `xsh:2 b:1` in bundled targets). Task `c` can be started on demand with:

```tcl
zlink_dev::shell_cmd "start b"
zlink_dev::shell_cmd "start c"
zlink_dev::shell_cmd "start b 3"
zlink_dev::shell_cmd "start rchk safe"
zlink_dev::shell_cmd "weight 1 2"
zlink_dev::shell_cmd "stop b"
zlink_dev::shell_cmd "stop c"
```

From the openMSX Tcl console, you can inject a shell command via:

```tcl
zlink_dev::shell_cmd help
zlink_dev::shell_cmd "tasks"
zlink_dev::shell_cmd "stack 0"
zlink_dev::shell_cmd "help" 0 auto -decode 1
```

Raw equivalent (without helper):

```tcl
zlink_dev::queue_text 0 "help\r"
```

To disable text decoding:

```tcl
zlink_dev::shell_cmd "help" 0 auto -decode 0
```

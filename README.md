# 252-OS — Simple Operating System Simulator

> Bài tập lớn môn **Hệ Điều Hành (CO2018)** — HCMC University of Technology, VNU-HCM  
> Khóa học: **252** | License: MIT

---

## 📋 Mục lục

- [Giới thiệu](#-giới-thiệu)
- [Tính năng](#-tính-năng)
- [Cấu trúc dự án](#-cấu-trúc-dự-án)
- [Yêu cầu hệ thống](#-yêu-cầu-hệ-thống)
- [Cài đặt & Build](#-cài-đặt--build)
- [Chạy chương trình](#-chạy-chương-trình)
- [Chạy test tự động](#-chạy-test-tự-động)
- [Visualize Gantt Chart](#-visualize-gantt-chart)
- [Cấu hình OS](#-cấu-hình-os)
- [Định dạng file input](#-định-dạng-file-input)

---

## 🧠 Giới thiệu

**252-OS** là một trình giả lập hệ điều hành đơn giản viết bằng C, mô phỏng các cơ chế cốt lõi của một hệ điều hành thực tế, bao gồm:

- Lập lịch CPU đa cấp độ ưu tiên (Multi-Level Queue)
- Quản lý bộ nhớ ảo theo cơ chế phân trang (Paging 32/64-bit)
- Bộ nhớ vật lý và swap memory
- Hệ thống gọi hệ thống (System calls)
- Đa CPU với đồng bộ hóa luồng (pthreads)

---

## ✨ Tính năng

| Thành phần | Mô tả |
|---|---|
| **MLQ Scheduler** | Multi-Level Queue với 140 mức ưu tiên, time-slice theo priority |
| **Paging (32/64-bit)** | Bộ nhớ ảo phân trang, hỗ trợ page table 4 cấp (mm64) |
| **Swap Memory** | Hoán đổi trang giữa RAM và swap device |
| **Multi-CPU** | Nhiều CPU song song, đồng bộ qua `pthread_mutex` |
| **System Calls** | Syscall mô phỏng: cấp phát/giải phóng bộ nhớ, đọc/ghi |
| **Test Runner** | Script tự động chạy toàn bộ test case với timeout và lọc module |
| **Gantt Chart** | Visualize lịch thực thi CPU bằng Python/Matplotlib |

---

## 📁 Cấu trúc dự án

```
252-OS/
├── src/                    # Mã nguồn C chính
│   ├── os.c                # Entry point, khởi tạo OS, vòng lặp chính
│   ├── sched.c             # Bộ lập lịch MLQ (Multi-Level Queue)
│   ├── cpu.c               # Mô phỏng CPU, thực thi instruction
│   ├── timer.c             # Quản lý time slot
│   ├── loader.c            # Nạp tiến trình từ file
│   ├── queue.c             # Cấu trúc dữ liệu hàng đợi
│   ├── mm.c                # Quản lý bộ nhớ ảo (paging)
│   ├── mm64.c              # Paging 64-bit, page table 4 cấp
│   ├── mm-memphy.c         # Bộ nhớ vật lý (RAM & Swap)
│   ├── mm-vm.c             # Virtual memory mapping
│   ├── mem.c               # Khởi tạo memory region
│   ├── libmem.c            # Thư viện quản lý heap (malloc/free)
│   ├── libstd.c            # Hàm tiện ích chuẩn
│   ├── syscall.c           # Dispatcher system call
│   ├── sys_mem.c           # Syscall: quản lý bộ nhớ
│   ├── sys_kmem.c          # Syscall: kernel memory
│   ├── sys_listsyscall.c   # Danh sách syscall
│   └── syscall.tbl         # Bảng định nghĩa syscall
│
├── include/                # Header files
│   ├── os-cfg.h            # Cấu hình toàn cục (bật/tắt tính năng)
│   ├── mm.h                # Định nghĩa cấu trúc bộ nhớ
│   ├── mm64.h              # Cấu trúc paging 64-bit
│   ├── sched.h             # Định nghĩa scheduler
│   ├── common.h            # Kiểu dữ liệu dùng chung
│   └── ...                 # Các header khác
│
├── input/                  # File cấu hình test case
│   ├── sched               # Test scheduler cơ bản
│   ├── os_1_mlq_paging     # Test MLQ + Paging, 1 CPU
│   ├── os_2_mlq_paging     # Test MLQ + Paging, 2 CPU
│   ├── os_sc               # Test system call
│   ├── test_swap           # Test swap memory
│   └── proc/               # Mã giả của các tiến trình
│
├── output/                 # Kết quả chạy (*.output)
├── my_output/              # Output mẫu để so sánh
├── obj/                    # Object files (được tạo khi build)
├── Makefile                # Build system
├── run.sh                  # Script chạy toàn bộ test case tự động
├── gantt.py                # Visualize lịch CPU dưới dạng Gantt chart
└── os                      # Binary sau khi build
```

---

## 🛠 Yêu cầu hệ thống

**Chạy native (Linux / WSL / macOS):**
- `gcc` ≥ 7.0
- `make`
- `pthread` (thường có sẵn)
- `python3` + `matplotlib` *(chỉ cần cho Gantt chart)*

**Chạy qua Docker:**
- Docker Desktop

---

## ⚙️ Cài đặt & Build

### Cách 1: Build trực tiếp (Linux/WSL)

```sh
# Build toàn bộ dự án
make

# Xóa file build
make clean
```

### Cách 2: Dùng Docker

```sh
# Build Docker image
docker build -t final-btl .

# Chạy container (mount thư mục hiện tại)
docker run -it --name final-btl -v ${PWD}:/app -w /app final-btl
```

---

## 🚀 Chạy chương trình

Sau khi build xong, chạy OS simulator với một file input cụ thể:

```sh
./os <tên_testcase>
```

Ví dụ:

```sh
# Chạy test MLQ + Paging với 1 CPU
./os os_1_mlq_paging

# Chạy test scheduler cơ bản
./os sched

# Chạy test system call
./os os_sc
```

Kết quả được ghi vào `output/<tên_testcase>.output`.

---

## 🧪 Chạy test tự động

Script `run.sh` tự động chạy toàn bộ test trong thư mục `input/`, hỗ trợ:
- **Lọc module**: chỉ chạy nhóm test cụ thể
- **Timeout per test**: mặc định 90 giây
- **Tổng kết**: Passed / Crashed / Timeout / Skipped

```sh
bash run.sh
```

### Lọc module qua `test_config.conf`

Tạo file `test_config.conf` tại thư mục gốc để chỉ định nhóm test muốn chạy:

```ini
# Chạy tất cả
MODULES=all

# Chỉ chạy paging và scheduler
MODULES=paging,scheduler

# Chỉ chạy syscall
MODULES=syscall
```

| Module | Pattern nhận diện tên testcase |
|---|---|
| `paging` | chứa `paging` |
| `syscall` | chứa `syscall` hoặc `_sc` |
| `scheduler` | chứa `sched` hoặc `mlq` |
| `memory` | chứa `memory` |
| `synchronization` | chứa `sync` |

**Tùy chỉnh timeout (giây):**

```sh
TESTCASE_TIMEOUT=60 bash run.sh
```

---

## 📊 Visualize Gantt Chart

Sau khi chạy OS, dùng `gantt.py` để hiển thị biểu đồ Gantt lịch thực thi CPU:

```sh
python3 gantt.py <input_file> <output_file>
```

Ví dụ:

```sh
python3 gantt.py input/os_1_mlq_paging output/os_1_mlq_paging.output
```

---

## 🔧 Cấu hình OS

Chỉnh sửa `include/os-cfg.h` để bật/tắt tính năng:

```c
#define MLQ_SCHED    1      // Bật Multi-Level Queue scheduler
#define MAX_PRIO     140    // Số mức priority (0 = cao nhất)

#define MM_PAGING           // Bật cơ chế phân trang
// #define MM_FIXED_MEMSZ   // Bộ nhớ cố định (comment = động)

#define IODUMP       1      // In log I/O ra stdout
#define PAGETBL_DUMP 1      // In nội dung page table

#define MM64         1      // Dùng paging 64-bit (page table 4 cấp)
// #undef MM64              // Dùng paging 32-bit
```

---

## 📄 Định dạng file input

Mỗi file trong `input/` là cấu hình một kịch bản chạy. Dòng đầu tiên:

```
<time_slice> <num_cpus> <num_processes>
```

Các dòng tiếp theo mô tả tiến trình:

```
<start_time> <path_to_process_code> [priority]
```

Ví dụ (`input/os_1_mlq_paging`):

```
4 1 3
0 proc/p0 5
2 proc/p1 10
4 proc/p2 1
```

---

## 👥 Thành viên nhóm

> *(Cập nhật tên thành viên tại đây)*

---

## 📜 License

MIT License — xem file [LICENSE](LICENSE) để biết thêm chi tiết.

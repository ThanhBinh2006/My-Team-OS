import re
import sys
import os
import matplotlib.pyplot as plt

def parse_input(input_filename):
    """
    Đọc file input để lấy cấu hình hệ thống.
    Định dạng dòng đầu tiên: [time_slice] [num_cpus] [num_processes]
    """
    num_cpus = 1
    try:
        with open(input_filename, 'r') as f:
            first_line = f.readline().strip()
            if first_line:
                parts = first_line.split()
                if len(parts) >= 2:
                    num_cpus = int(parts[1])
    except Exception as e:
        print(f"Lỗi đọc file input: {e}")
    return num_cpus

def parse_log(filename, num_cpus):
    """
    Đọc file log và xây dựng lịch chạy cho mỗi CPU.
    Trả về một dict: {cpu_id: {time_slot: process_id hoặc None}}.
    """
    # Khởi tạo động số lượng CPU dựa trên file input
    cpu_state = {i: None for i in range(num_cpus)}
    cpu_schedule = {i: {} for i in range(num_cpus)}
    current_time = None

    # Các pattern regex
    time_slot_pattern = re.compile(r"Time slot\s+(\d+)")
    dispatched_pattern = re.compile(r"CPU\s+(\d+):\s+Dispatched process\s+(\d+)")
    processed_pattern = re.compile(r"CPU\s+(\d+):\s+Processed\s+(\d+)\s+has finished")
    stopped_pattern = re.compile(r"CPU\s+(\d+)\s+stopped")

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Gặp dòng "Time slot"
            ts_match = time_slot_pattern.search(line)
            if ts_match:
                current_time = int(ts_match.group(1))
                # Lưu trạng thái hiện tại của mỗi CPU tại time slot này
                for cpu in cpu_state:
                    cpu_schedule[cpu][current_time] = cpu_state[cpu]
                continue
            
            # Gặp dòng "Dispatched process"
            d_match = dispatched_pattern.search(line)
            if d_match:
                cpu = int(d_match.group(1))
                proc = int(d_match.group(2))
                if cpu in cpu_state:
                    cpu_state[cpu] = proc
                    if current_time is not None:
                        cpu_schedule[cpu][current_time] = proc
                continue
            
            # Gặp dòng "Processed ... has finished"
            p_match = processed_pattern.search(line)
            if p_match:
                cpu = int(p_match.group(1))
                if cpu in cpu_state:
                    cpu_state[cpu] = None
                    if current_time is not None:
                        cpu_schedule[cpu][current_time] = None
                continue
            
            # Gặp dòng "stopped"
            s_match = stopped_pattern.search(line)
            if s_match:
                cpu = int(s_match.group(1))
                if cpu in cpu_state:
                    cpu_state[cpu] = None
                    if current_time is not None:
                        cpu_schedule[cpu][current_time] = None
                continue
                
    return cpu_schedule

def merge_schedule(schedule):
    """
    Nhận vào lịch chạy của 1 CPU (dict: time_slot -> process id hoặc None)
    và gom các time slot liên tiếp có cùng trạng thái thành các khoảng liên tục.
    """
    intervals = []
    sorted_times = sorted(schedule.keys())
    if not sorted_times:
        return intervals
    
    current_proc = schedule[sorted_times[0]]
    start_time = sorted_times[0]
    
    for t in sorted_times[1:]:
        if schedule[t] == current_proc:
            continue
        else:
            duration = t - start_time
            if duration > 0:
                intervals.append((start_time, duration, current_proc))
            start_time = t
            current_proc = schedule[t]
            
    # Thêm khoảng cuối cùng
    if sorted_times:
        t = sorted_times[-1] + 1
        duration = t - start_time
        if duration > 0:
            intervals.append((start_time, duration, current_proc))
            
    return intervals

def main():
    if len(sys.argv) < 1:
        print("Cú pháp: python ganttchart.py <ten_file_input>")
        sys.exit(1)
        
    input_file = "input/"+sys.argv[1]
    log_file ="my_output/"+sys.argv[1]
    out_img = "my_output/"+sys.argv[1]+".png"
    
    # 1. Trích xuất số lượng CPU từ file input
    num_cpus = parse_input(input_file)
    print(f"[INFO] Hệ thống được cấu hình với {num_cpus} CPU.")
    
    # 2. Phân tích file log (output/expected)
    cpu_schedule = parse_log(log_file, num_cpus)
    
    # 3. Xác định time slot lớn nhất an toàn (tránh lỗi khi CPU không chạy gì)
    max_time = 0
    for schedule in cpu_schedule.values():
        if schedule:
            max_time = max(max_time, max(schedule.keys()))
    max_time += 1
    
    # Gom lịch cho mỗi CPU thành các khoảng liên tục
    cpu_intervals = {}
    for cpu, schedule in cpu_schedule.items():
        cpu_intervals[cpu] = merge_schedule(schedule)
    
    # Định nghĩa màu sắc cho các process (mở rộng thêm màu cho nhiều process)
    process_colors = {
        1: '#1f77b4', 2: '#ff7f0e', 3: '#2ca02c', 4: '#d62728', 
        5: '#9467bd', 6: '#8c564b', 7: '#e377c2', 8: '#7f7f7f', 
        9: '#bcbd22', 10: '#17becf', None: 'lightgray'
    }
    
    # Vẽ biểu đồ
    fig, ax = plt.subplots(figsize=(12, max(4, num_cpus * 2)))
    row_height = 8
    row_gap = 10
    yticks = []
    y_labels = []
    
    for cpu, intervals in cpu_intervals.items():
        y = cpu * row_gap
        yticks.append(y + row_height / 2)
        y_labels.append(f"CPU {cpu}")
        for (start, duration, proc) in intervals:
            color = process_colors.get(proc, 'tab:gray') if proc is not None else 'lightgray'
            ax.broken_barh([(start, duration)], (y, row_height),
                           facecolors=color, edgecolor='black', linewidth=1)
            
            if proc is not None:
                ax.text(start + duration/2, y + row_height/2, f"P{proc}",
                        ha='center', va='center', color='white', fontsize=10, fontweight='bold')
    
    # Thiết lập trục Ox
    ax.set_xticks([x + 0.5 for x in range(max_time)], minor=False)
    ax.set_xticklabels(range(max_time))
    ax.set_xticks(range(max_time + 1), minor=True)
    ax.grid(which='minor', axis='x', linestyle='--', color='gray', alpha=0.5)
    
    ax.set_xlabel("Time Slot")
    ax.set_yticks(yticks)
    ax.set_yticklabels(y_labels)
    ax.set_title("OS Scheduler Gantt Chart")
    ax.set_xlim(0, max_time)
    
    # Lưu biểu đồ
    os.makedirs(os.path.dirname(out_img) if os.path.dirname(out_img) else '.', exist_ok=True)
    plt.tight_layout()
    plt.savefig(out_img, dpi=150)
    print(f"[SUCCESS] Đã xuất biểu đồ ra file: {out_img}")

if __name__ == '__main__':
    main()
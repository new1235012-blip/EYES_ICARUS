import re
import matplotlib.pyplot as plt
import matplotlib.patches as patches

class SegmentedHeatbed:
    def __init__(self, width, height, rows, cols, safe_margin=10):
        """
        safe_margin là vùng thêm cho kích thước ban đầu (mm)
        """
        self.bed_w = width
        self.bed_h = height
        self.rows = rows
        self.cols = cols
        self.safe_margin = safe_margin
        
        # Kích thước một miếng pad
        self.pad_w = width / cols
        self.pad_h = height / rows

    def _get_pad_indices(self, min_x, max_x, min_y, max_y):
        """Chuyển đổi vùng tọa độ (mm) sang danh sách các Pad ID (row, col)"""
        # 1. Áp dụng Safe Margin
        min_x = max(0, min_x - self.safe_margin)
        max_x = min(self.bed_w, max_x + self.safe_margin)
        min_y = max(0, min_y - self.safe_margin)
        max_y = min(self.bed_h, max_y + self.safe_margin)

        # 2. Tính chỉ số hàng/cột bắt đầu và kết thúc
        # Trục X -> Cột (Col), Trục Y -> Hàng (Row)
        col_start = int(min_x // self.pad_w)
        col_end = int(max_x // self.pad_w)
        
        row_start = int(min_y // self.pad_h)
        row_end = int(max_y // self.pad_h)

        # 3. Tạo danh sách các pad
        active_pads = set()
        for r in range(row_start, row_end + 1):
            for c in range(col_start, col_end + 1):
                if 0 <= r < self.rows and 0 <= c < self.cols:
                    active_pads.add((r, c))
        
        return active_pads

    def process_gcode(self, filepath):
        """
        Hàm chính: Đọc G-code và trả về lịch trình bật nhiệt.
        Return: List các Dictionary chứa thông tin từng layer.
        """
        print(f"[INFO] Đang xử lý file: {filepath}...")
        
        layers = []
        # Biến tạm cho layer đang quét
        current_layer = {
            'id': 0, 'z': 0.0,
            'min_x': float('inf'), 'max_x': float('-inf'),
            'min_y': float('inf'), 'max_y': float('-inf'),
            'has_extrusion': False
        }
        
        current_e = 0.0
        
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line: continue

                # --- 1. PHÁT HIỆN CHUYỂN LAYER ---
                # Simplify3D/Cura thường dùng comment "; layer X"
                if "; layer" in line and ("Z =" in line or ", Z =" in line):
                    # Lưu layer cũ trước khi sang layer mới
                    if current_layer['has_extrusion']:
                        layers.append(current_layer)
                    
                    # Parse Z height và Layer ID
                    try:
                        z_match = re.search(r'Z = ([\d\.]+)', line)
                        z_val = float(z_match.group(1)) if z_match else 0.0
                        l_id_match = re.search(r'layer (\d+)', line)
                        l_id = int(l_id_match.group(1)) if l_id_match else len(layers)+1
                    except:
                        z_val, l_id = 0.0, 0

                    # Reset biến cho layer mới
                    current_layer = {
                        'id': l_id, 'z': z_val,
                        'min_x': float('inf'), 'max_x': float('-inf'),
                        'min_y': float('inf'), 'max_y': float('-inf'),
                        'has_extrusion': False
                    }
                    continue

                # --- 2. XỬ LÝ RESET E (G92) ---
                if line.startswith('G92'):
                    e_match = re.search(r'E([\d\.]+)', line)
                    if e_match: current_e = float(e_match.group(1))
                    continue

                # --- 3. XỬ LÝ LỆNH IN (G1) ---
                if line.startswith('G1'):
                    # Lấy tọa độ
                    x_match = re.search(r'X([\d\.]+)', line)
                    y_match = re.search(r'Y([\d\.]+)', line)
                    e_match = re.search(r'E([\d\.]+)', line)

                    x = float(x_match.group(1)) if x_match else None
                    y = float(y_match.group(1)) if y_match else None
                    new_e = float(e_match.group(1)) if e_match else None

                    # Kiểm tra có phun nhựa không (Extruding)
                    is_extruding = False
                    if new_e is not None:
                        if new_e > current_e:
                            is_extruding = True
                            current_e = new_e # Update E
                        else:
                            current_e = new_e # Retraction / Reset

                    # Cập nhật Bounding Box
                    if is_extruding and x is not None and y is not None:
                        current_layer['has_extrusion'] = True
                        if x < current_layer['min_x']: current_layer['min_x'] = x
                        if x > current_layer['max_x']: current_layer['max_x'] = x
                        if y < current_layer['min_y']: current_layer['min_y'] = y
                        if y > current_layer['max_y']: current_layer['max_y'] = y

        # Lưu layer cuối cùng
        if current_layer['has_extrusion']:
            layers.append(current_layer)

        # --- 4. TÍNH TOÁN MA TRẬN NHIỆT (CÓ CỘNG DỒN) ---
        schedule = []
        accumulated_pads = set() # Tập hợp các pad đã bật (để giữ nhiệt đế)

        print(f"[INFO] Đã quét xong {len(layers)} layers. Đang tính toán ma trận nhiệt...")

        for layer in layers:
            # Lấy các pad cần thiết cho riêng layer này
            layer_pads = self._get_pad_indices(
                layer['min_x'], layer['max_x'], 
                layer['min_y'], layer['max_y']
            )
            
            # Logic: Bật thêm pad mới, nhưng KHÔNG tắt pad cũ (tránh bong đế)
            accumulated_pads.update(layer_pads)
            
            schedule.append({
                'layer_id': layer['id'],
                'z': layer['z'],
                'active_pads': list(accumulated_pads), # Convert set -> list
                'bbox': (layer['min_x'], layer['max_x'], layer['min_y'], layer['max_y'])
            })
            
        return schedule

    def visualize(self, schedule_data, pause_time=0.1):
        """Hàm mô phỏng trực quan bằng Matplotlib"""
        print("[INFO] Đang chạy mô phỏng đồ họa...")
        fig, ax = plt.subplots(figsize=(6, 6))
        
        for step in schedule_data:
            ax.clear()
            ax.set_title(f"Layer {step['layer_id']} (Z={step['z']}mm)")
            ax.set_xlim(0, self.bed_w)
            ax.set_ylim(0, self.bed_h)
            ax.set_aspect('equal')
            
            # Vẽ lưới bàn in
            # Vẽ các ô pad
            for r in range(self.rows):
                for c in range(self.cols):
                    x_pos = c * self.pad_w
                    y_pos = r * self.pad_h
                    
                    # Kiểm tra xem pad này có được bật không
                    color = 'white'
                    edge = 'gray'
                    if (r, c) in step['active_pads']:
                        color = 'red'  # NÓNG
                        edge = 'red'
                    
                    rect = patches.Rectangle((x_pos, y_pos), self.pad_w, self.pad_h, 
                                             linewidth=1, edgecolor=edge, facecolor=color, alpha=0.5)
                    ax.add_patch(rect)
            
            # Vẽ khung bao vật thể (Bounding Box)
            min_x, max_x, min_y, max_y = step['bbox']
            bbox_w = max_x - min_x
            bbox_h = max_y - min_y
            rect_bbox = patches.Rectangle((min_x, min_y), bbox_w, bbox_h, 
                                          linewidth=2, edgecolor='blue', facecolor='none', linestyle='--')
            ax.add_patch(rect_bbox)
            
            plt.pause(pause_time)
        
        plt.show()

# --- PHẦN CODE CHẠY THỰC TẾ ---
if __name__ == "__main__":
    # 1. Cấu hình bàn in của bạn (Ví dụ: 300x300mm, chia 3x3 miếng)
    my_bed = SegmentedHeatbed(width=300, height=300, rows=3, cols=3, safe_margin=15)
    
    # 2. File G-code đầu vào
    gcode_file = "Magnificent Tumelo-Jaban (8).gcode"
    
    # 3. Chạy xử lý
    try:
        result_schedule = my_bed.process_gcode(gcode_file)
        
        # 4. Xuất kết quả ra màn hình (hoặc lưu file JSON/Text tại đây)
        print(f"\n{'='*40}")
        print(f"KẾT QUẢ ĐIỀU KHIỂN NHIỆT ({len(result_schedule)} layers)")
        print(f"{'='*40}")
        
        # In mẫu 5 layer đầu
        for i in range(min(5, len(result_schedule))):
            d = result_schedule[i]
            print(f"Layer {d['layer_id']} (Z={d['z']}): Bật Pads {d['active_pads']}")
            
        print("...")
        # In layer cuối cùng
        last = result_schedule[-1]
        print(f"Layer {last['layer_id']} (Z={last['z']}): Bật Pads {last['active_pads']}")
        
        # 5. Chạy mô phỏng (Nếu cài matplotlib)
        # my_bed.visualize(result_schedule) 
        
    except FileNotFoundError:
        print("Lỗi: Không tìm thấy file G-code!")
# QCamber PNG Export Feature

## Tổng quan

Tính năng export PNG đã được thêm vào QCamber để cho phép xuất panel PCB và các layer riêng lẻ ra file PNG với độ phân giải cao (lên đến 20,000 x 20,000 pixel).

## Tính năng chính

### 1. **Panel Export với Step & Repeat**
- Xuất toàn bộ panel bao gồm tất cả các step repeat instances
- Hỗ trợ các transformation: rotation, mirroring, translation
- Tự động tính toán bounding rectangle của panel

### 2. **Layer Selection**
- Có thể chọn export layer cụ thể (L1, L2, L3, L4, etc.)
- Hoặc export tất cả layers cùng lúc
- Filtering thông minh để chỉ hiển thị layer được chọn

### 3. **High Resolution Export**
- Hỗ trợ độ phân giải lên đến 50,000 x 50,000 pixels
- Presets cho các độ phân giải phổ biến:
  - Full HD (1920x1080)
  - 4K Ultra HD (3840x2160)
  - High Resolution (10,000x10,000)
  - Ultra High (20,000x20,000)
  - Custom resolution

### 4. **Export Configuration**
- **Background Color**: Tùy chọn màu nền (mặc định: đen)
- **DPI Settings**: Cấu hình DPI từ 72 đến 2400
- **Crop to Content**: Tự động crop theo nội dung thực tế
- **Quality Settings**: Anti-aliasing và high quality rendering

## Cách sử dụng

### 1. **Từ ViewerWindow**

1. Mở PCB design trong QCamber
2. Vào menu **File** → **Export to PNG...** hoặc nhấn **Ctrl+E**
3. Cấu hình export settings:
   - Chọn resolution preset hoặc custom size
   - Chọn target layer (L2 cho inner layer)
   - Bật "Include Step & Repeat" để export toàn panel
   - Chọn background color
4. Chọn output path
5. Click **Export**

### 2. **Programmatic Usage**

```cpp
#include "pngexporter.h"
#include "exportdialog.h"

// Create exporter
PngExporter exporter;

// Configure settings
PngExporter::ExportSettings settings;
settings.width = 20000;
settings.height = 20000;
settings.outputPath = "panel_L2.png";
settings.layerName = "L2";
settings.includeStepRepeat = true;
settings.backgroundColor = Qt::black;
settings.cropToContent = true;

// Export
bool success = exporter.exportPanelToPng(scene, settings);
```

## Đặc biệt cho Layer L2

Để export layer L2 với tất cả components trong panel:

1. **Layer Selection**: Chọn "L2 (Inner)" trong dropdown
2. **Step Repeat**: Bật "Include Step & Repeat (Panel)" 
3. **Resolution**: Sử dụng "Ultra High (20000x20000)" cho quality tốt nhất
4. **Background**: Chọn màu đen để contrast tốt với copper traces
5. **Crop to Content**: Bật để loại bỏ không gian trống

## Kỹ thuật Implementation

### 1. **Architecture**

```
ExportDialog (UI) → PngExporter (Logic) → QGraphicsScene::render() → PNG File
```

### 2. **Core Classes**

- **`PngExporter`**: Main export engine
- **`ExportDialog`**: Configuration UI  
- **`LayerFeatures`**: Handle step repeat và panel assembly
- **`ODBPPGraphicsScene`**: Scene chứa graphics data

### 3. **Memory Management**

- Automatic memory limit checking (400 megapixels max by default)
- Progressive rendering cho large images
- Efficient QPixmap allocation và cleanup

### 4. **Step Repeat Processing**

QCamber xử lý step repeat bằng cách:

1. Parse step repeat blocks từ ODB++ data
2. Tạo multiple instances của LayerFeatures
3. Apply transformations (translation, rotation, mirroring)
4. Add tất cả instances vào scene
5. Render toàn bộ scene ra PNG

## File Formats và Quality

### **PNG Output**
- **Format**: PNG với full color support
- **Compression**: Optimized PNG compression
- **Quality**: Lossless với anti-aliasing
- **File Size**: ~3 bytes per pixel với compression

### **Estimated File Sizes**
- 10k x 10k: ~200 MB
- 20k x 20k: ~800 MB  
- 30k x 30k: ~1.8 GB

## Testing

Chạy test functionality:

```bash
# Build test
qmake test.pro
make

# Run test
./test_png_export
```

## Troubleshooting

### **Common Issues**

1. **"Insufficient memory"**
   - Giảm resolution xuống
   - Check available RAM (cần ít nhất 4GB cho 20k x 20k)

2. **"Layer not found"**
   - Verify layer name exists trong design
   - Check layer visibility

3. **"Empty export rectangle"**
   - Design có thể không có content
   - Check step repeat configuration

### **Performance Tips**

1. **Memory**: 16GB RAM recommended cho ultra high resolution
2. **Storage**: SSD recommended cho fast I/O
3. **CPU**: Multi-core CPU giúp rendering nhanh hơn

## Tương lai Enhancements

1. **Batch Export**: Export multiple layers/steps cùng lúc
2. **PDF Export**: Vector format support
3. **Tiled Export**: Split large images thành tiles
4. **Custom Filters**: Advanced layer filtering options
5. **Progress Resume**: Resume interrupted exports

## Requirements

- **Qt 5.x** với Widgets và SVG modules
- **Minimum RAM**: 4GB (8GB recommended)
- **Storage**: 2GB free space cho temporary files
- **OS**: Windows, Linux, macOS
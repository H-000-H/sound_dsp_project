from PIL import Image
import os

def img_to_c(path, out_name, var_name):
    img = Image.open(path)
    print(f'Input: {path} -> {img.size} {img.mode}')
    # Resize to 240x240 with high quality
    img = img.resize((240, 240), Image.LANCZOS)
    # Handle alpha: composite onto black background
    if img.mode in ('RGBA', 'P'):
        if img.mode == 'P':
            img = img.convert('RGBA')
        bg = Image.new('RGB', (240, 240), (0, 0, 0))
        bg.paste(img, mask=img.split()[3])
        img = bg
    else:
        img = img.convert('RGB')

    w, h = img.size
    pixels = list(img.getdata())
    lines = []
    lines.append('#include "lvgl.h"')
    lines.append('')
    lines.append(f'static const LV_ATTRIBUTE_LARGE_CONST uint8_t {var_name}_map[] = {{')
    row_bytes = []
    for (r, g, b) in pixels:
        rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        row_bytes.append(f'0x{(rgb>>8)&0xFF:02X}, 0x{rgb&0xFF:02X}')
    for i in range(0, len(row_bytes), 12):
        chunk = row_bytes[i:i+12]
        lines.append('    ' + ', '.join(chunk) + ',')
    lines.append('};')
    lines.append('')
    lines.append(f'const lv_image_dsc_t {var_name} = {{')
    lines.append('  .header = {')
    lines.append('    .magic = LV_IMAGE_HEADER_MAGIC,')
    lines.append('    .cf = LV_COLOR_FORMAT_RGB565,')
    lines.append('    .flags = 0,')
    lines.append(f'    .w = {w},')
    lines.append(f'    .h = {h},')
    lines.append(f'    .stride = {w} * 2,')
    lines.append('    .reserved_2 = 0,')
    lines.append('  },')
    lines.append(f'  .data_size = sizeof({var_name}_map),')
    lines.append(f'  .data = {var_name}_map,')
    lines.append('  .reserved = NULL,')
    lines.append('};')

    out_path = os.path.join('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture', out_name + '.c')
    with open(out_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f'Written {out_path} ({w}x{h})')

img_to_c('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/94b41d6edc8e938239b051ad4496e9b.bmp', 'icon_settings', 'icon_settings')
img_to_c('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/屏幕截图 2026-05-12 191716.bmp', 'icon_music', 'icon_music')

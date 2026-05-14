from PIL import Image
import os

def img_to_c(path, var_name):
    img = Image.open(path)
    img = img.resize((60, 60), Image.LANCZOS)
    if img.mode in ('RGBA', 'P'):
        if img.mode == 'P':
            img = img.convert('RGBA')
        bg = Image.new('RGB', (60, 60), (0, 0, 0))
        bg.paste(img, mask=img.split()[3])
        img = bg
    else:
        img = img.convert('RGB')
    pixels = list(img.getdata())
    lines = ['#include "lvgl.h"', '', f'static const LV_ATTRIBUTE_LARGE_CONST uint8_t {var_name}_map[] = {{']
    row_bytes = []
    for (r, g, b) in pixels:
        rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        row_bytes.append(f'0x{rgb&0xFF:02X}, 0x{(rgb>>8)&0xFF:02X}')
    for i in range(0, len(row_bytes), 12):
        lines.append('    ' + ', '.join(row_bytes[i:i+12]) + ',')
    lines.append('};')
    lines.append('')
    lines.append(f'const lv_image_dsc_t {var_name} = {{')
    lines.append('  .header = {')
    lines.append('    .magic = LV_IMAGE_HEADER_MAGIC,')
    lines.append('    .cf = LV_COLOR_FORMAT_RGB565,')
    lines.append('    .flags = 0,')
    lines.append('    .w = 60,')
    lines.append('    .h = 60,')
    lines.append('    .stride = 60 * 2,')
    lines.append('    .reserved_2 = 0,')
    lines.append('  },')
    lines.append(f'  .data_size = sizeof({var_name}_map),')
    lines.append(f'  .data = {var_name}_map,')
    lines.append('  .reserved = NULL,')
    lines.append('};')
    out = os.path.join('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture', var_name + '.c')
    with open(out, 'w') as f:
        f.write('\n'.join(lines))
    print(f'Written {out}')

img_to_c('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/94b41d6edc8e938239b051ad4496e9b.png', 'icon_settings')
img_to_c('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/music_icon_60x60.png', 'icon_music')

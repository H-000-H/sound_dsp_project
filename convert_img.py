from PIL import Image
import os

img = Image.open('d:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/1_HjDNdbtW6fkmOxief3Yu3A.webp')
w, h = img.size
print(f'Original: {w}x{h}')

# Resize to fit 240x240 screen, keep aspect ratio, leave margin
# Target max 200 width -> height 150
img.thumbnail((200, 200), Image.LANCZOS)
w, h = img.size
print(f'Thumbnail: {w}x{h}')

img_rgb = img.convert('RGB')

c_name = 'music_cover'
arr_name = f'{c_name}_map'

out = []
out.append('#include "lvgl.h"')
out.append('')
out.append(f'static const LV_ATTRIBUTE_LARGE_CONST uint8_t {arr_name}[] = {{')

vals = []
for y in range(h):
    for x in range(w):
        r, g, b = img_rgb.getpixel((x, y))
        rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        vals.append(f'0x{(rgb>>8)&0xFF:02X}, 0x{rgb&0xFF:02X}')

for i in range(0, len(vals), 12):
    out.append('    ' + ', '.join(vals[i:i+12]) + ',')

out.append('};')
out.append('')
out.append(f'const lv_image_dsc_t {c_name} = {{')
out.append('    .header.magic = LV_IMAGE_HEADER_MAGIC,')
out.append(f'    .header.w = {w},')
out.append(f'    .header.h = {h},')
out.append(f'    .header.stride = {w * 2},')
out.append('    .header.cf = LV_COLOR_FORMAT_RGB565,')
out.append(f'    .data_size = {w * h * 2},')
out.append(f'    .data = {arr_name},')
out.append('};')

out_path = 'd:/ESP32_PROJECT/sound_dsp_project/components/app/lvgl/UI/picture/music_cover.c'
with open(out_path, 'w') as f:
    f.write('\n'.join(out))

print(f'Written: {out_path} {w}x{h}')

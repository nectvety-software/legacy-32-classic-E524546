#!/usr/bin/env python3
"""Generates the checked 240x320 VQEAF screen contract from source data.
No dependencies. Generated C++ is host-buildable without Arduino or external fonts.
"""
from pathlib import Path
import json, re
R=Path(__file__).resolve().parents[1]
def r(name,x,y,w,h,token,description,kind='box'):
    return dict(id=name,x=x,y=y,w=w,h=h,token=token,description=description,kind=kind)
def screen(id,title,parts,keys='',notes='',state='target; retained baseline screen behavior'):
    return dict(id=id,title=title,regions=parts,keys=keys,notes=notes,implementation=state)
L=[]
L.append(screen('home','Home / General',[r('clock_card',12,43,216,78,'panel','Card đồng hồ + ngày'),r('clock_text',18,50,204,43,'text','HH:MM hoặc --:--, căn giữa'),r('date_line',18,91,204,22,'text','Ngày chưa đồng bộ hiển thị Date not set'),r('network_card',12,132,216,46,'panel','Trạng thái WiFi và số thông báo'),r('network_line',18,137,205,15,'text','SSID/trạng thái cắt theo pixel'),r('alerts_line',18,153,205,20,'text','Thông báo chưa đọc hoặc trạng thái nhạc'),r('quick_wifi',8,191,72,67,'selected','WiFi shortcut'),r('quick_music',84,191,72,67,'panel','Music shortcut'),r('quick_files',160,191,72,67,'panel','Files shortcut'),r('key_hint',8,266,224,20,'status','Phím giữ MENU / OPTION')], 'MENU→Menu; LEFT/RIGHT chọn shortcut; START mở; OPTION→Quick','Đồng hồ NTP không được giả giờ; battery chỉ outline','integrated from existing Idle Home; geometry cross-checked with SymbianUI.cpp'))
items=['WiFi','Bluetooth','Music','File mgr','Gallery','Internet','Shell','Recovery','Settings','Themes','Apps','Library']
parts=[]
for i,item in enumerate(items):
 c,row=i%3,i//3; x,y=1+c*78,28+row*66
 parts.extend([r('cell_%02d'%i,x,y,76,66,'selected' if i==0 else 'list','Ô %d: %s'%(i+1,item)),r('icon_%02d'%i,x+20,y+3,36,36,'icon','Icon của %s'%item),r('caption_%02d'%i,x+2,y+44,72,16,'text','Nhãn %s'%item)])
parts.append(r('grid_scroll',236,34,2,252,'scrollbar','Thanh cuộn/menu rail'))
L.append(screen('menu','Menu 3 x 4',parts,'D-Pad di chuyển; START mở; OPTION popup; A/B về Home; Menu→Home','Giữ thứ tự 12 biểu tượng y hệt ảnh; Retro Explorer là tùy chọn trong Options.','integrated original firmware v2.0.1 grid; external vqeaf palette mapped'))
list_base=lambda cnt,start=29:[r('row_%02d'%i,2,start+42*i,233,41,'selected' if i==0 else 'list','Danh sách dòng '+str(i+1)) for i in range(cnt)]
empty=lambda title:[r('page_label',12,55,216,24,'text',title),r('empty_notice',12,101,216,20,'dim','Trạng thái trống'),r('empty_hint',12,126,216,20,'dim','Hướng dẫn thao tác tiếp theo')]
L.append(screen('wifi','WiFi',list_base(6)+empty('WIFI')+[r('signal_icons',8,32,28,248,'icon','Cột biểu tượng mạng nếu quét thấy')], 'OPTION quét; UP/DOWN chọn; START kết nối; A lùi','Trạng thái trống và list là 2 trạng thái loại trừ. Không hiển thị mật khẩu; scan không chặn UI.'))
L.append(screen('bluetooth','Bluetooth',list_base(6)+empty('Bluetooth'), 'OPTION quét; START chi tiết; A lùi','BLE scanner; không quảng cáo A2DP/Classic nếu chưa hỗ trợ.'))
L.append(screen('files','File manager',[r('path_bar',5,32,226,21,'panel','Đường dẫn hiện tại'),*list_base(5,56),r('scrollbar',236,56,2,211,'scrollbar','Thumb danh sách file'),r('empty_notice',12,103,216,25,'dim','microSD is not mounted')], 'D-Pad di chuyển; START Open; OPTION actions; A lên một thư mục','Bất kỳ thao tác xóa file nào cũng phải xác nhận; path bị sandbox.'))
L.append(screen('gallery','Gallery',[r('page_title',12,53,216,22,'text','Gallery'),r('thumb0',7,81,72,73,'panel','Thumbnail trái'),r('thumb1',84,81,72,73,'panel','Thumbnail giữa'),r('thumb2',161,81,72,73,'panel','Thumbnail phải'),r('thumb3',7,158,72,73,'panel','Hàng thumbnail 2'),r('thumb4',84,158,72,73,'panel','Hàng thumbnail 2'),r('thumb5',161,158,72,73,'panel','Hàng thumbnail 2'),r('missing_sd',12,103,216,24,'dim','microSD is not mounted')], 'UP/DOWN/LEFT/RIGHT thumbnails; START View; A Back', 'Thumbnail view là layout đích; baseline hiện có thể dùng danh sách.'))
L.append(screen('music','Music',[r('title_line',12,51,216,27,'text','Tên media / trạng thái'),*list_base(5,36),r('mini_player',6,251,228,40,'panel','Bộ điều khiển nhỏ không chồng footer'),r('mini_line1',11,255,218,14,'text','Audio ready hoặc trạng thái thực'),r('mini_line2',11,273,218,14,'dim','Track / shuffle / repeat / volume')], 'START Play/Pause; OPTION playlist; A Back', 'Không báo audio ready khi phần cứng DAC chưa sẵn sàng; list & empty states loại trừ.'))
L.append(screen('shell','Shell',[r('terminal',2,28,236,228,'terminal','Ring buffer 10-12 dòng đầu ra'),r('terminal_info',3,31,234,16,'text','Shell version/đường dẫn'),r('prompt',3,256,234,38,'terminal','Lệnh nhập + tối đa 1 dòng gợi ý'),r('prompt_text',7,264,226,16,'accent','vqeaf:/$')], 'START Command; UP history; OPTION actions; A Back','Lệnh được allowlist/sandbox; không trao arbitrary shell cho .qeapp.','integrated existing shell; coordinates specified for next reflow'))
labels=['Theme','Backlight','Audio volume','Clock format','Auto WiFi strongest','Auto keypad lock']
L.append(screen('settings','Settings',[r('setting_%d'%i,2,29+i*42,233,41,'selected' if i==0 else 'list',label) for i,label in enumerate(labels)]+[r('scrollbar',236,30,2,264,'scrollbar','Theo số mục')], 'UP/DOWN chọn; LEFT/RIGHT thay đổi nếu hợp lệ; START nhập; A Back', 'Lưu NVS; trình render tuyệt đối không chạy WiFi scan.'))
L.append(screen('themes','Themes',[r('theme_%d'%i,2,29+i*42,233,41,'selected' if i==0 else 'list','Theme row '+str(i+1)) for i in range(6)]+[r('theme_icon',11,36,25,25,'icon','Preview mini palette'),r('scrollbar',236,30,2,264,'scrollbar','Có khi số theme >6')], 'START Apply; OPTION Import/Info; A Back', 'Theme Studio DSL @vqeaf 1.x: palette và launcher overrides; tài nguyên phoneShell nằm ngoài LCD.'))
L.append(screen('applications','Applications',[r('app_%d'%i,2,29+i*42,233,41,'selected' if i==0 else 'list','Installed/system app row') for i in range(6)]+[r('app_icon',10,36,27,27,'icon','Icon của app'),r('scrollbar',236,30,2,264,'scrollbar','Danh sách app')], 'START Open; OPTION Info/Uninstall; A Back', 'Chỉ nhận installed QEAPP/2 signed + built-in; không chạy Lua native mới.'))
L.append(screen('installer','App installer',[r('inbox_label',12,42,216,28,'text','App inbox'),*list_base(5,76),r('details_modal',11,77,218,166,'modal','Manifest/publisher/signature trước khi cài'),r('missing_sd',12,106,216,23,'dim','microSD is not available')], 'UP/DOWN chọn; START xem/xác minh; OPTION Install; A Back', 'Không đổi QEAPP/2 định dạng ECDSA-P256; dialog loại trừ empty state.'))
L.append(screen('recovery','Recovery',[r('recovery_%d'%i,2,29+i*42,233,41,'selected' if i==0 else 'list',title) for i,title in enumerate(['Boot status','Start normal mode','Enable Safe Mode','Clear recovery flags','Restart device'])]+[r('confirmation_modal',14,88,212,132,'modal','Cần xác nhận Clear/Restart')], 'UP/DOWN chọn; START Select; A Back', 'Giữ DOWN lúc khởi động; không tải theme từ SD trước khi chọn Safe Mode.'))
calc_keys=['7','8','9','/','4','5','6','*','1','2','3','-','C','0','=','+']
calc_parts=[r('result',12,40,216,48,'panel','Biểu thức/kết quả'),r('result_text',17,52,205,30,'text','Số được căn phải, ellipsis nếu quá dài')]
for i,ch in enumerate(calc_keys):
 c,row=i%4,i//4;calc_parts.append(r('key_'+str(i),8+c*56,100+row*45,51,41,'selected' if i==0 else 'panel','Phím '+ch))
calc_parts.append(r('instruction',8,284,224,11,'dim','Options / Backspace / Clear'))
L.append(screen('calculator','Calculator',calc_parts,'D-pad move 4×4; START Enter; B xóa; OPTION reset; A Back','Khu vực 4×4 không vượt vào footer; phép chia 0 được chặn.'))
L.append(screen('stopwatch','Stopwatch',[r('face',10,52,220,74,'panel','00:00.00 hoặc elapsed'),r('digits',22,79,196,34,'text','Thời gian căn giữa'),r('start',8,135,72,36,'selected','Start/Pause'),r('lap',84,135,72,36,'panel','Lap'),r('reset',160,135,72,36,'panel','Reset'),r('lap_list',8,178,224,113,'list','Các vòng đã ghi (nếu có)')], 'LEFT/RIGHT chọn; START thao tác; A Back','Chỉ dirty vùng đồng hồ theo tick; thời gian chạy ở service không phụ thuộc màn hình.'))
L.append(screen('browser','Qeafbrowser',[r('address',4,29,232,26,'panel','Thanh địa chỉ nếu ở browser loaded mode'),r('page_view',4,59,232,234,'list','Viewport HTML/WML và scrollbar'),r('offline_header',12,57,216,22,'text','Qeafbrowser khi WiFi offline'),r('offline_reason',12,100,216,20,'dim','WiFi is not connected'),r('offline_hint',12,121,216,22,'dim','Options > Home can open cache')], 'OPTION browser menu; START open link; D-pad scroll/focus; SELECT hold T9; A Back', 'Offline và loaded là 2 trạng thái loại trừ; HTTPS phải kiểm CA khi có.'))
assert len(L)==16
common=[r('header',0,0,240,27,'chrome','Tiêu đề x4..79, NTP giữa, WiFi/pin outline phải'),r('header_rule',0,27,240,1,'border','Đường ngăn'),r('content',0,28,240,270,'background','Nội dung'),r('soft_left',0,298,80,22,'footer','Options/Menu'),r('soft_center',80,298,80,22,'footer','Open/Select'),r('soft_right',160,298,80,22,'footer','Back/Exit')]
obj=dict(format='VQEAF OS 240x320 pixel atlas',version='2.2.0',orientation='portrait',unit='physical_px',screen_width=240,screen_height=320,rect_convention='[x,y,w,h], right-bottom exclusive',shared=common,screens=L,remarks=['Danh sách các region giao nhau có chủ đích: UI nhiều trạng thái (offline vs loaded), hoặc container chứa child text/icon.', 'Status bar WiFi thực, không giả pin %, SIM, LTE khi thiếu cảm biến/modem.', 'Tọa độ spec là hợp đồng đích; phần app cũ cần soát lại khi port từ baseline.'])
out=R/'docs/pixel_atlas.json';out.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
md=['# VQEAF OS v2.2 — Pixel Atlas 240 × 320 (dọc)','', '**Đơn vị:** px vật lý, gốc `(0,0)` ở trái-trên, hình chữ nhật `[x, y, w, h]`, mép phải/dưới không gồm pixel đích.','', '**Phạm vi:** hình tham chiếu người dùng có 17 khung (Menu và Menu Navigated là một màn có 2 trạng thái), 16 màn độc nhất. Đây là **tọa độ thiết kế chuẩn**, không phải chứng nhận từng màn đã render chính xác trên mạch.','', '**Hệ theme:** `.vqeaf` Theme Studio 1.0 với `palette` và `launcher{}`; firmware chỉ áp màu lên LCD thật. **Ứng dụng:** giữ QEAPP/2 nhị phân, ECDSA-P256, không biến thành ZIP/Lua.','', '## A. Khung dùng chung','', '| Vùng | X | Y | W | H | Token | Chức năng |','|---|---:|---:|---:|---:|---|---|']
for p in common:md.append(f"| `{p['id']}` | {p['x']} | {p['y']} | {p['w']} | {p['h']} | `{p['token']}` | {p['description']} |")
md += ['','### Quy tắc hiển thị & input','', '- Header trái 80 px, giữa 80 px (giờ `--:--` trước NTP), phải 80 px (WiFi và pin **outline**), 1 px separator ở y=27.','- Content không đè lên footer; footer 3 cột 80 px, y=298..319. Không giải phóng/đọc NVS, SD, RF ở trong `draw()`.','- Chỉ vẽ lại vùng dirty khi focus, clock, WiFi thay đổi. Popup đóng phải dựng lại nền bên dưới, không gọi `fillScreen(BLACK)`.','- `MENU` từ Home mở grid; MENU trong grid về Home; OPTION trong grid có Retro Explorer tùy chọn; SELECT giữ >600 ms đổi Game/T9.','', '## B. Tọa độ từng màn']
for i,sc in enumerate(L,1):
 md.extend(['',f"### {i}. {sc['title']} (`{sc['id']}`)",'',f"**Điều hướng:** {sc['keys']}  ",f"**Hiện trạng:** {sc['implementation']}  ",f"**Lưu ý:** {sc['notes']}",'','| Element | X | Y | W | H | Token | Mô tả |','|---|---:|---:|---:|---:|---|---|'])
 for p in sc['regions']:md.append(f"| `{p['id']}` | {p['x']} | {p['y']} | {p['w']} | {p['h']} | `{p['token']}` | {p['description']} |")
md += ['', '## C. Trạng thái và điều kiện kiểm tra pixel', '', '1. Tối thiểu 16 màn/17 trạng thái contact sheet, cả focus và no-SD/no-WiFi. Ảnh baseline của người dùng là nguồn bố cục; các tọa độ như đồng hồ, shortcut và grid dựa vào mã `SymbianUI.cpp`.', '2. Mọi vùng trong atlas phải nằm trong màn 240×320; mọi body không tràn lên footer. Trùng nhau **có chủ đích** đối với vùng container và các trạng thái loại trừ.', '3. Grid menu gồm 12 cell, 3 cột × 4 hàng. Biên giới cuối: cột 3 kết thúc x=233, hàng 4 kết thúc y=292; rail x=236; footer từ y=298.', '4. RTL/fallback font tiếng Việt: baseline TFT_eSPI font bitmap có thể thiếu dấu; nâng cấp font cần bổ sung glyph/UTF-8 riêng, KHÔNG tuyên bố đã đầy đủ.', '5. Giao diện `.vqeaf` với background/base64 của lớp **vỏ điện thoại ảo** không hiển thị trên màn ST7789 vật lý; chỉ các màu LCD `palette`/`launcher` được sử dụng.', '', '## D. Từ atlas tới firmware', '', '`tools/gen_pixel_atlas.py` xuất `docs/pixel_atlas.json`, tài liệu này và `src/core/UiScreenAtlas.h/.cpp` để đồng bộ màn hình. `src/core/UiLayoutGeometry.h` chứa tọa độ chrome/grid/list chính đang được `SymbianUI.h` dùng. Chạy `python tools/test_ui_v22.py` để kiểm tra bounds và build/link host.', '']
(R/'docs/UI_PIXEL_ATLAS_V22.md').write_text('\n'.join(md),encoding='utf-8')
header='''// Generated by tools/gen_pixel_atlas.py with docs/pixel_atlas.json. Do not edit.
#pragma once
#include <stddef.h>
#include "UiLayoutGeometry.h"
namespace VqeafAtlas {
struct Region {const char *name; VqeafLayout::Rect rect; const char *token;};
struct Screen {const char *id; const char *name; const Region *regions; size_t count;};
extern const Screen screens[16];
extern const size_t screenCount;
const Screen *get(const char *id);
}
'''
(R/'src/core/UiScreenAtlas.h').write_text(header)
c=['// Generated from tools/gen_pixel_atlas.py. Metadata-only; never parse on every draw.','#include "UiScreenAtlas.h"','#include <string.h>', 'namespace VqeafAtlas {']
for sc in L:
 c.append(f'static const Region r_{sc["id"]}[] = {{')
 for p in sc['regions']:
  c.append('  {"%s", {%d,%d,%d,%d}, "%s"},'%(p['id'],p['x'],p['y'],p['w'],p['h'],p['token']))
 c.append('};')
c.append('const Screen screens[16] = {')
for sc in L:c.append('  {"%s", "%s", r_%s, sizeof(r_%s)/sizeof(Region)},'%(sc['id'],sc['title'],sc['id'],sc['id']))
c.extend(['};', 'const size_t screenCount=16;', 'const Screen *get(const char *id) { if(!id)return nullptr; for(size_t i=0;i<screenCount;++i)if(!strcmp(screens[i].id,id))return &screens[i]; return nullptr;}', '}'])
(R/'src/core/UiScreenAtlas.cpp').write_text('\n'.join(c)+'\n')
print('Generated', len(L),'screens and',sum(len(z['regions']) for z in L),'regions')

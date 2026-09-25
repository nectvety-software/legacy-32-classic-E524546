// rg_input.h — đọc keypad 10 phím E524546 (INPUT_PULLUP, nhấn = LOW).
// Trả về bitmask RG_KEY_* (định nghĩa trong rg_config.h — file config duy nhất).
//   * Debounce 25ms; auto-repeat UP/DOWN/LEFT/RIGHT khi giữ (450ms/110ms).
//   * SELECT nhấn thường = RG_KEY_MODE; giữ >600ms = đổi chế độ Game/T9 (chuẩn repo).
//   * Chế độ T9: phím 3x3 phát ký tự '1'..'9', SELECT phát '0' (xem rg_input_read_char).
// Không hard-code GPIO: lấy từ pins.h qua rg_config.h.
#pragma once
#include <stdint.h>
#include "rg_config.h"

void     rg_input_init();
uint32_t rg_input_read();        // sự kiện cạnh nhấn + auto-repeat (gọi đều trong loop)
uint32_t rg_input_held();        // bitmask các phím đang giữ (mức vật lý đã debounce)
uint32_t rg_input_raw();         // mức vật lý tức thời, KHÔNG debounce (chẩn đoán phím kẹt)
bool     rg_input_t9_mode();     // true = chế độ T9 (nhập số), false = Game
char     rg_input_read_char();   // ký tự T9 vừa phát ('0'..'9'), 0 = không có
void     rg_input_wait_release(uint32_t mask); // chờ nhả các phím trong mask (tối đa ~1s)

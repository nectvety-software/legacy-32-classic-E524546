# Chess 2P — Lua beta QEAPP

Game cờ caro 2 người chơi trên vùng vẽ 240×270 của QEAPP Studio.

- D-Pad di chuyển ô đang chọn.
- `START` chọn quân hoặc thực hiện nước đi.
- `OPTION` chơi lại.
- Luật mẫu: không nhập thành, không en passant, tốt cờ tự phong cấp hậu, kiểm tra vua, hậu cờ và hòa khi chỉ còn hai vua.

```powershell
py -3 tools/qstudio.py validate projects/lua-chess
py -3 tools/qstudio.py lua-preview projects/lua-chess --frames 16 --replay tests/input_replay.json -o build/chess_2p.png
```

Firmware đích là profile thử nghiệm `vqeaf_lua_beta`; stock VQEAF-OS chỉ chạy `web/text`. Cần bootstrap Lua, provision trust key và build `.qeapp` bằng private key tương ứng trước khi chạy trên thiết bị.

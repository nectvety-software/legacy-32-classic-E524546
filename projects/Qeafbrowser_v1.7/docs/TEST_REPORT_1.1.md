# Qeafbrowser 1.1 — Test Report

## Static compile check

Đã chạy `g++ -fsyntax-only` với shim Arduino/Linux cho:

- `src/http.cpp`
- `src/wml.cpp`
- `src/store.cpp`
- `src/main.cpp`

Kết quả: **PASS**.

## Core unit test

Các case đã PASS:

- `example.com/a` → `https://example.com/a`
- resolve `../img/x.png`
- resolve `/root?q=2`
- resolve `?q=2`
- encode query `xin chao & ok` → `xin+chao+%26+ok`
- parse HTML có `<title>`
- bỏ `<script>` và `<style>` khỏi nội dung
- giữ khoảng trắng `Hello world`
- parse `href = "..."`
- lấy `img alt`

## Chưa xác nhận trong môi trường này

- Build PlatformIO thật với toolchain `espressif32@6.10.0` vì container hiện tại không có `pio`.
- TLS handshake thực trên phần cứng ESP32-S3 và mạng Wi-Fi thật.
- Simulator Windows `--live` HTTPS vì bridge hiện dùng Winsock raw TCP, chưa dùng Schannel.

Firmware giữ API `WiFiClientSecure::setInsecure()` + `setHandshakeTimeout()` tương thích với Arduino-ESP32 2.0.17; nên vẫn cần chạy `pio run` trên máy dev trước khi flash release.

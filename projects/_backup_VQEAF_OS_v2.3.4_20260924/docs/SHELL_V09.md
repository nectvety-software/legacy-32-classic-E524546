# Symbian S3 OS v0.9 Shell: file, network and monitoring tools

v0.9 keeps the bounded firmware shell introduced in v0.8 and extends it without turning the ESP32-S3 firmware into a POSIX/Linux userspace. There is still no `fork`, arbitrary executable loading, package manager or unrestricted command execution.

## File management

The shell works against the mounted microSD filesystem and supports quoted paths with spaces.

```text
pwd
ls [path]
cd <path>
cat <file>
stat <path>
mkdir <dir>
rm <file>
rmdir <empty-dir>
touch <file>
cp <src> <dst>
mv <src> <dst>
write <file> <text>
append <file> <text>
hexdump <file> [offset]
```

`cp` uses a fixed 512-byte transfer buffer. `rm` never removes directories, `rmdir` removes only empty directories, and `/` is explicitly protected. `hexdump` emits at most 64 bytes per invocation.

## Network tools

```text
wifi status|scan|on|off
wifi <ssid> [passphrase]
ifconfig
ip
nslookup <host>
ping <host> [count]
wget <url> [output-file]
netmon
```

`ping` uses the ESP-IDF ICMP ping session API when the target framework exposes `ping/ping_sock.h`; otherwise the build falls back to a bounded TCP reachability probe and labels it as such. `wget` streams the HTTP body directly into a microSD `File` rather than materializing it in a large `String` or RAM buffer. HTTPS currently uses `WiFiClientSecure::setInsecure()` for compatibility, matching the existing lightweight browser approach; do not treat that mode as certificate-authenticated HTTPS.

## Monitoring

`top` is a firmware health snapshot rather than a Linux process sampler. It reports uptime, free/minimum heap, free/total PSRAM, WiFi RSSI, cumulative shell file-copy bytes and shell download bytes. `netmon` reports WiFi carrier/IP configuration, signal quality, cumulative download bytes and age of the most recent network operation. `ps` continues to show logical firmware services rather than OS processes.

## S60 keypad workflow

- START/SELECT: enter a command.
- UP/DOWN: command history.
- OPTION: Command, Network monitor, System monitor, File commands, Help, Clear, Recovery.
- A/B: return to Applications.

All command output remains bounded to the 20 x 39-character terminal ring to keep redraw cost and memory use predictable on the 240x320 handheld.

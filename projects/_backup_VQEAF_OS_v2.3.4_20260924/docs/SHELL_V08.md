# Symbian S3 OS v0.8 Shell

## Design basis

The reference project `platima/esp32-s31-linux` boots OpenSBI, Linux and a Buildroot userspace on an ESP32-S31 RISC-V target and exposes interactive getty shells on UART/panel terminals. It also supplies a small `wifi` command for console network control.

The E524546 handheld here uses ESP32-S3-WROOM-1 N16R8 (Xtensa LX7), ST7789 240x320 portrait, keypad, microSD, WiFi/BLE and the existing Arduino firmware runtime. Therefore this version does not embed or boot the S31 Linux image. Instead it adds a small firmware command interpreter with a feature-phone/S60 UI and bounded memory use.

## UI

- Open: `Menu -> Applications -> Shell`, or `Menu -> Options -> Shell`.
- `START/SELECT`: open command editor.
- `UP`: recall older command into the editor.
- `DOWN`: move toward newer history.
- `OPTION`: Command / Help / Clear / System info / Recovery.
- `A/B`: return to Applications.
- The normal S60 titlebar remains visible and only shows WiFi + battery status.

## Command set

```text
help clear uname version uptime free df mount
pwd cd ls cat stat date
wifi ifconfig ip ps dmesg safe reboot echo history
```

Examples:

```text
pwd
ls /
cd Music
ls
cat /config.txt
free
wifi status
wifi scan
wifi MySSID mypassword
ifconfig
ip
dmesg
safe status
history
```

`ls` displays at most 14 entries per command, `cat` displays at most 12 terminal lines, history stores 8 commands, and the terminal stores 20 lines of 39 characters. These caps avoid large transient allocations on the ESP32-S3.

## Safety and limitations

This is not a POSIX shell. There is no process creation, ELF execution, pipes, shell expansion, redirection, package manager, user permissions, or arbitrary command execution. Commands call explicit firmware services only. `safe on/off` changes the recovery flag; reboot is recommended after changing Safe Mode because disabled services are initialized only during boot.

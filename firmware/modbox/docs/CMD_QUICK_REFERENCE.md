# ModBox CMD - Quick Reference

## File Commands
```
ls [-la]           List files
cd <dir>           Change directory
cd ..              Go back
pwd                Print working directory
mkdir <name>       Create directory
touch <name>       Create file
rm <file>          Delete file
rm -rf <dir>       Delete directory
cp <src> <dst>     Copy file
mv <src> <dst>     Move/rename file
cat <file>         Show file content
stat <file>        File info
```

## WiFi Commands
```
scanap             Scan WiFi networks
scanap -live      Live scan (updates in real-time)
scanap -list      Show saved networks
connect "SSID" "pass" Connect to WiFi
connect            Reconnect last network
disconnect         Disconnect from WiFi
```

## Network Scan Commands
```
scanlocal          Scan local devices & services
scanarp           ARP scan for devices
scanports <ip>     Scan common ports
scanports <ip> all Scan all ports
scanports <ip> 1-1024 Scan port range
scanssh <ip>       Check SSH status
```

## Sweep Commands
```
sweep              Default sweep (10s each)
sweep -w <s>       WiFi sweep time
sweep -b <s>       BLE sweep time
sweep -h           Show sweep help
```

## Capture Commands
```
capture -probe      Record probe requests
capture -deauth    Record deauth frames
capture -beacon     Record beacon frames
capture -raw        Raw WiFi capture
capture -eapol     Record WPA handshakes
capture -pwn       Pwnagotchi mode
capture -wps       WPS traffic
capture -802154    IEEE 802.15.4
capture stop       Stop capture
capture list       List capture files
capture verify     Verify .pcap file
capture status     Show status
```

## System Commands
```
ifconfig           Network information
ps                 Running processes
top                System info
uptime             System uptime
free               Memory info
df                 Storage info
```

## Basic Commands
```
whoami             Current user
date               Current date/time
echo <text>        Print text
clear              Clear screen
help               Show help
exit               Exit CMD
```

## Navigation
```
UP/DOWN            Command history
SELECT             Open keyboard
A                  Execute command
LEFT/RIGHT         Scroll output
B/MENU             Exit
```

## Keyboard Buttons
```
DEL                Delete character
SPACE              Add space
OK                 Confirm & execute
CLEAR              Clear input
```

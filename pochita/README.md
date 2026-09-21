# PochitaOS - PlatformIO project

Day la thu muc goc ma PlatformIO IDE can mo:

```text
pochita/
|-- platformio.ini
|-- include/
|   `-- asset/             # bitmap/header dung chung
|-- src/
|   |-- main.cpp           # setup() va loop()
|   |-- component/         # cac module .h/.cpp
|   `-- *.h, *.cpp         # cac ung dung he thong
|-- sd_card/               # noi dung mau de chep vao the SD
`-- docs/
```

Trong VS Code, chon **File > Open Folder** va mo dung thu muc chua file
`platformio.ini` nay. Khong mo rieng `src/main.cpp`, thu muc `src`, hoac thu muc
cha `legacy-32-classic-E524546`.

Build nhanh:

```powershell
py -m platformio run
```

Xem [docs/PLATFORMIO.md](docs/PLATFORMIO.md) de biet cau hinh board va pin.

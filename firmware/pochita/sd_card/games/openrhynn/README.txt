OPENRHYNN: THE ELDERWOOD CHRONICLE (NATIVE ESP32-S3)

Copy the entire /games/openrhynn folder to the root of the SD card.
Open OpenRhynn.bin in File Manager, then choose Install to add it to Launcher.

OpenRhynn.bin is a real POCHITA binary game-data package, not a text manifest.
POCHITA validates its binary header, version, payload length and CRC32, then the
native OpenRhynn engine reads its maps, quests, items and starting game values.
Damaged or renamed arbitrary .bin files will not launch as OpenRhynn.

Controls:
- Direction keys: move
- A: attack / talk / interact / buy
- Select: inventory and character menu
- Option: use health potion
- Menu: quest log
- B: exit confirmation

Progress is stored offline in /games/openrhynn/save.txt as readable text.
No WiFi or Internet connection is required.

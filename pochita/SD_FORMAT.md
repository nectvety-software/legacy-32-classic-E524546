# POCHITA OS SD Format / Factory Reset

Open **Files**, return to `/`, then select `[SYS] Format / Reset SD`.

1. Press `A` or `SELECT` to open the format submenu.
2. Choose `Factory reset` or `Repartition FAT32`.
3. Press `RIGHT` to arm the destructive action.
4. Press `A` or `SELECT` again to erase the card contents.
5. Press `B` or `MENU` before step 4 to cancel safely.

The operation removes all accessible files and folders, shows live progress, and
recreates the default POCHITA OS directory tree. It is a logical factory reset,
matching the CyberOS implementation: it does not repartition the card or change
its FAT32 allocation settings.

`Repartition FAT32` performs a full repair operation. It unmounts the filesystem,
writes a new MBR with one 1 MiB-aligned partition, creates a FAT32 filesystem,
remounts it and verifies the FAT32 boot sector. Use this mode when Windows
reports that the card is damaged or needs to be formatted.

Formatting deletes OpenRhynn, ROMs, music, documents and save files. Copy the
contents of `dist/POCHITA_SD_PACKAGE.zip` back to the SD card to reinstall the
bundled applications and game data.

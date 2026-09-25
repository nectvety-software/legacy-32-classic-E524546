# Symbian S3 OS v1.3 – microSD system layout

The firmware creates the directory tree automatically after `SD_MMC.begin()` succeeds.
The goal is predictable paths, bounded caches, and compatibility with cards created by v1.2.

```text
/System/
  Cache/
    Web/
    Thumbs/
  Themes/
  Apps/
    Installed/
    Inbox/
  Downloads/
  Logs/
  Temp/
/Media/
  Music/
  Pictures/
/Documents/
/Themes/                 # legacy compatibility
```

## Responsibilities

- `/System/Cache/Web`: Qeafbrowser HTML response cache. Firmware keeps at most 16 files / 512 KiB and prunes large files first when over quota.
- `/System/Cache/Thumbs`: reserved for Gallery/browser thumbnails.
- `/System/Themes`: preferred `.vqeaf` theme directory. Themes app still scans `/Themes` for old cards.
- `/System/Apps/Inbox`: app/package downloads staged by Qeafbrowser. v1.3 does not execute arbitrary downloaded binaries.
- `/System/Apps/Installed`: reserved for validated future application packages.
- `/System/Downloads`: ordinary browser downloads.
- `/System/Logs`: service/crash logs.
- `/System/Temp`: scratch files; safe to clear while no app is using them.
- `/Media/Music`, `/Media/Pictures`, `/Documents`: preferred user content roots.

The layout is created parent-first and uses fixed paths to avoid repeated recursive scans and reduce temporary String/file-handle churn.

Shell maintenance:

```text
layout
cache status
cache prune
cache clear
df
```

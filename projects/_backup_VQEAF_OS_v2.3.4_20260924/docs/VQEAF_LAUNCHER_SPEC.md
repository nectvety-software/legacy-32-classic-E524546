# VQEAF Launcher Spec 2.1.0 (G3)

The new firmware retains the uploaded ESP32-S3 PlatformIO/Arduino/TFT_eSPI stack, services and routing and replaces `LauncherApp`'s original 3×4 grid with original event-driven, Retro-Go-*inspired* portrait code. **No Retro-Go source or copyrighted theme assets copied.** LCD rotation stays 0 and the effective framebuffer is 240×320 RGB565. The existing low-level TFT_eSPI driver remains in place (not a claim of having implemented Retro-Go DMA/checksums).

## Fixed launcher layout — 240×320 px

| Region | Y range | Contents |
|---|---:|---|
| Status | 0–23 | VQEAF, NTP time placeholder or time, WiFi/SD flags, uncalibrated battery outline |
| Banner / carousel | 24–81 | Current tab, arrows, 6 selection indicators |
| List viewport | 87–231 | Five rows, 29 px each, accent highlight, scrollbar only as needed |
| Preview | 237–295 | Selected app emblem, title, summary, QEAPP/2 marker if installed |
| Footer | 297–319 | OPTION, OK Open, A Back |

Between regions, the free pixels provide separation. Preview is a description + procedural icon for this migration; cached web thumbnail/cover images are future work, not falsely represented here.

## Navigation

Physical D-Pad UP/DOWN: previous/next list row; LEFT/RIGHT: adjacent tab; single key-down consumes only one tab jump, row repeat is allowed. `START`: open item. `OPTION`: opens dark themed modal. `A`: back to Home. `B`: selected item details. `MENU`: global launcher Home from anywhere except text editor (where MENU finishes text). `SELECT` **short**: same as OK outside text editor. `SELECT` **hold 650ms**: toggle logical Game/T9 mode, without first generating a short event. T9 key mapping 1..9 applies while an OS text editor has focus; hold SELECT again to navigate the virtual keyboard in Game mode.

Launcher redraw policy: no periodic full-screen launcher repaint. On moving the cursor within the same 5-row viewport, repaint only the old/new rows, scrollbar and preview. A tab change/scroll crossing a viewport boundary repaints the whole launcher or the list content respectively. Status updates only repaint 0–23 at most once per 5 seconds (and on WiFi change). Device FPS and DMA usage have not been benchmarked on hardware.

## Tabs and routing

- **Home**: Qeafbrowser, installed apps, file manager, media library, Themes, recent tasks.
- **Internet**: browser, WiFi manager, downloads, browser bookmarks entry (open browser and use its Options menu).
- **Applications**: built-in Apps, App Installer, each verified installed QEAPP/2 package loaded dynamically from existing `AppInstallerService` catalog.
- **Media**: Gallery, Music, Library, File manager, Text reader.
- **System**: WiFi, Bluetooth, Shell, Recovery, Notifications.
- **Settings**: Themes, Settings, Calculator, Stopwatch, About.

**No feature deletion**: original app classes and `ScreenId` routes are retained. Some existing child-app UIs still use older list/softkey layouts; replacing every app interior is outside G3 launcher migration. Built-in dark theme and themed color palette apply globally, and modal in the launcher has its own renderer.

## Structure

```
src/launcher/LauncherTheme.h   // RGB565 skin tokens mapped from VQEAF Studio palette
src/launcher/LauncherView.h    // fixed portrait rectangles / row draw API
src/launcher/LauncherView.cpp  // procedural independent GUI rendering
src/apps/Apps.cpp             // tab catalog + cursor state + dynamic signed .qeapp rows
src/services/ThemeFileService.cpp // streaming VQEAF color/launcher import
src/main.cpp                 // VQEAF splash and launch-on-boot; input priority/status
```

The old 3×4 navigation tests are stored under `docs/legacy`; the active host regression now validates six-tab carousel and bounded list offsets.

# Qeafbrowser downloads and cache – v1.3

## Cache

Successful text/HTML pages up to the browser body cap are cached to `/System/Cache/Web/<FNV32>.htm`. The cache is bounded to 16 files and 512 KiB. When a page cannot be fetched because WiFi is offline or the request fails, Qeafbrowser attempts the matching cached file. Cached pages show a small `C` marker in the address row.

## Downloads

Highlight a link, then choose **Options > Download link**. Data is streamed in 512-byte chunks; no complete download is buffered in RAM.

Routing:

- `.vqeaf` -> `/System/Themes/`
- `.qeapp`, `.zip`, `.vxp` -> `/System/Apps/Inbox/`
- all other files -> `/System/Downloads/`

The firmware caps a single browser download at 4 MiB. A `.part` temporary file is renamed only after the complete transfer succeeds, so interrupted downloads are not exposed as valid packages.

Downloaded application packages are **not automatically executed**. They can be inspected in **Applications > App inbox** or **File Manager > Options > App inbox**. A future installer can validate package metadata/signatures before moving packages to `/System/Apps/Installed`.

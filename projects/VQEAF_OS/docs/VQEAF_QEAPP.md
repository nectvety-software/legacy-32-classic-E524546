# `.qeapp` in VQEAF OS 2.1.0

This release **preserves the uploaded firmware's production QEAPP/2 signed package format** and its existing catalog/service/API. It does not introduce an incompatible ZIP, JSON-directory or executable Lua `.qeapp`. See `docs/QEAPP_V15_SIGNING.md` and `tools/build_qeapp.py` for exact byte-level format, publisher key provisioning and packaging instructions.

Applications tab: 2 built-in entries followed by up to 12 installed signed packages, listed using the existing `AppInstallerService`. The launcher displays each package's verified catalog name, type and signature marker, then hands execution to the firmware's existing `ScreenId::PackageApp` gate. The gate launches only declarative `web` (HTTPS) or `text` (read-only bundled content) packages. QEAPP/2 signatures use a pinned P-256 publisher key. Do not ship example private keys.

App installer: put signed `.qeapp` bundles in `SD:/System/Apps/Inbox/`, then choose **Applications → App installer**. Select the file and install through the pre-existing verifier. A signed bundle is not automatically trusted for execution beyond those two types. Adding Lua support, unrestricted native code, VXP or JS requires a separate restricted runtime and format-version/security review.

# VQEAF OS v2.4.1 — GitHub publish

This prepared local repository has two commits: v2.4.0 base + Pixel Snake feature.

The `.qeapp` demo in `games/pixel_snake/dist/` is signed by a disposable demo key;
it only installs when firmware is compiled with `vqeaf_snake_demo`.
Never commit personal publisher PEM/private keys.

## Checks

```bash
python3 tools/test_pixel_snake.py
python3 tools/verify_v240.py
# Hardware (requires installed PlatformIO/toolchain)
pio run -e vqeaf_snake_demo
```

## Push when remote destination is chosen and authorized

```bash
git remote add origin https://github.com/<owner>/<repository>.git
git push -u origin main
```

No code has been pushed to GitHub by this preparation.

# RCJ UART Link Module — Web Flasher

One-click browser flasher for the [link-module firmware](../../firmware/RCj_link_module/). It
flashes the merged image at offset `0x0` with a full erase and resets the module — the user
picks no files and sets no offsets.

Built from the same proven flasher as the referee module (`../flasher/`, esptool-js 0.6.0,
ESP32-C5). It serves the firmware **same-origin** (`RCj_link_module-*-merged.bin` next to
`manifest.json`), so there are no CORS issues.

## Use it

Web Serial needs a **secure context**, so open it over **HTTPS or `localhost`** (not `file://`),
in **desktop Chrome or Edge**:

- **Hosted (GitHub Pages):** publish the `web/flasher-link/` folder and open its URL.
- **Locally:**
  ```sh
  cd web/flasher-link
  python3 -m http.server 8000
  # open http://localhost:8000
  ```

Then: **Install firmware → pick the module's serial port → wait**. Repeat for the second module.

## Updating to a new firmware version

1. Build + merge the new firmware (see `firmware/RCj_link_module/README.md` → For developers).
2. Replace `RCj_link_module-*-merged.bin` here.
3. Update `manifest.json` (`parts[].path`) and `version.json` (`version`, `files.merged.size`,
   `files.merged.sha256`, `releaseUrl`).

## Files

| File | Role |
|------|------|
| `index.html`, `app.js`, `styles.css` | UI + flashing logic (app.js/styles.css shared with `../flasher/`) |
| `manifest.json` | chip family + part list (merged image @ `0x0`) |
| `version.json` | version / size / sha256 / release link shown in the UI |
| `RCj_link_module-*-merged.bin` | the firmware image flashed to the module |

# XIAO bootloaders with distinct drive names

Stock Seeed XIAO nRF52840 Sense bootloaders all mount as `XIAO-SENSE`, so the dongle and the
trackpad XIAOs look identical. These are Adafruit nRF52 Bootloader 0.11.0 builds of the
`xiao_nrf52840_ble_sense` board with only `UF2_VOLUME_LABEL` changed (FAT labels: max 11 chars):

| File | Device | Drive name |
|---|---|---|
| `update-xiao_ble_sense_dongle_bootloader-0.11.0_nosd.uf2` | Prospector dongle | `DONGLEBOOT` |
| `update-xiao_ble_sense_tpad_lh_bootloader-0.11.0_nosd.uf2` | left trackpad XIAO | `TPADLHBOOT` |
| `update-xiao_ble_sense_tpad_rh_bootloader-0.11.0_nosd.uf2` | right trackpad XIAO | `TPADRHBOOT` |

All three are for the **Sense** variant with SoftDevice S140 7.3.0 (check `INFO_UF2.TXT`:
`Board-ID: Seeed_XIAO_nRF52840_Sense`, `SoftDevice: S140 version 7.3.0`). A plain XIAO
(`XIAO-BOOT`) needs a `xiao_nrf52840_ble`-based build instead.

## Flashing
Double-tap reset, copy the `update-*.uf2` onto the drive, and don't unplug until it restarts by
itself (a few seconds). Self-update needs a running bootloader >= 0.4.0 (stock is 0.6.1).
The SoftDevice and the ZMK application are kept. Power loss during the update needs SWD to
recover.

Done (2026-11-01, verified 0.11.0, ZMK app intact): left trackpad (`TPADLHBOOT`), dongle (`DONGLEBOOT`), right trackpad XIAO (`TPADRHBOOT`, no pad wired yet).

## Rebuilding
```
git clone --branch 0.11.0 https://github.com/adafruit/Adafruit_nRF52_Bootloader && cd Adafruit_nRF52_Bootloader
git submodule update --init
cp -r src/boards/xiao_nrf52840_ble_sense src/boards/xiao_ble_sense_dongle
sed -i 's/#define UF2_VOLUME_LABEL ".*"/#define UF2_VOLUME_LABEL "DONGLEBOOT"/' src/boards/xiao_ble_sense_dongle/board.h
# needs arm-none-eabi-gcc and adafruit-nrfutil (pip) on PATH
make BOARD=xiao_ble_sense_dongle all   # -> _build/build-xiao_ble_sense_dongle/update-*_nosd.uf2
```

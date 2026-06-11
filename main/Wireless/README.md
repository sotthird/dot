# Wireless

Initializes Wi-Fi (station mode) and BLE, and runs one-shot scans for nearby
access points and BLE devices, logging what it finds.

## What it does

- Initializes NVS (auto-erasing and retrying if the partition is the wrong
  version or full — required before the Wi-Fi/BT stacks can start).
- Spawns two scanning tasks pinned to separate cores so Wi-Fi and BLE scans run
  in parallel:
  - **`WIFI_Init`** (core 0, priority 3) — brings up Wi-Fi in station mode and
    performs a blocking AP scan.
  - **`BLE_Init`** (core 1, priority 2) — releases the unused Classic-Bluetooth
    memory, brings up BLE, and performs a fixed-duration GAP scan.
- Both tasks delete themselves once their scan is launched/completed.
- BLE results are de-duplicated by MAC address and have their advertised device
  name extracted (supports both "complete" and "shortened" name AD types);
  matching devices and counts are logged.
- `Scan_finish` is set once **both** scans have completed, so calling code can
  wait on a single flag.

## Public API

| Function | Description |
|---|---|
| `void Wireless_Init(void)` | Initializes NVS and spawns the Wi-Fi and BLE scanning tasks. Call once at startup. |
| `void WIFI_Init(void *arg)` | Wi-Fi init + scan task entry point (usually not called directly — spawned by `Wireless_Init`). |
| `uint16_t WIFI_Scan(void)` | Performs a blocking Wi-Fi AP scan, returns the AP count. |
| `void BLE_Init(void *arg)` | BLE init + scan task entry point (usually not called directly). |
| `uint16_t BLE_Scan(void)` | Starts a GAP scan and returns the discovered device count. |

## Globals

| Variable | Meaning |
|---|---|
| `uint16_t WIFI_NUM` | Number of Wi-Fi access points found |
| `uint16_t BLE_NUM` | Number of unique BLE devices found |
| `bool Scan_finish` | Set true once both scans have completed |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `SCAN_DURATION` | `5` seconds | How long the BLE GAP scan runs |
| `MAX_DISCOVERED_DEVICES` | `100` | Size of the static BLE device dedup table |
| BLE scan interval / window | `0x50` / `0x30` | Active-scan timing (in 0.625 ms units — 80 / 48 slots) |
| BLE scan filter / dedup | allow-all / disabled | All advertisements are seen, including repeats |
| BLE address type | RPA public | Resolvable Private Address |

## Notes

- **Why two cores:** running Wi-Fi and BLE scans concurrently on separate cores
  roughly halves the total scan time compared to running them sequentially.
- `esp_bt_controller_mem_release()` is called to free the Classic Bluetooth
  memory, since this code path only uses BLE — don't remove this if you're
  memory-constrained, but you'll need to revisit it if Classic BT is ever needed.
- The original verbose per-device logging is left commented out in the GAP
  callback; only summary counts are printed by default. Uncomment it if you need
  to debug specific devices.
- BLE scanning is stopped via `esp_ble_dtm_stop()`, which is the Direct Test Mode
  stop API being repurposed to end a regular GAP scan — unconventional, but it
  works; be aware of this if you're cross-referencing ESP-IDF BLE examples.
- This module brings up its own NVS/Wi-Fi/BT stacks — if your application also
  needs Wi-Fi for something else (e.g. the abandoned UDP CPU-monitor approach),
  make sure you don't double-initialize these subsystems.

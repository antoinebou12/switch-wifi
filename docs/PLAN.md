# switch-wifi implementation plan

## Phase 1 - diagnostic toolkit [implemented]

- [x] C++17 + libnx project
- [x] Borealis controller/touch UI
- [x] Dashboard tab with shader-style signal visualization
- [x] NIFM current link/IP/DNS/MTU information
- [x] Horizon 0-3 signal bars
- [x] Legacy `wlan:inf` RSSI where available
- [x] Rolling signal graph
- [x] Saved Wi-Fi basic metadata without full saved credential-profile reads
- [x] Horizon allowed-channel query
- [x] Channel/band/frequency helpers
- [x] Contention-proxy calculation and tests
- [x] Repeated HTTPS latency probes
- [x] Min/average/median/P95/jitter statistics
- [x] HTTPS download/upload benchmark
- [x] Cancellable background transfers
- [x] Download and latency history graphs
- [x] Bluetooth service/device counts
- [x] Passive bounded BLE discovery
- [x] Diagnostics report export
- [x] Split host unit tests
- [x] GCC + Clang CI lanes
- [x] ASan + UBSan CI lane
- [x] devkitPro NRO build and NRO structure verification
- [x] released/current toolchain NRO matrix
- [x] moving current-libnx-master NRO compatibility lane
- [x] NRO SHA-256 artifact upload
- [x] tag/version-checked GitHub Release CD
- [x] README badges/features/build guide
- [x] compatibility + benchmark + development docs
- [x] Mermaid + PlantUML documentation

## Phase 2 - verified nearby Wi-Fi scan

- [ ] Implement isolated NIFM `CreateScanRequest` wrapper
- [ ] Submit/wait/read scan request without blocking UI
- [ ] Capture opaque AP records on representative Horizon versions
- [ ] Verify SSID/BSSID/channel/RSSI/security offsets independently
- [ ] Add binary parser fixtures and host tests
- [ ] Display nearby AP list
- [ ] Plot AP strength graph
- [ ] Plot channel occupancy
- [ ] Enable contention proxy using measured scan data

## Phase 3 - modern WLAN diagnostics

- [ ] Add HOS 15+ WLAN service adapter if launch permissions permit
- [ ] Verify connection-status structure
- [ ] Verify RSSI/signal-level outputs
- [ ] Verify allowed-channel query
- [ ] Evaluate TX-power query where available
- [ ] Compare values against NIFM/legacy behavior
- [ ] Add firmware-specific hardware fixtures/regression tests

## Phase 4 - controlled benchmark lab

- [ ] Runtime-configurable LAN benchmark endpoint
- [ ] DNS-resolution timing
- [ ] Application probe failure/loss percentage
- [ ] CSV/JSON benchmark session export
- [ ] Session comparison screen
- [ ] Local-server companion instructions/container
- [ ] Device matrix for OLED/V2/Lite and docked/handheld testing

## Non-goals

- saved passphrase display/export
- deauthentication or packet injection
- credential capture
- monitor-mode attacks
- fabricated RF noise floor/SNR
- Bluetooth pairing/disconnection controls

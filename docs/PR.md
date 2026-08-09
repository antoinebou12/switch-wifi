# Draft PR: switch-wifi benchmark + diagnostics toolkit

## Summary

Introduces a release-oriented `switch-wifi` Nintendo Switch network diagnostics application using Borealis, libnx and libcurl, with host-tested metric logic, a verified NRO packaging target, richer benchmark statistics, shader-style graphs, compatibility documentation and CI/CD.

## Included

- NIFM connection snapshot backend: Wi-Fi/Ethernet state, SSID, IP/subnet/gateway/DNS/MTU.
- Horizon Wi-Fi bars plus legacy `wlan:inf` RSSI only where supported.
- Released-libnx-compatible saved-profile metadata adapter.
- Horizon allowed-channel query and channel/band/frequency calculations.
- Labeled channel-contention proxy model.
- Repeated HTTPS latency probes with min/average/median/P95/jitter.
- Cancellable HTTPS download and zero-filled upload benchmark.
- Read-only BTM state/device counts and bounded passive BLE discovery.
- Borealis tabs: Dashboard, Networks, Benchmark, Bluetooth, About.
- NanoVG shader-style signal status panel and gradient/glow history graphs.
- Plain-text diagnostics export with credentials excluded from output.
- Three host unit-test executables.
- GCC/Clang warnings-as-errors lanes and ASan/UBSan lane.
- NRO builds against stable/current devkitPro images plus current libnx `master`.
- NRO magic verification, SHA-256 artifacts and version-checked release CD.
- README badges and build/test instructions.
- Architecture, benchmark, compatibility, development, Mermaid and PlantUML docs.

## Deliberate verification boundary

Nearby Wi-Fi AP field decoding is not represented as complete. The project does not ship guessed offsets for SSID/BSSID/channel/RSSI. Modern HOS also has newer WLAN services that need hardware/service-permission validation first.

A real RF noise floor/SNR is not fabricated. The implemented overlap score is explicitly a **contention proxy**.

The benchmark is explicitly an **end-to-end HTTPS benchmark**, not a raw Wi-Fi PHY/link-rate measurement.

## Validation

```bash
cmake -S . -B build-host \
  -DSWITCH_WIFI_BUILD_APP=OFF \
  -DSWITCH_WIFI_BUILD_TESTS=ON \
  -DSWITCH_WIFI_WARNINGS_AS_ERRORS=ON
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure

git diff --check
```

CI performs the ARM64 NRO builds and verifies the resulting NRO header. Physical Switch validation remains a separate requirement because compile success cannot prove Horizon service availability/runtime behavior.

## Follow-up

1. Collect and verify nearby-AP scan fixtures on physical hardware.
2. Add HOS 15+ modern WLAN signal/channel adapter after permission/layout validation.
3. Add runtime-configurable local benchmark endpoints and session comparison/export.
4. Populate the compatibility matrix with OLED/V2/Lite hardware results.

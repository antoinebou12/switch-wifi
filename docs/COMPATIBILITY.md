# Compatibility

Last reviewed: **2026-08-09**

`switch-wifi` is built as normal libnx homebrew and is designed to run under Atmosphère/hbmenu. Compatibility is split into a compile-time baseline and a moving compatibility lane so the project remains reproducible while also detecting libnx API regressions early.

## Supported baseline

| Component | Project target | Policy |
| --- | --- | --- |
| Atmosphère | 1.11.2 | Current runtime target; includes basic HOS 22.5.0 support. Hardware validation is tracked separately from compile validation. |
| Horizon OS | 22.5.0 | Current target for runtime testing. |
| libnx | 4.10.0+ | Minimum released ABI baseline for modern HOS homebrew. CI also compiles against current `switchbrew/libnx` master. |
| Borealis | `xfangfang/borealis` `wiliwili` @ `5f08b286` | Pinned submodule for reproducible UI builds. |
| C++ | C++17 | Core and Switch application. |

Atmosphère 1.10.0 documented that HOS 21.0.0 changed the userland/kernel TLS ABI and homebrew may need to be rebuilt with libnx 4.10.0 or newer. For that reason `switch-wifi` does not support binaries built against older libnx as its modern compatibility baseline.

## CI compatibility lanes

The CI pipeline intentionally tests three different concerns:

1. **Released devkitPro image**: reproducible NRO build with `devkitpro/devkita64:20260219`.
2. **Current devkitPro image**: catches changes to the published Switch toolchain image.
3. **libnx master**: starts from the current devkitPro image, builds the latest `switchbrew/libnx` `master`, installs it in the CI container, and compiles the NRO again.

This does not prove runtime behavior on a physical console. It proves that the source remains build-compatible with both the released toolchain baseline and current libnx.

## Horizon networking compatibility

### Current connection

NIFM provides the current internet status, IP configuration and network profile. The UI normalizes these values into `ConnectionSnapshot`.

### Signal strength

- **HOS < 15.0.0**: the legacy `wlan:inf` service can provide RSSI in dBm when available.
- **HOS 15.0.0+**: `wlan:inf` is no longer available. `switch-wifi` falls back to the NIFM 0-3 Wi-Fi strength value until the modern WLAN scan/status path is validated on hardware.

A bar value is never relabeled as dBm.

### Saved profiles

Released libnx 4.10.0 does not include the typed `nifmEnumerateNetworkProfiles()` helper that was merged later on `master`. `switch-wifi` therefore uses a small compatibility adapter for NIFM `IGeneralService` command 7 and decodes only the basic profile record used by the upstream implementation.

The adapter reads only basic profile metadata. It does not request the full saved profile object containing credential storage fields.

### Nearby access-point scanning

NIFM and newer WLAN services expose scan operations, but the access-point data layout is not treated as stable in this project until verified against real hardware and multiple HOS versions. The public UI therefore does not guess BSSID, RSSI or channel offsets from opaque records.

## Hardware validation matrix

| Runtime | Build | Status |
| --- | --- | --- |
| Atmosphère 1.11.2 / HOS 22.5.0 | latest-libnx CI lane | **Target, hardware validation required** |
| Atmosphère 1.11.x / HOS 22.x | released/current devkitPro lanes | Expected, hardware validation required |
| Atmosphère 1.10.x / HOS 21.x | libnx 4.10.0+ | Expected, hardware validation required |
| HOS < 15 | legacy RSSI path | Supported by code path; hardware validation varies by environment |

When reporting a hardware result, include Switch model, HOS, Atmosphère, hbmenu launch mode, docked/handheld state, Wi-Fi band/channel, AP model, and the NRO SHA-256.

# Horizon / libnx research notes

Last reviewed: **2026-08-09**. Re-check these assumptions before changing raw IPC.

## NIFM

Released libnx exposes wrappers for the baseline connection data used by this application: current IP configuration, current network profile, internet connection type/status, Wi-Fi strength bars and wireless enabled state.

The typed `nifmEnumerateNetworkProfiles()` helper was merged to libnx `master` after the 4.10.0 release. To keep a released 4.10.0 compile baseline, `switch-wifi` isolates NIFM `IGeneralService` command 7 in `NetworkService` using the compact basic-profile wire record from the upstream implementation. This avoids requesting full saved profile objects for the Networks page.

NIFM also exposes scan-request and scan-data operations. Reverse-engineered interface descriptions identify an access-point record, but this project does not treat field offsets as stable until they are correlated with known AP data on physical Switch hardware.

## `wlan:inf`

Older Horizon versions provide `wlan:inf`, and libnx exposes `wlaninfGetRSSI()`. The service was removed on HOS 15.0.0+, so it is only an optional legacy source.

The UI never estimates dBm from NIFM's 0-3 bar value.

## Modern WLAN service

Modern Horizon provides newer WLAN service interfaces with scan and connection diagnostics. This is the likely long-term route for richer HOS 15+ signal/channel information. Before enabling it, the project needs verified service permissions, command signatures, versioned structures and hardware fixtures.

## Atmosphère / libnx compatibility

Atmosphère 1.11.2 is the current runtime target and adds basic support for HOS 22.5.0. Atmosphère's HOS 21 transition also documented a TLS ABI change requiring homebrew to be rebuilt with libnx 4.10.0+ in affected cases. The project therefore treats libnx 4.10.0 as its minimum modern released baseline and has an additional CI lane against current libnx `master`.

Compile compatibility is not equivalent to runtime validation. Every new firmware path must still be tested on physical hardware.

## Borealis lifecycle

Borealis' Switch wrapper initializes BSD sockets and NIFM before application `main()`. `switch-wifi` must not initialize the BSD socket layer a second time. Project service classes may use libnx's service guards for scoped NIFM/BTM access, but raw platform lifecycle ownership stays consistent with Borealis.

## Bluetooth

libnx BTM wrappers provide device-condition/device-info queries and general BLE scan commands on supported firmware. `BtdrvBleScanResult` exposes a Bluetooth address while much of the record remains opaque, so the UI only displays values whose meaning is known.

## Noise / channel utilization

A real RF noise floor is not the same as nearby AP density. Until a verified accessible service exposes PHY noise or channel statistics, `switch-wifi` uses **contention proxy** for its derived overlap score.

## Benchmark semantics

The current benchmark uses libcurl HTTPS requests. `CURLINFO_STARTTRANSFER_TIME` is an application/network-path measurement and includes more than radio latency. Throughput also includes the LAN/Internet path and endpoint capacity. Use a wired local endpoint when the research question is Wi-Fi rather than ISP performance.

## Next validation work

- Test service availability from hbmenu applet mode vs full-memory title override.
- Capture scan buffers on HOS 21/22.x without recording credentials.
- Correlate buffers with router-known SSID/BSSID/channel values.
- Validate modern WLAN service permissions and outputs.
- Build a fixture corpus before public AP-field decoding.
- Run the benchmark on OLED/V2/Lite hardware with controlled 2.4/5 GHz configurations.

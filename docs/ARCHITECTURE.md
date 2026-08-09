# Architecture

## Goals

`switch-wifi` is a diagnostic application first. Every displayed metric should have clear provenance: direct measurement, Horizon-reported state, or derived estimate.

## Layers

### `core`

Platform-independent models and calculations:

- link/signal enums and snapshots
- throughput conversion
- latency distribution and jitter summaries
- ring history
- Wi-Fi band/frequency mapping
- channel-overlap weighting
- contention proxy

This layer has no libnx or Borealis dependency and is exercised by host CI.

### `network`

`NetworkService` owns NIFM and legacy `wlan:inf` access. It exposes only normalized application models.

`SpeedTestEngine` owns libcurl transfers. It runs synchronously from the engine's point of view while Borealis invokes it on a background worker. Cancellation uses an atomic flag through libcurl's progress callback.

### `bluetooth`

`BluetoothService` owns BTM calls. Normal snapshots are short-lived and read-only. BLE discovery is explicit and bounded; it starts a general BLE scan, sleeps for a capped interval, reads at most ten results from the libnx API, stops the scan, and returns unique raw addresses.

### `ui`

The Borealis layer renders normalized models and owns navigation, histories, graphs, async scheduling, and diagnostics export. `SignalShaderView` and `SparklineView` use NanoVG gradient/glow paints inside Borealis' rendering pipeline, avoiding unmanaged raw OpenGL state. UI changes from worker tasks are marshalled with `brls::sync`.

## Firmware compatibility policy

1. Prefer libnx wrappers when they exist.
2. Gate firmware-specific APIs using `hosversion*` helpers.
3. Do not decode undocumented buffers by assumption.
4. Put experimental/raw IPC behind a dedicated adapter and unit-test all independent parsing logic.
5. A missing metric is shown as unavailable, never substituted with a different quantity under the same label.

## Wi-Fi scan boundary

NIFM has scan request/data commands, but the public reverse-engineered `AccessPointData` record is still opaque at the field level. Modern WLAN services also expose richer scan/connection commands, but libnx does not currently provide a stable high-level wrapper for them in this project.

The project therefore already contains the model and contention calculations, while AP-buffer parsing remains a separate milestone. This avoids baking a guessed structure into the public UI.

## Threading

- Dashboard/profile refresh: UI thread, short IPC operations.
- Speed test: Borealis background task.
- BLE scan: Borealis background task.
- UI mutation from workers: `brls::sync` only.

## Data handling

Saved-network display uses the NIFM basic-profile command through a compatibility adapter rather than retrieving full saved profile objects containing passphrase storage fields. The active network profile is read for connection metadata, then treated as short-lived data; credential fields are never displayed or exported.


## Benchmark pipeline

The benchmark performs a bounded series of HTTPS latency probes, summarizes them as minimum/average/median/P95/jitter, then performs bounded download and zero-filled upload transfers. `SpeedTestEngine` never mutates Borealis views directly; progress/results are marshalled back to the UI thread.

## Build compatibility

The host core intentionally has no libnx dependency. Switch CI builds against both official devkitPro images and a moving libnx-master lane. This catches C++/API breakage independently from physical-console runtime validation. See [`COMPATIBILITY.md`](COMPATIBILITY.md).

# Wi-Fi benchmark methodology

The benchmark is designed for **repeatable network-path comparison on a Nintendo Switch**, not for claiming raw Wi-Fi PHY rate.

## What is measured

### Application latency

The default benchmark makes eight HTTPS requests to the configured download endpoint with a zero-byte response request. For each probe it records libcurl `STARTTRANSFER_TIME`, then reports:

- minimum;
- average;
- median;
- P95;
- mean absolute successive-difference jitter;
- probe count.

This includes DNS/TCP/TLS/server effects as applicable to the connection. It is **not ICMP ping**, radio airtime latency or controller-level Wi-Fi latency.

### Download throughput

The default transfer downloads **25 MiB** and discards the response body. Throughput is calculated from bytes transferred and elapsed wall time.

### Upload throughput

The default transfer uploads **10 MiB** of generated zero-filled memory. No SD-card file or user payload is uploaded.

## Default endpoint

The engine defaults to Cloudflare's speed-test compatible endpoints:

- download: `https://speed.cloudflare.com/__down`
- upload: `https://speed.cloudflare.com/__up`

Endpoints are configuration values in `SpeedTestConfig` and can be replaced by a self-hosted compatible target for LAN-only testing.

## Recommended test protocol

For useful Wi-Fi comparisons:

1. Run the NRO as a full application/title takeover rather than the constrained Album applet when possible.
2. Keep the Switch in one physical position and orientation.
3. Record docked/handheld state and any CPU/GPU/RAM overclock separately.
4. Run at least three complete benchmark passes per configuration.
5. Compare median latency and median throughput rather than only the single best result.
6. Change one variable at a time, such as AP node, 2.4/5 GHz band, channel, channel width, router feature or Switch clock profile.
7. If testing Wi-Fi itself rather than ISP capacity, point the benchmark at a local server on wired Ethernet.

## Interpretation

A 200 Mbps result means the Switch delivered roughly that application throughput over the entire tested network path. It does **not** mean the Wi-Fi PHY negotiated exactly 200 Mbps.

Similarly, the derived channel contention score is a comparison proxy. It is not RF noise floor and not SNR. True noise/SNR is shown as unavailable until a verified Horizon API exposes it.

## Reproducibility record

The diagnostics export should accompany serious benchmark results. Capture:

- `switch-wifi` version / commit;
- NRO SHA-256;
- Atmosphère and HOS version;
- Switch model;
- SSID/band/channel when available;
- IP/MTU;
- endpoint;
- latency sample count;
- median/min/P95/jitter;
- download and upload Mbps.

# UML and runtime flows

The diagrams below are source-controlled Mermaid so GitHub can render them directly. A PlantUML equivalent is also kept in [`uml/switch-wifi.puml`](uml/switch-wifi.puml).

## Component diagram

```mermaid
flowchart LR
    User[User / Joy-Con / Touch]
    UI[Borealis UI\nMainTabs]
    Shader[SignalShaderView\nSparklineView]
    Core[Core metrics\nmodels + histories]
    Net[NetworkService]
    Bench[SpeedTestEngine]
    BT[BluetoothService]
    NIFM[Horizon NIFM]
    WLAN[legacy wlan:inf\nHOS < 15]
    BTM[Horizon BTM]
    CURL[libcurl / BSD sockets]
    Endpoint[HTTPS benchmark endpoint]

    User --> UI
    UI --> Shader
    UI --> Net
    UI --> Bench
    UI --> BT
    Net --> Core
    Bench --> Core
    BT --> Core
    Net --> NIFM
    Net -. legacy RSSI .-> WLAN
    BT --> BTM
    Bench --> CURL --> Endpoint
```

## Class diagram

```mermaid
classDiagram
    class MainTabs {
      -NetworkService network
      -BluetoothService bluetooth
      -RingHistory signalHistory
      -RingHistory downloadHistory
      -RingHistory latencyHistory
      +refreshDashboard()
      +runBenchmark()
      +exportDiagnostics()
    }

    class NetworkService {
      +initialize() bool
      +snapshot() ConnectionSnapshot
      +savedNetworks() vector~SavedNetwork~
      +allowedChannels() vector~int~
    }

    class SpeedTestEngine {
      -atomic_bool cancelled
      +run(config, progress) SpeedSample
      +cancel()
    }

    class BluetoothService {
      +snapshot() BluetoothSnapshot
      +scanBle(duration) vector~string~
    }

    class SignalShaderView {
      -float strength
      -bool connected
      +setSignal(strength, connected)
      +draw(ctx, x, y, w, h)
    }

    class SparklineView {
      -vector~float~ samples
      +setSamples(samples)
      +draw(ctx, x, y, w, h)
    }

    class ConnectionSnapshot
    class SpeedSample
    class LatencyStats

    MainTabs --> NetworkService
    MainTabs --> SpeedTestEngine
    MainTabs --> BluetoothService
    MainTabs --> SignalShaderView
    MainTabs --> SparklineView
    NetworkService --> ConnectionSnapshot
    SpeedTestEngine --> SpeedSample
    SpeedSample --> LatencyStats
```

## Benchmark sequence

```mermaid
sequenceDiagram
    actor User
    participant UI as Borealis MainTabs
    participant Worker as brls::async worker
    participant Engine as SpeedTestEngine
    participant Curl as libcurl
    participant Server as HTTPS endpoint

    User->>UI: Run benchmark
    UI->>Worker: async(run)
    Worker->>Engine: run(config, progress)
    loop latency probes (default 8)
        Engine->>Curl: GET __down?bytes=0
        Curl->>Server: HTTPS request
        Server-->>Curl: headers / response
        Curl-->>Engine: STARTTRANSFER_TIME
        Engine-->>UI: brls::sync(progress)
    end
    Engine->>Engine: median/min/P95/jitter
    Engine->>Curl: bounded download
    Curl->>Server: GET bytes=N
    Server-->>Curl: N bytes
    Engine->>Curl: bounded zero-filled upload
    Curl->>Server: POST N bytes
    Server-->>Curl: response
    Engine-->>Worker: SpeedSample
    Worker-->>UI: brls::sync(result + graphs)
    UI-->>User: Render metrics
```

## CI/CD deployment diagram

```mermaid
flowchart TD
    Push[Push / PR] --> HostGCC[GCC unit tests]
    Push --> HostClang[Clang unit tests]
    Push --> Sanitizers[ASan + UBSan]
    Push --> NROStable[devkita64:20260219\nreleased baseline]
    Push --> NROLatest[devkita64:latest]
    Push --> NROMaster[devkita64:latest\n+ libnx master]

    NROStable --> Verify[verify NRO0 + SHA-256]
    NROLatest --> Verify
    NROMaster --> Verify
    Verify --> Artifacts[GitHub Actions artifacts]

    Tag[vX.Y.Z tag] --> ReleaseBuild[Release NRO build + tests]
    ReleaseBuild --> Package[.nro + checksum + ZIP]
    Package --> Release[GitHub Release]
```

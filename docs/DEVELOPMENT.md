# Development guide

## Repository setup

```bash
git clone --recursive https://github.com/antoinebou12/switch-wifi.git
cd switch-wifi
```

If the checkout already exists:

```bash
git submodule update --init --recursive
```

## Host unit tests

```bash
cmake -S . -B build-host \
  -DSWITCH_WIFI_BUILD_APP=OFF \
  -DSWITCH_WIFI_BUILD_TESTS=ON \
  -DSWITCH_WIFI_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
```

### Sanitizers

```bash
cmake -S . -B build-sanitize \
  -DSWITCH_WIFI_BUILD_APP=OFF \
  -DSWITCH_WIFI_BUILD_TESTS=ON \
  -DSWITCH_WIFI_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Nintendo Switch NRO

The recommended reproducible build environment is the official devkitPro Switch Docker image.

```bash
docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64:latest \
  bash -lc 'cmake -S . -B build-switch -DPLATFORM_SWITCH=ON -DUSE_GLFW=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build-switch --target switch-wifi.nro --parallel'
```

Expected output:

```text
build-switch/switch-wifi.nro
```

Validate the container with:

```bash
python3 scripts/verify_nro.py build-switch/switch-wifi.nro
```

## Latest libnx compatibility

CI has a moving lane that builds and installs `switchbrew/libnx` `master` inside the official devkitPro image, then recompiles the application. This lane is intentionally separate from the pinned/released toolchain lane.

Do not make application code depend on a master-only wrapper when the same stable Horizon command can be isolated behind a small compatibility adapter. Moving APIs should stay behind `NetworkService` rather than leaking into UI code.

## UI rules

- Borealis UI mutation occurs on the UI thread.
- Worker results use `brls::sync`.
- Expensive network operations use `brls::async`.
- Shader-style status visuals use Borealis/NanoVG's existing rendering pipeline to avoid corrupting raw GL state.
- Every graph label must identify the source/units of the metric.

## Network metric rules

Before adding a metric, classify it as one of:

- directly measured;
- reported by Horizon;
- derived/proxy;
- unavailable.

Do not convert a proxy into a physical quantity by relabeling it. In particular, channel crowding is not noise floor and Horizon bars are not dBm.

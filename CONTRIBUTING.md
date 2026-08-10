# Contributing

## Development rules

- Keep Horizon/libnx IPC out of the Borealis UI layer.
- Add a host-side test for calculations and parsers whenever possible.
- Gate firmware-dependent behavior explicitly.
- Do not add code that retrieves Wi-Fi passphrases just to display profile information.
- Do not call a derived AP-overlap value "noise floor", "SNR", or "channel utilization" unless the underlying metric is actually measured.
- Keep Bluetooth diagnostics read-only.

## Host checks

```bash
cmake -B build-host -DSWITCH_WIFI_BUILD_APP=OFF -DSWITCH_WIFI_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## Switch build

```bash
git submodule update --init --recursive
cmake -B build -DPLATFORM_SWITCH=ON -DUSE_GLFW=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target switch-wifi.nro -j
```

## Pull requests

Describe:

- firmware versions tested
- launch method (applet mode/full-memory)
- which values were directly measured vs derived
- how undocumented structures were validated

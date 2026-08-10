#include "switch_wifi/bluetooth/bluetooth_service.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>

namespace swifi {
namespace {

#ifdef __SWITCH__
std::string formatAddress(const BtdrvAddress& address) {
    char text[18]{};
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  address.address[0], address.address[1], address.address[2],
                  address.address[3], address.address[4], address.address[5]);
    return text;
}
#endif

} // namespace

BluetoothSnapshot BluetoothService::snapshot() {
    BluetoothSnapshot out;
#ifdef __SWITCH__
    Result rc = btmInitialize();
    if (R_FAILED(rc)) {
        out.diagnostic = "BTM service unavailable in this launch context";
        return out;
    }

    out.serviceAvailable = true;
    BtmState state{};
    if (R_SUCCEEDED(btmGetState(&state))) out.state = static_cast<int>(state);

    if (hosversionAtLeast(13, 0, 0)) {
        BtmConnectedDeviceV13 connected[16]{};
        s32 connectedCount = 0;
        if (R_SUCCEEDED(btmGetDeviceCondition(BtmProfile_None, connected, 16, &connectedCount))) out.connectedDevices = connectedCount;

        BtmDeviceInfoV13 known[16]{};
        s32 knownCount = 0;
        if (R_SUCCEEDED(btmGetDeviceInfo(BtmProfile_None, known, 16, &knownCount))) out.knownDevices = knownCount;
    }

    out.diagnostic = "Read-only Bluetooth diagnostics";
    btmExit();
#else
    out.serviceAvailable = true;
    out.state = 7;
    out.connectedDevices = 2;
    out.knownDevices = 4;
    out.diagnostic = "Desktop mock backend";
#endif
    return out;
}

BluetoothScanResult BluetoothService::passiveScan(std::uint32_t durationMs) {
    BluetoothScanResult out;
#ifdef __SWITCH__
    Result rc = btmInitialize();
    if (R_FAILED(rc)) {
        out.diagnostic = "BTM service unavailable";
        return out;
    }
    out.serviceAvailable = true;

    if (hosversionBefore(5, 1, 0)) {
        out.diagnostic = "General BLE scan requires HOS 5.1.0+";
        btmExit();
        return out;
    }

    BtdrvBleAdvertisePacketParameter parameter{};
    rc = btmGetBleScanParameterGeneral(1, &parameter);
    if (R_FAILED(rc)) {
        out.diagnostic = "BLE scan parameters unavailable in this launch context";
        btmExit();
        return out;
    }

    rc = btmStartBleScanForGeneral(parameter);
    if (R_FAILED(rc)) {
        out.diagnostic = "BLE scan start denied/unavailable";
        btmExit();
        return out;
    }
    out.scanSupported = true;

    const std::uint32_t boundedDuration = std::max<std::uint32_t>(250, std::min<std::uint32_t>(durationMs, 5000));
    svcSleepThread(static_cast<s64>(boundedDuration) * 1'000'000LL);

    std::array<BtdrvBleScanResult, 10> raw{};
    u8 total = 0;
    rc = btmGetBleScanResultsForGeneral(raw.data(), static_cast<u8>(raw.size()), &total);
    btmStopBleScanForGeneral();

    if (R_FAILED(rc)) {
        out.diagnostic = "BLE scan completed but results could not be read";
        btmExit();
        return out;
    }

    std::set<std::string> unique;
    const std::size_t count = std::min<std::size_t>(total, raw.size());
    for (std::size_t i = 0; i < count; ++i) {
        const std::string address = formatAddress(raw[i].addr);
        if (unique.insert(address).second) out.devices.push_back({address, {}});
    }

    out.diagnostic = out.devices.empty() ? "No BLE advertisements returned during bounded scan" : "Passive BLE addresses only; advertisement payload remains opaque";
    btmExit();
#else
    out.serviceAvailable = true;
    out.scanSupported = true;
    out.devices = {{"01:23:45:67:89:AB", "Desktop mock"}, {"10:20:30:40:50:60", {}}};
    out.diagnostic = "Desktop mock backend";
#endif
    return out;
}

} // namespace swifi

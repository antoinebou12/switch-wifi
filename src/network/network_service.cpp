#include "switch_wifi/network/network_service.hpp"

#ifdef __SWITCH__
#include <switch.h>
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>

namespace swifi {
namespace {

#ifdef __SWITCH__
std::string ipv4ToString(u32 raw) {
    char buffer[INET_ADDRSTRLEN]{};
    in_addr addr{};
    addr.s_addr = raw;
    if (!inet_ntop(AF_INET, &addr, buffer, sizeof(buffer))) return "-";
    return buffer;
}

struct NifmBasicProfileWire {
    Uuid uuid;
    char networkName[0x40];
    u8 profileType;
    u8 connectionType;
    u8 ssidLen;
    char ssid[0x20];
    u8 authentication;
    u8 encryption;
};
static_assert(sizeof(NifmBasicProfileWire) == 0x75, "NIFM basic-profile wire layout changed");

std::string authName(u8 value) {
    switch (value) {
        case 1: return "Open";
        case 2: return "Shared";
        case 3: return "WPA";
        case 4: return "WPA-PSK";
        case 5: return "WPA2";
        case 6: return "WPA2-PSK";
        case 7: return "Unknown (7)";
        default: return "Unknown";
    }
}

std::string encryptionName(u8 value) {
    switch (value) {
        case 1: return "None";
        case 2: return "WEP";
        case 3: return "TKIP";
        case 4: return "AES";
        default: return "Unknown";
    }
}

Result enumerateUserProfiles(std::array<NifmBasicProfileWire, 32>& profiles, s32* total) {
    Service* general = nifmGetServiceSession_GeneralService();
    serviceAssumeDomain(general);
    const u8 profileTypeUser = 1;
    return serviceDispatchInOut(general, 7, profileTypeUser, *total,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = { { profiles.data(), profiles.size() * sizeof(profiles[0]) } },
    );
}
#endif

} // namespace

bool NetworkService::initialize() {
#ifdef __SWITCH__
    if (initialized_) return true;
    const Result rc = nifmInitialize(NifmServiceType_User);
    initialized_ = R_SUCCEEDED(rc);
    if (initialized_ && hosversionBefore(15, 0, 0)) wlanInfInitialized_ = R_SUCCEEDED(wlaninfInitialize());
    return initialized_;
#else
    initialized_ = true;
    return true;
#endif
}

void NetworkService::shutdown() {
#ifdef __SWITCH__
    if (wlanInfInitialized_) { wlaninfExit(); wlanInfInitialized_ = false; }
    if (initialized_) { nifmExit(); initialized_ = false; }
#else
    initialized_ = false;
#endif
}

bool NetworkService::legacyRssiSupported() const { return wlanInfInitialized_; }

ConnectionSnapshot NetworkService::snapshot() {
    ConnectionSnapshot out;
#ifdef __SWITCH__
    if (!initialized_) { out.diagnostic = "nifm:u unavailable"; return out; }
    bool wireless = false;
    if (R_SUCCEEDED(nifmIsWirelessCommunicationEnabled(&wireless))) out.wirelessEnabled = wireless;
    NifmInternetConnectionType type{}; u32 strength = 0; NifmInternetConnectionStatus status{};
    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status))) {
        out.internetConnected = status == NifmInternetConnectionStatus_Connected;
        out.wifiBars = static_cast<int>(strength);
        if (type == NifmInternetConnectionType_WiFi) out.linkType = LinkType::WiFi;
        else if (type == NifmInternetConnectionType_Ethernet) out.linkType = LinkType::Ethernet;
    }
    u32 ip = 0, mask = 0, gateway = 0, dns1 = 0, dns2 = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&ip, &mask, &gateway, &dns1, &dns2))) {
        out.ip.address = ipv4ToString(ip); out.ip.subnet = ipv4ToString(mask); out.ip.gateway = ipv4ToString(gateway);
        out.ip.primaryDns = ipv4ToString(dns1); out.ip.secondaryDns = ipv4ToString(dns2);
    }
    NifmNetworkProfileData profile{};
    if (R_SUCCEEDED(nifmGetCurrentNetworkProfile(&profile))) {
        const auto& wifi = profile.wireless_setting_data;
        const std::size_t len = std::min<std::size_t>(wifi.ssid_len, 0x20);
        out.ssid.assign(wifi.ssid, wifi.ssid + len); out.ip.mtu = profile.ip_setting_data.mtu;
        std::memset(&profile, 0, sizeof(profile));
    }
    if (wlanInfInitialized_) { s32 rssi = 0; if (R_SUCCEEDED(wlaninfGetRSSI(&rssi))) { out.rssiDbm = static_cast<int>(rssi); out.hasRssi = true; } }
    if (out.linkType == LinkType::WiFi) {
        const SignalQuality quality = out.hasRssi ? classifyRssi(out.rssiDbm) : classifyBars(out.wifiBars);
        out.diagnostic = std::string("Wi-Fi signal: ") + toString(quality);
    } else if (out.linkType == LinkType::Ethernet) out.diagnostic = "Wired connection";
    else out.diagnostic = "No active network";
#else
    out.wirelessEnabled = true; out.internetConnected = true; out.linkType = LinkType::WiFi; out.ssid = "Desktop mock";
    out.wifiBars = 3; out.rssiDbm = -52; out.hasRssi = true;
    out.ip = {"192.168.1.50", "255.255.255.0", "192.168.1.1", "1.1.1.1", "1.0.0.1", 1500};
    out.diagnostic = "Desktop mock backend";
#endif
    return out;
}

std::vector<SavedNetwork> NetworkService::savedNetworks() {
    std::vector<SavedNetwork> result;
#ifdef __SWITCH__
    if (!initialized_) return result;
    std::array<NifmBasicProfileWire, 32> profiles{}; s32 total = 0;
    if (R_FAILED(enumerateUserProfiles(profiles, &total))) return result;
    const int count = std::max(0, std::min<int>(total, static_cast<int>(profiles.size())));
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const auto& p = profiles[i];
        const std::size_t nameLen = strnlen(p.networkName, sizeof(p.networkName));
        const std::size_t ssidLen = std::min<std::size_t>(p.ssidLen, sizeof(p.ssid));
        SavedNetwork net; net.name.assign(p.networkName, p.networkName + nameLen); net.ssid.assign(p.ssid, p.ssid + ssidLen);
        net.authentication = authName(p.authentication); net.encryption = encryptionName(p.encryption); result.push_back(std::move(net));
    }
#else
    result.push_back({"Mock profile", "Desktop mock", "WPA2-PSK", "AES"});
#endif
    return result;
}

std::vector<int> NetworkService::allowedWifiChannels() {
    std::vector<int> result;
#ifdef __SWITCH__
    if (!initialized_ || hosversionBefore(6, 0, 0)) return result;
    std::array<u16, 0x40> channels{}; u32 total = 0;
    Service* general = nifmGetServiceSession_GeneralService(); serviceAssumeDomain(general);
    const Result rc = serviceDispatchOut(general, 38, total,
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_Out },
        .buffers = { { channels.data(), channels.size() * sizeof(channels[0]) } },
    );
    if (R_FAILED(rc)) return result;
    const std::size_t count = std::min<std::size_t>(total, channels.size());
    result.reserve(count); for (std::size_t i = 0; i < count; ++i) if (channels[i] != 0) result.push_back(static_cast<int>(channels[i]));
#else
    result = {1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161};
#endif
    return result;
}

} // namespace swifi

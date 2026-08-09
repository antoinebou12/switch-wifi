#include "switch_wifi/ui/main_tabs.hpp"

#include "switch_wifi/bluetooth/bluetooth_service.hpp"
#include "switch_wifi/network/network_service.hpp"
#include "switch_wifi/network/speed_test.hpp"
#include "switch_wifi/ui/signal_shader.hpp"
#include "switch_wifi/ui/sparkline.hpp"
#include "switch_wifi/version.hpp"

#include <borealis/core/thread.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace swifi {
namespace {

std::string fmt(double value, const char* suffix, int precision = 1) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value << suffix;
    return ss.str();
}

brls::DetailCell* cell(brls::Box* box, const std::string& name) {
    auto* c = new brls::DetailCell();
    c->setText(name);
    c->setDetailText("-");
    box->addView(c);
    return c;
}

brls::Label* title(brls::Box* box, const std::string& text) {
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(28.0f);
    l->setMarginTop(18.0f);
    l->setMarginBottom(8.0f);
    box->addView(l);
    return l;
}

brls::Label* subtitle(brls::Box* box, const std::string& text) {
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(17.0f);
    l->setMarginBottom(10.0f);
    box->addView(l);
    return l;
}

float signalNormalized(const ConnectionSnapshot& snapshot) {
    if (snapshot.hasRssi) return static_cast<float>(normalizedSignalWeight(snapshot.rssiDbm));
    if (snapshot.wifiBars >= 0) return std::max(0.0f, std::min(1.0f, snapshot.wifiBars / 3.0f));
    return 0.0f;
}

class DashboardTab : public brls::Box {
  public:
    DashboardTab() : signalHistory_(120) {
        setPadding(20, 28, 20, 28);
        title(this, "Wi-Fi dashboard");
        subtitle(this, "Live connection state with a GPU-backed signal visualization and rolling history.");

        shader_ = new SignalShaderView();
        addView(shader_);

        type_ = cell(this, "Link");
        ssid_ = cell(this, "SSID");
        signal_ = cell(this, "Signal");
        ip_ = cell(this, "IPv4");
        gateway_ = cell(this, "Gateway");
        dns_ = cell(this, "DNS");
        mtu_ = cell(this, "MTU");
        diagnostic_ = cell(this, "Diagnostic");

        title(this, "Signal history");
        subtitle(this, "RSSI where verified; otherwise Horizon's 0-3 signal bars.");
        graph_ = new SparklineView();
        addView(graph_);

        auto* refresh = cell(this, "Refresh sample");
        refresh->setDetailText("Press A");
        refresh->registerClickAction([this](brls::View*) {
            refreshNow();
            return true;
        });
        refreshNow();
    }

    ~DashboardTab() override { service_.shutdown(); }

    void willAppear(bool resetState) override {
        Box::willAppear(resetState);
        refreshNow();
    }

  private:
    void refreshNow() {
        service_.initialize();
        const auto s = service_.snapshot();
        type_->setDetailText(toString(s.linkType));
        ssid_->setDetailText(s.ssid.empty() ? "(hidden/unknown)" : s.ssid);
        signal_->setDetailText(s.hasRssi
            ? std::to_string(s.rssiDbm) + " dBm (" + toString(classifyRssi(s.rssiDbm)) + ")"
            : (s.wifiBars >= 0 ? std::to_string(s.wifiBars) + "/3 bars (" + toString(classifyBars(s.wifiBars)) + ")" : "Unavailable"));
        ip_->setDetailText(s.ip.address);
        gateway_->setDetailText(s.ip.gateway);
        dns_->setDetailText(s.ip.primaryDns + " / " + s.ip.secondaryDns);
        mtu_->setDetailText(s.ip.mtu ? std::to_string(s.ip.mtu) : "-");
        diagnostic_->setDetailText(s.diagnostic);

        shader_->setSignal(signalNormalized(s), s.linkType == LinkType::WiFi && s.internetConnected);
        const float sample = s.hasRssi ? static_cast<float>(s.rssiDbm)
                                       : static_cast<float>(std::max(0, s.wifiBars));
        signalHistory_.push(sample);
        graph_->setSamples(signalHistory_.values());
    }

    NetworkService service_;
    RingHistory<float> signalHistory_;
    SignalShaderView* shader_{};
    SparklineView* graph_{};
    brls::DetailCell *type_{}, *ssid_{}, *signal_{}, *ip_{}, *gateway_{}, *dns_{}, *mtu_{}, *diagnostic_{};
};

class NetworksTab : public brls::Box {
  public:
    NetworksTab() {
        setPadding(20, 28, 20, 28);
        title(this, "Wi-Fi environment");
        subtitle(this, "Verified Horizon diagnostics are separated from experimental scan decoding.");

        auto* note = cell(this, "Nearby active scan");
        note->setDetailText("Experimental: AP field decoding requires hardware fixtures");
        auto* noise = cell(this, "RF noise floor / SNR");
        noise->setDetailText("Unavailable through the verified user-level APIs used here");
        auto* proxy = cell(this, "Channel contention proxy");
        proxy->setDetailText("Core model ready; activates after verified AP channel/RSSI parsing");

        title(this, "Channel reference");
        allowedChannels_ = cell(this, "Allowed channels");
        auto* ch1 = cell(this, "2.4 GHz channel 1");
        ch1->setDetailText(std::to_string(wifiChannelFrequencyMhz(1)) + " MHz");
        auto* ch6 = cell(this, "2.4 GHz channel 6");
        ch6->setDetailText(std::to_string(wifiChannelFrequencyMhz(6)) + " MHz");
        auto* ch11 = cell(this, "2.4 GHz channel 11");
        ch11->setDetailText(std::to_string(wifiChannelFrequencyMhz(11)) + " MHz");
        auto* ch36 = cell(this, "5 GHz channel 36");
        ch36->setDetailText(std::to_string(wifiChannelFrequencyMhz(36)) + " MHz");

        title(this, "Saved profiles");
        subtitle(this, "Basic metadata only; switch-wifi intentionally never fetches saved passphrases.");
        savedBox_ = new brls::Box();
        addView(savedBox_);

        auto* refresh = cell(this, "Refresh profiles");
        refresh->setDetailText("Press A");
        refresh->registerClickAction([this](brls::View*) {
            reloadSavedNetworks();
            return true;
        });
        reloadSavedNetworks();
    }

    ~NetworksTab() override { service_.shutdown(); }

  private:
    void reloadSavedNetworks() {
        service_.initialize();
        const auto channels = service_.allowedWifiChannels();
        if (channels.empty()) {
            allowedChannels_->setDetailText("Unavailable");
        } else {
            std::ostringstream text;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                if (i) text << ", ";
                text << channels[i];
            }
            allowedChannels_->setDetailText(text.str());
        }

        savedBox_->clearViews();
        const auto networks = service_.savedNetworks();
        if (networks.empty()) {
            auto* empty = cell(savedBox_, "No profiles available");
            empty->setDetailText("Launch context or firmware may restrict profile access");
            return;
        }

        for (const auto& n : networks) {
            auto* c = cell(savedBox_, n.ssid.empty() ? n.name : n.ssid);
            c->setDetailText(n.authentication + " / " + n.encryption);
        }
    }

    NetworkService service_;
    brls::Box* savedBox_{};
    brls::DetailCell* allowedChannels_{};
};

class BenchmarkTab : public brls::Box {
  public:
    BenchmarkTab() : dlHistory_(40), ulHistory_(40), latencyHistory_(40) {
        setPadding(20, 28, 20, 28);
        title(this, "Wi-Fi benchmark");
        subtitle(this, "End-to-end HTTPS benchmark: repeated request latency, jitter, download and upload.");

        stage_ = cell(this, "State");
        latency_ = cell(this, "Median latency");
        minimumLatency_ = cell(this, "Minimum latency");
        averageLatency_ = cell(this, "Average latency");
        p95Latency_ = cell(this, "P95 latency");
        jitter_ = cell(this, "Jitter");
        probes_ = cell(this, "Latency probes");
        download_ = cell(this, "Download");
        upload_ = cell(this, "Upload");
        endpoint_ = cell(this, "Endpoint");
        endpoint_->setDetailText("speed.cloudflare.com");

        auto* run = cell(this, "Run benchmark");
        run->setDetailText("Press A");
        run->registerClickAction([this](brls::View*) {
            startTest();
            return true;
        });

        auto* cancel = cell(this, "Cancel benchmark");
        cancel->setDetailText("Press A");
        cancel->registerClickAction([this](brls::View*) {
            engine_.cancel();
            return true;
        });

        title(this, "Download history (Mbps)");
        downloadGraph_ = new SparklineView();
        addView(downloadGraph_);

        title(this, "Upload history (Mbps)");
        uploadGraph_ = new SparklineView();
        addView(uploadGraph_);

        title(this, "Median latency history (ms)");
        latencyGraph_ = new SparklineView();
        addView(latencyGraph_);
    }

    ~BenchmarkTab() override { engine_.cancel(); }

  private:
    void updateResult(const SpeedSample& result) {
        if (result.latencyMs > 0) latency_->setDetailText(fmt(result.latencyMs, " ms"));
        if (result.latencyMinMs > 0) minimumLatency_->setDetailText(fmt(result.latencyMinMs, " ms"));
        if (result.latencyAverageMs > 0) averageLatency_->setDetailText(fmt(result.latencyAverageMs, " ms"));
        if (result.latencyP95Ms > 0) p95Latency_->setDetailText(fmt(result.latencyP95Ms, " ms"));
        if (result.jitterMs >= 0 && result.latencyProbeCount > 1) jitter_->setDetailText(fmt(result.jitterMs, " ms"));
        if (result.latencyProbeCount > 0) probes_->setDetailText(std::to_string(result.latencyProbeCount));
        if (result.downloadMbps > 0) download_->setDetailText(fmt(result.downloadMbps, " Mbps"));
        if (result.uploadMbps > 0) upload_->setDetailText(fmt(result.uploadMbps, " Mbps"));
    }

    void startTest() {
        if (engine_.isRunning()) return;
        stage_->setDetailText("Starting...");
        ptrLock();
        brls::async([this] {
            try {
                const auto result = engine_.run([this](const SpeedTestProgress& p) {
                    brls::sync([this, p] {
                        stage_->setDetailText(p.message);
                        updateResult(p.result);
                    });
                });
                brls::sync([this, result] {
                    updateResult(result);
                    if (result.cancelled) {
                        stage_->setDetailText("Cancelled");
                    } else if (result.completed) {
                        stage_->setDetailText("Complete");
                        dlHistory_.push(static_cast<float>(result.downloadMbps));
                        ulHistory_.push(static_cast<float>(result.uploadMbps));
                        latencyHistory_.push(static_cast<float>(result.latencyMs));
                        downloadGraph_->setSamples(dlHistory_.values());
                        uploadGraph_->setSamples(ulHistory_.values());
                        latencyGraph_->setSamples(latencyHistory_.values());
                    }
                    ptrUnlock();
                });
            } catch (const std::exception& e) {
                const std::string error = e.what();
                brls::sync([this, error] {
                    stage_->setDetailText("Failed: " + error);
                    ptrUnlock();
                });
            }
        });
    }

    SpeedTestEngine engine_;
    RingHistory<float> dlHistory_;
    RingHistory<float> ulHistory_;
    RingHistory<float> latencyHistory_;
    SparklineView* downloadGraph_{};
    SparklineView* uploadGraph_{};
    SparklineView* latencyGraph_{};
    brls::DetailCell *stage_{}, *latency_{}, *minimumLatency_{}, *averageLatency_{}, *p95Latency_{}, *jitter_{}, *probes_{},
        *download_{}, *upload_{}, *endpoint_{};
};

class BluetoothTab : public brls::Box {
  public:
    BluetoothTab() {
        setPadding(20, 28, 20, 28);
        title(this, "Bluetooth diagnostics");
        subtitle(this, "Read-only BTM state plus an explicit, bounded passive BLE scan.");
        available_ = cell(this, "BTM service");
        state_ = cell(this, "State code");
        connected_ = cell(this, "Connected devices");
        known_ = cell(this, "Known devices");
        diagnostic_ = cell(this, "Mode");

        auto* refresh = cell(this, "Refresh");
        refresh->setDetailText("Press A");
        refresh->registerClickAction([this](brls::View*) {
            refreshNow();
            return true;
        });

        title(this, "Passive BLE discovery");
        bleStatus_ = cell(this, "Scan state");
        bleStatus_->setDetailText("Idle");
        bleResults_ = new brls::Box();
        addView(bleResults_);

        auto* scan = cell(this, "Scan for 1.5 seconds");
        scan->setDetailText("Press A");
        scan->registerClickAction([this](brls::View*) {
            startPassiveScan();
            return true;
        });
        refreshNow();
    }

  private:
    void refreshNow() {
        const auto s = service_.snapshot();
        available_->setDetailText(s.serviceAvailable ? "Available" : "Unavailable");
        state_->setDetailText(s.state >= 0 ? std::to_string(s.state) : "-");
        connected_->setDetailText(s.connectedDevices >= 0 ? std::to_string(s.connectedDevices) : "Unavailable");
        known_->setDetailText(s.knownDevices >= 0 ? std::to_string(s.knownDevices) : "Unavailable");
        diagnostic_->setDetailText(s.diagnostic);
    }

    void startPassiveScan() {
        bleStatus_->setDetailText("Scanning...");
        ptrLock();
        brls::async([this] {
            const auto result = service_.passiveScan(1500);
            brls::sync([this, result] {
                bleResults_->clearViews();
                for (const auto& device : result.devices) {
                    auto* entry = cell(bleResults_, device.name.empty() ? "BLE device" : device.name);
                    entry->setDetailText(device.address);
                }
                bleStatus_->setDetailText(result.diagnostic + " (" +
                                          std::to_string(result.devices.size()) + " unique)");
                ptrUnlock();
            });
        });
    }

    BluetoothService service_;
    brls::Box* bleResults_{};
    brls::DetailCell *available_{}, *state_{}, *connected_{}, *known_{}, *diagnostic_{}, *bleStatus_{};
};

class AboutTab : public brls::Box {
  public:
    AboutTab() {
        setPadding(20, 28, 20, 28);
        title(this, std::string("switch-wifi ") + build::version);
        subtitle(this, "Nintendo Switch network diagnostics built for modern Atmosphere/libnx homebrew.");
        auto* purpose = cell(this, "Purpose");
        purpose->setDetailText("Read-only Switch Wi-Fi/network benchmarking and diagnostics");
        auto* stack = cell(this, "Stack");
        stack->setDetailText("libnx + Borealis + libcurl + NanoVG");
        auto* compatibility = cell(this, "Compatibility target");
        compatibility->setDetailText(std::string("Atmosphere ") + build::atmosphereTarget +
                                     " / HOS " + build::hosTarget +
                                     "; released libnx " + build::libnxBaseline);
        auto* privacy = cell(this, "Privacy");
        privacy->setDetailText("Never displays saved Wi-Fi passphrases");
        auto* noise = cell(this, "Noise metric");
        noise->setDetailText("No fake RF noise floor; derived contention is labeled as a proxy");

        title(this, "Diagnostics export");
        exportStatus_ = cell(this, "Report");
        exportStatus_->setDetailText("Not exported");
        auto* exportButton = cell(this, "Export text report");
        exportButton->setDetailText("Press A");
        exportButton->registerClickAction([this](brls::View*) {
            exportReport();
            return true;
        });
    }

  private:
    void exportReport() {
        NetworkService network;
        network.initialize();
        const auto connection = network.snapshot();
        const auto saved = network.savedNetworks();
        network.shutdown();

        BluetoothService bluetooth;
        const auto bt = bluetooth.snapshot();

        const std::string home = brls::Application::getPlatform()->getHomeDirectory("switch-wifi");
        const std::string path = home + "/diagnostics.txt";
        std::ofstream report(path, std::ios::trunc);
        if (!report) {
            exportStatus_->setDetailText("Failed to open " + path);
            return;
        }

        report << "switch-wifi diagnostics\n"
               << "=======================\n"
               << "app_version=" << build::version << "\n"
               << "compatibility_target=Atmosphere " << build::atmosphereTarget
               << " / HOS " << build::hosTarget
               << " / libnx " << build::libnxBaseline << "\n"
               << "link=" << toString(connection.linkType) << "\n"
               << "internet_connected=" << (connection.internetConnected ? "yes" : "no") << "\n"
               << "wireless_enabled=" << (connection.wirelessEnabled ? "yes" : "no") << "\n"
               << "ssid=" << connection.ssid << "\n"
               << "wifi_bars=" << connection.wifiBars << "\n";
        if (connection.hasRssi) report << "rssi_dbm=" << connection.rssiDbm << "\n";
        report << "ipv4=" << connection.ip.address << "\n"
               << "subnet=" << connection.ip.subnet << "\n"
               << "gateway=" << connection.ip.gateway << "\n"
               << "dns_primary=" << connection.ip.primaryDns << "\n"
               << "dns_secondary=" << connection.ip.secondaryDns << "\n"
               << "mtu=" << connection.ip.mtu << "\n"
               << "network_diagnostic=" << connection.diagnostic << "\n\n"
               << "saved_profiles=" << saved.size() << "\n";
        for (const auto& item : saved) {
            report << "profile=" << (item.ssid.empty() ? item.name : item.ssid)
                   << " | " << item.authentication << " | " << item.encryption << "\n";
        }
        report << "\nbluetooth_service=" << (bt.serviceAvailable ? "available" : "unavailable") << "\n"
               << "bluetooth_state=" << bt.state << "\n"
               << "bluetooth_connected=" << bt.connectedDevices << "\n"
               << "bluetooth_known=" << bt.knownDevices << "\n"
               << "bluetooth_diagnostic=" << bt.diagnostic << "\n\n"
               << "privacy_note=Wi-Fi passphrases are intentionally excluded.\n"
               << "rf_note=No RF noise-floor or SNR value is fabricated.\n";
        report.close();
        exportStatus_->setDetailText(path);
    }

    brls::DetailCell* exportStatus_{};
};

} // namespace

MainTabs::MainTabs() {
    addTab("Dashboard", [] { return new DashboardTab(); });
    addTab("Networks", [] { return new NetworksTab(); });
    addTab("Benchmark", [] { return new BenchmarkTab(); });
    addTab("Bluetooth", [] { return new BluetoothTab(); });
    addSeparator();
    addTab("About", [] { return new AboutTab(); });
}

} // namespace swifi

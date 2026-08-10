#include "switch_wifi/ui/main_tabs.hpp"

#include "switch_wifi/bluetooth/bluetooth_service.hpp"
#include "switch_wifi/network/network_service.hpp"
#include "switch_wifi/network/speed_test.hpp"
#include "switch_wifi/ui/signal_shader.hpp"
#include "switch_wifi/ui/sparkline.hpp"
#include "switch_wifi/version.hpp"

#include <borealis/core/thread.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace swifi {
namespace {

constexpr float kSectionGap = 30.0f;

std::string fmt(double value, const char* suffix, int precision = 1) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value << suffix;
    return ss.str();
}

class ScrollableTab : public brls::Box {
  public:
    ScrollableTab() : brls::Box(brls::Axis::ROW) {
        setAlignItems(brls::AlignItems::STRETCH);

        scroll_ = new brls::ScrollingFrame();
        scroll_->setGrow(1.0f);
        scroll_->setAlignItems(brls::AlignItems::STRETCH);

        content_ = new brls::Box(brls::Axis::COLUMN);
        content_->setAlignItems(brls::AlignItems::STRETCH);
        content_->setPadding(24.0f, 34.0f, 44.0f, 34.0f);
        scroll_->setContentView(content_);
        addView(scroll_);
    }

  protected:
    brls::Box* content() const { return content_; }

  private:
    brls::ScrollingFrame* scroll_{};
    brls::Box* content_{};
};

brls::Header* section(brls::Box* box, const std::string& name,
                      const std::string& detail = {}, bool first = false) {
    auto* header = new brls::Header();
    header->setTitle(name);
    if (!detail.empty()) header->setSubtitle(detail);
    if (!first) header->setMarginTop(kSectionGap);
    box->addView(header);
    return header;
}

brls::Label* paragraph(brls::Box* box, const std::string& text, float bottom = 14.0f) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(16.0f);
    label->setLineHeight(1.25f);
    label->setWidthPercentage(100.0f);
    label->setMarginTop(10.0f);
    label->setMarginBottom(bottom);
    box->addView(label);
    return label;
}

brls::Label* statusCard(brls::Box* box, const std::string& heading, const std::string& text) {
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setAlignItems(brls::AlignItems::STRETCH);
    card->setWidthPercentage(100.0f);
    card->setPadding(14.0f, 18.0f, 14.0f, 18.0f);
    card->setMarginTop(12.0f);
    card->setMarginBottom(8.0f);
    card->setBackgroundColor(brls::Application::getTheme()["brls/sidebar/background"]);
    card->setCornerRadius(10.0f);

    auto* title = new brls::Label();
    title->setText(heading);
    title->setFontSize(18.0f);
    title->setWidthPercentage(100.0f);
    title->setMarginBottom(5.0f);
    card->addView(title);

    auto* body = new brls::Label();
    body->setText(text);
    body->setFontSize(15.0f);
    body->setLineHeight(1.25f);
    body->setWidthPercentage(100.0f);
    card->addView(body);

    box->addView(card);
    return body;
}

brls::DetailCell* cell(brls::Box* box, const std::string& name,
                       const std::string& value = "-") {
    auto* result = new brls::DetailCell();
    result->setText(name);
    result->setDetailText(value);
    box->addView(result);
    return result;
}

brls::DetailCell* action(brls::Box* box, const std::string& name,
                         const brls::ActionListener& listener) {
    auto* result = cell(box, name, "Press A");
    result->registerClickAction(listener);
    return result;
}

float signalNormalized(const ConnectionSnapshot& snapshot) {
    if (snapshot.hasRssi) return static_cast<float>(normalizedSignalWeight(snapshot.rssiDbm));
    if (snapshot.wifiBars >= 0) return std::max(0.0f, std::min(1.0f, snapshot.wifiBars / 3.0f));
    return 0.0f;
}

class DashboardTab : public ScrollableTab {
  public:
    DashboardTab() : signalHistory_(120) {
        auto* page = content();
        section(page, "Connection", "LIVE", true);
        paragraph(page, "Current network details from Horizon OS. Values refresh when this tab opens.");

        shader_ = new SignalShaderView();
        shader_->setMarginBottom(12.0f);
        page->addView(shader_);

        type_ = cell(page, "Link type");
        ssid_ = cell(page, "Network");
        signal_ = cell(page, "Signal");
        ip_ = cell(page, "IPv4 address");
        gateway_ = cell(page, "Router");
        dns_ = cell(page, "DNS servers");
        mtu_ = cell(page, "MTU");
        diagnostic_ = statusCard(page, "Connection note", "Waiting for a network sample...");

        action(page, "Refresh connection", [this](brls::View*) {
            refreshNow();
            return true;
        });

        section(page, "Signal history", "RECENT SAMPLES");
        paragraph(page, "Uses verified RSSI when available; otherwise it shows the Switch signal-bar reading.");
        graph_ = new SparklineView();
        page->addView(graph_);

        refreshNow();
    }

    ~DashboardTab() override { service_.shutdown(); }

    void willAppear(bool resetState) override {
        ScrollableTab::willAppear(resetState);
        refreshNow();
    }

  private:
    void refreshNow() {
        service_.initialize();
        const auto snapshot = service_.snapshot();
        type_->setDetailText(toString(snapshot.linkType));
        ssid_->setDetailText(snapshot.ssid.empty() ? "Hidden or unknown" : snapshot.ssid);
        signal_->setDetailText(snapshot.hasRssi
            ? std::to_string(snapshot.rssiDbm) + " dBm - " + toString(classifyRssi(snapshot.rssiDbm))
            : (snapshot.wifiBars >= 0
                ? std::to_string(snapshot.wifiBars) + " / 3 bars - " + toString(classifyBars(snapshot.wifiBars))
                : "Unavailable"));
        ip_->setDetailText(snapshot.ip.address);
        gateway_->setDetailText(snapshot.ip.gateway);
        dns_->setDetailText(snapshot.ip.primaryDns + "  /  " + snapshot.ip.secondaryDns);
        mtu_->setDetailText(snapshot.ip.mtu ? std::to_string(snapshot.ip.mtu) : "Unavailable");
        diagnostic_->setText(snapshot.diagnostic.empty() ? "No additional diagnostic details." : snapshot.diagnostic);

        shader_->setSignal(signalNormalized(snapshot),
                           snapshot.linkType == LinkType::WiFi && snapshot.internetConnected);
        const float sample = snapshot.hasRssi ? static_cast<float>(snapshot.rssiDbm)
                                              : static_cast<float>(std::max(0, snapshot.wifiBars));
        signalHistory_.push(sample);
        graph_->setSamples(signalHistory_.values());
    }

    NetworkService service_;
    RingHistory<float> signalHistory_;
    SignalShaderView* shader_{};
    SparklineView* graph_{};
    brls::Label* diagnostic_{};
    brls::DetailCell *type_{}, *ssid_{}, *signal_{}, *ip_{}, *gateway_{}, *dns_{}, *mtu_{};
};

class NetworksTab : public ScrollableTab {
  public:
    NetworksTab() {
        auto* page = content();
        section(page, "Wi-Fi environment", "READ ONLY", true);
        paragraph(page, "Useful radio and saved-profile information exposed by the verified user-level Switch APIs.");
        statusCard(page, "Scan availability",
                   "Nearby access-point decoding and RF noise-floor values are hidden until they can be verified on hardware. The app never invents signal data.");

        section(page, "Channel reference");
        allowedChannels_ = statusCard(page, "Allowed channels", "Loading the channel list...");
        cell(page, "2.4 GHz channel 1", std::to_string(wifiChannelFrequencyMhz(1)) + " MHz");
        cell(page, "2.4 GHz channel 6", std::to_string(wifiChannelFrequencyMhz(6)) + " MHz");
        cell(page, "2.4 GHz channel 11", std::to_string(wifiChannelFrequencyMhz(11)) + " MHz");
        cell(page, "5 GHz channel 36", std::to_string(wifiChannelFrequencyMhz(36)) + " MHz");

        section(page, "Saved profiles", "NO PASSWORDS");
        paragraph(page, "Only profile names and security types are displayed. Saved Wi-Fi passwords are never requested.");
        action(page, "Refresh profiles", [this](brls::View*) {
            reloadSavedNetworks();
            return true;
        });

        savedBox_ = new brls::Box(brls::Axis::COLUMN);
        savedBox_->setAlignItems(brls::AlignItems::STRETCH);
        page->addView(savedBox_);
        reloadSavedNetworks();
    }

    ~NetworksTab() override { service_.shutdown(); }

  private:
    void reloadSavedNetworks() {
        service_.initialize();
        const auto channels = service_.allowedWifiChannels();
        if (channels.empty()) {
            allowedChannels_->setText("The system did not expose an allowed-channel list in this launch context.");
        } else {
            std::ostringstream text;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                if (i) text << ", ";
                text << channels[i];
            }
            allowedChannels_->setText(text.str());
        }

        savedBox_->clearViews();
        const auto networks = service_.savedNetworks();
        if (networks.empty()) {
            paragraph(savedBox_, "No saved profiles are available. Applet launch mode or firmware permissions may restrict access.", 0.0f);
            return;
        }

        for (const auto& network : networks) {
            auto* entry = cell(savedBox_, network.ssid.empty() ? network.name : network.ssid);
            entry->setDetailText(network.authentication + " / " + network.encryption);
        }
    }

    NetworkService service_;
    brls::Box* savedBox_{};
    brls::Label* allowedChannels_{};
};

class BenchmarkTab : public ScrollableTab {
  public:
    BenchmarkTab() : dlHistory_(40), ulHistory_(40), latencyHistory_(40) {
        auto* page = content();
        section(page, "Internet benchmark", "HTTPS", true);
        paragraph(page, "Measures latency, jitter, download, and upload through a real encrypted connection.");

        stage_ = statusCard(page, "Benchmark status", "Ready to run.");
        action(page, "Run benchmark", [this](brls::View*) {
            startTest();
            return true;
        });
        action(page, "Cancel benchmark", [this](brls::View*) {
            engine_.cancel();
            return true;
        });

        section(page, "Results", "LATEST RUN");
        endpoint_ = cell(page, "Endpoint", "speed.cloudflare.com");
        latency_ = cell(page, "Median latency");
        minimumLatency_ = cell(page, "Minimum latency");
        averageLatency_ = cell(page, "Average latency");
        p95Latency_ = cell(page, "P95 latency");
        jitter_ = cell(page, "Jitter");
        probes_ = cell(page, "Latency probes");
        download_ = cell(page, "Download speed");
        upload_ = cell(page, "Upload speed");

        section(page, "Download history", "Mbps");
        downloadGraph_ = new SparklineView();
        page->addView(downloadGraph_);

        section(page, "Upload history", "Mbps");
        uploadGraph_ = new SparklineView();
        page->addView(uploadGraph_);

        section(page, "Latency history", "ms");
        latencyGraph_ = new SparklineView();
        page->addView(latencyGraph_);
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
        stage_->setText("Starting benchmark...");
        ptrLock();
        brls::async([this] {
            try {
                const auto result = engine_.run([this](const SpeedTestProgress& progress) {
                    brls::sync([this, progress] {
                        stage_->setText(progress.message);
                        updateResult(progress.result);
                    });
                });
                brls::sync([this, result] {
                    updateResult(result);
                    if (result.cancelled) {
                        stage_->setText("Benchmark cancelled.");
                    } else if (result.completed) {
                        stage_->setText("Benchmark complete.");
                        dlHistory_.push(static_cast<float>(result.downloadMbps));
                        ulHistory_.push(static_cast<float>(result.uploadMbps));
                        latencyHistory_.push(static_cast<float>(result.latencyMs));
                        downloadGraph_->setSamples(dlHistory_.values());
                        uploadGraph_->setSamples(ulHistory_.values());
                        latencyGraph_->setSamples(latencyHistory_.values());
                    }
                    ptrUnlock();
                });
            } catch (const std::exception& error) {
                const std::string message = error.what();
                brls::sync([this, message] {
                    stage_->setText("Benchmark failed: " + message);
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
    brls::Label* stage_{};
    brls::DetailCell *latency_{}, *minimumLatency_{}, *averageLatency_{}, *p95Latency_{}, *jitter_{}, *probes_{},
        *download_{}, *upload_{}, *endpoint_{};
};

class BluetoothTab : public ScrollableTab {
  public:
    BluetoothTab() {
        auto* page = content();
        section(page, "Bluetooth", "READ ONLY", true);
        paragraph(page, "Shows the current BTM service state and performs a short passive BLE discovery scan.");

        available_ = cell(page, "BTM service");
        state_ = cell(page, "State code");
        connected_ = cell(page, "Connected devices");
        known_ = cell(page, "Known devices");
        diagnostic_ = statusCard(page, "Bluetooth note", "Waiting for a service sample...");
        action(page, "Refresh Bluetooth", [this](brls::View*) {
            refreshNow();
            return true;
        });

        section(page, "Passive BLE discovery", "1.5 SECONDS");
        bleStatus_ = statusCard(page, "Scan status", "Ready to scan.");
        action(page, "Start BLE scan", [this](brls::View*) {
            startPassiveScan();
            return true;
        });

        bleResults_ = new brls::Box(brls::Axis::COLUMN);
        bleResults_->setAlignItems(brls::AlignItems::STRETCH);
        page->addView(bleResults_);
        refreshNow();
    }

  private:
    void refreshNow() {
        const auto snapshot = service_.snapshot();
        available_->setDetailText(snapshot.serviceAvailable ? "Available" : "Unavailable");
        state_->setDetailText(snapshot.state >= 0 ? std::to_string(snapshot.state) : "Unavailable");
        connected_->setDetailText(snapshot.connectedDevices >= 0 ? std::to_string(snapshot.connectedDevices) : "Unavailable");
        known_->setDetailText(snapshot.knownDevices >= 0 ? std::to_string(snapshot.knownDevices) : "Unavailable");
        diagnostic_->setText(snapshot.diagnostic.empty() ? "No additional diagnostic details." : snapshot.diagnostic);
    }

    void startPassiveScan() {
        bleStatus_->setText("Scanning for nearby BLE advertisements...");
        ptrLock();
        brls::async([this] {
            const auto result = service_.passiveScan(1500);
            brls::sync([this, result] {
                bleResults_->clearViews();
                for (const auto& device : result.devices) {
                    auto* entry = cell(bleResults_, device.name.empty() ? "Unnamed BLE device" : device.name);
                    entry->setDetailText(device.address);
                }
                bleStatus_->setText(result.diagnostic + " Found " +
                                    std::to_string(result.devices.size()) + " unique device(s).");
                ptrUnlock();
            });
        });
    }

    BluetoothService service_;
    brls::Box* bleResults_{};
    brls::Label *diagnostic_{}, *bleStatus_{};
    brls::DetailCell *available_{}, *state_{}, *connected_{}, *known_{};
};

class AboutTab : public ScrollableTab {
  public:
    AboutTab() {
        auto* page = content();
        section(page, std::string("switch-wifi ") + build::version, "SYSTEM TOOL", true);
        paragraph(page, "A focused Nintendo Switch network diagnostics and benchmark utility.");

        statusCard(page, "Privacy first",
                   "Saved Wi-Fi passwords are never requested, displayed, or written to diagnostics reports.");
        statusCard(page, "Honest measurements",
                   "Unsupported RF noise and SNR values stay unavailable instead of being estimated or fabricated.");

        section(page, "Build information");
        cell(page, "Application stack", "libnx / Borealis / libcurl");
        cell(page, "Atmosphere target", build::atmosphereTarget);
        cell(page, "Horizon OS target", build::hosTarget);
        cell(page, "libnx baseline", build::libnxBaseline);

        section(page, "Diagnostics report");
        paragraph(page, "Exports connection and Bluetooth status to the SD card as plain text. Wi-Fi passwords are excluded.");
        exportStatus_ = statusCard(page, "Export status", "No report has been exported yet.");
        action(page, "Export report", [this](brls::View*) {
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

#ifdef __SWITCH__
        if (R_FAILED(fsdevMountSdmc())) {
            exportStatus_->setText("Could not mount the SD card.");
            return;
        }
        const std::string path = "sdmc:/switch-wifi-diagnostics.txt";
#else
        const std::string path = "switch-wifi-diagnostics.txt";
#endif
        std::ofstream report(path, std::ios::trunc);
        if (!report) {
            exportStatus_->setText("Could not open " + path);
#ifdef __SWITCH__
            fsdevUnmountDevice("sdmc");
#endif
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
#ifdef __SWITCH__
        fsdevUnmountDevice("sdmc");
#endif
        exportStatus_->setText("Report saved to " + path);
    }

    brls::Label* exportStatus_{};
};

} // namespace

MainTabs::MainTabs() {
    addTab("Connection", [] { return new DashboardTab(); });
    addTab("Wi-Fi", [] { return new NetworksTab(); });
    addTab("Benchmark", [] { return new BenchmarkTab(); });
    addTab("Bluetooth", [] { return new BluetoothTab(); });
    addSeparator();
    addTab("About", [] { return new AboutTab(); });
}

} // namespace swifi

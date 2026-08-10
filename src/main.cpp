#include "switch_wifi/ui/main_tabs.hpp"

#include <borealis.hpp>
#include <curl/curl.h>
#include <cstdlib>

#ifdef __SWITCH__
#include <switch.h>
#endif

int main() {
#ifdef __SWITCH__
    // Borealis' Switch wrapper initializes sockets/NIFM before main(). Do not
    // initialize the socket service twice. Only request the WLAN scheduler hint.
    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);
#endif

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return EXIT_FAILURE;

    brls::Logger::setLogLevel(brls::LogLevel::LOG_INFO);
    if (!brls::Application::init()) {
        curl_global_cleanup();
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("switch-wifi");
    brls::Application::setGlobalQuit(false);
    brls::Application::pushActivity(new brls::Activity(new swifi::MainTabs()));

    while (brls::Application::mainLoop()) {}

    curl_global_cleanup();
    return EXIT_SUCCESS;
}

#ifndef WPA_CONTROLLER_H
#define WPA_CONTROLLER_H

#include <string>
#include <vector>
#include "wpa_ctrl.h"

class WPAController {
public:
    WPAController();
    ~WPAController();

    // Initialize connection to wpa_supplicant control socket
    bool init(const std::string& interface = "wlan0");
    
    // Trigger scan and print results to stdout
    void scan();
    
    // Configure a new network and connect to it
    bool select(const std::string& ssid, const std::string& password);

private:
    struct wpa_ctrl* ctrl_conn;
    
    bool sendCommand(const std::string& command, std::string& response, bool removeNewLineSymbols = true);
    void close_connection();
    std::string getActiveSSID();
};

#endif
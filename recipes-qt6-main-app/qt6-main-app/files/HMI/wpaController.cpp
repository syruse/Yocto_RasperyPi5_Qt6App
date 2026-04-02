#include "wpaController.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <charconv> // std::from_chars

namespace {
int safeStoi(const std::string& str, int defaultValue = -100) {
    int value = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    
    if (ec == std::errc()) {
        return value;
    }
    return defaultValue; 
}

int dbmToQuality(int dbm) {
    if (dbm <= -100)
        return 0;
    if (dbm >= -50)
        return 100;
    
    // interpolation between -100 (0%) and -50 (100%)
    return 2 * (dbm + 100);
}

void removeQuaotes(std::string& ssid) {
    if (ssid.size() >= 2 && ssid.front() == '"' && ssid.back() == '"') {
        ssid = ssid.substr(1, ssid.size() - 2);
    }
}

/**
 * Converts frequency in MHz to human-readable string (2.4GHz or 5GHz).
 */
std::string formatFrequency(const std::string& freqStr) {
    int freq = safeStoi(freqStr, 0);

    // 2.4 GHz band: usually channels 1-14 (2412 - 2484 MHz)
    if (freq >= 2400 && freq < 2500) {
        return "2.4GHz";
    }
    
    // 5 GHz band: usually channels 36-165 (5180 - 5825 MHz)
    if (freq >= 5000 && freq < 6000) {
        return "5GHz";
    }

    // 6 GHz band (Wi-Fi 6E), if supported by your hardware
    if (freq >= 6000 && freq < 7125) {
        return "6GHz";
    }

    return "Unknown";
}
}


WPAController::WPAController() : ctrl_conn(nullptr) {}

WPAController::~WPAController() {
    close_connection();
}

bool WPAController::init(const std::string& interface) {
    close_connection();
    std::string path = "/var/run/wpa_supplicant/" + interface;
    ctrl_conn = wpa_ctrl_open(path.c_str());
    
    if (!ctrl_conn) {
        std::cerr << "Error: failed to open control socket for " << interface << std::endl;
        return false;
    }
    return true;
}

bool WPAController::sendCommand(const std::string& command, std::string& response, bool removeNewLineSymbols) {
    if (!ctrl_conn) return false;

    std::cout << "sendCommand:" << command << std::endl;

    char buf[4096];
    size_t len = sizeof(buf) - 1;
    
    if (wpa_ctrl_request(ctrl_conn, command.c_str(), command.length(), buf, &len, nullptr) < 0) {
        return false;
    }
    
    buf[len] = '\0';
    response = buf;

    if (removeNewLineSymbols) {
        // Remove trailing newline character
        response.erase(std::remove(response.begin(), response.end(), '\n'), response.end());
    }

    return true;
}

void WPAController::scan() {
    if (!ctrl_conn) return;

    std::string res;
    std::cout << "Starting scan..." << std::endl;
    
    // Trigger the scan
    if (!sendCommand("SCAN", res) || res != "OK") {
        std::cerr << "Scan request failed" << std::endl;
        return;
    }

    // Wait for the scan to complete and results to be available
    sleep(5); 

    // Get scan results using the helper method
    if (sendCommand("SCAN_RESULTS", res, false)) {
        std::stringstream ss(res);
        std::string line;
        
        // Skip header: bssid / frequency / signal level / flags / ssid
        if (!std::getline(ss, line)) return;

        std::string connectedSSID = getActiveSSID();
        removeQuaotes(connectedSSID);
        
        while (std::getline(ss, line)) {
            std::stringstream lineStream(line);
            std::string bssid, freq, signal, flags, ssid;
            
            // Fields are tab-separated
            if (std::getline(lineStream, bssid, '\t') &&
                std::getline(lineStream, freq, '\t') &&
                std::getline(lineStream, signal, '\t') &&
                std::getline(lineStream, flags, '\t') &&
                std::getline(lineStream, ssid)) {
                
                if (!ssid.empty()) {
                    removeQuaotes(ssid);
                    bool isConnected = ssid == connectedSSID;
                    int signalInt = safeStoi(signal, -100); 
                    int quality = dbmToQuality(signalInt);
                    std::cout << "Network found: " << ssid << " [Quality: " << quality << "]" 
                    << " [Connected: " << isConnected << "]" << " [Frequency: " << formatFrequency(freq) << "]" << std::endl;
                }
            }
        }
    }
    else {
        std::cerr << "Couldn't get result" << std::endl;
    }
}

std::string WPAController::getActiveSSID() {
    if (!ctrl_conn) return "";

    std::string response;
    bool isCompleted = false;
    std::string ssid{};

    if (sendCommand("STATUS", response, false)) {
        std::stringstream ss(response);
        std::string line;
        
        // Parse the status output line by line
        while (std::getline(ss, line)) {
            // we exclude ASSOCIATING and 4WAY_HANDSHAKE statuses
            // we need really connected network
            if (line == "wpa_state=COMPLETED") {
                isCompleted = true;
            }
            // Standard wpa_supplicant status output uses "ssid=NetworkName"
            if (line.compare(0, 5, "ssid=") == 0) {
                ssid = line.substr(5); // cut "ssid="
            }
        }
    }
    
    return isCompleted ? ssid : ""; // Return empty string if not connected or command failed
}

bool WPAController::select(const std::string& ssid, const std::string& password) {
    std::string res;

    // 1. Add new network block
    if (!sendCommand("ADD_NETWORK", res) || res == "FAIL") {
        std::cerr << "Failed to add network" << std::endl;
        return false;
    }
    std::string netId = res;

    // 2. Set SSID (must be in quotes)
    if (!sendCommand("SET_NETWORK " + netId + " ssid \"" + ssid + "\"", res) || res != "OK") {
        std::cerr << "Failed to set SSID" << std::endl;
        return false;
    }

    // 3. Set Password (PSK, must be in quotes)
    if (!sendCommand("SET_NETWORK " + netId + " psk \"" + password + "\"", res) || res != "OK") {
        std::cerr << "Failed to set password" << std::endl;
        return false;
    }

    // 4. Select and enable this network
    if (!sendCommand("SELECT_NETWORK " + netId, res) || res != "OK") {
        std::cerr << "Failed to select network" << std::endl;
        return false;
    }

    // 5. Make changes persistent
    sendCommand("SAVE_CONFIG", res);

    std::cout << "Successfully connected to " << ssid << " (ID:" << netId << ")" << std::endl;
    return true;
}

void WPAController::close_connection() {
    if (ctrl_conn) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = nullptr;
    }
}
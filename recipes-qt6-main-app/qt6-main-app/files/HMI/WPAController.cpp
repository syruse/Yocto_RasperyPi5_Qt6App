#include "WPAController.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <charconv> // std::from_chars

namespace {
template <typename... Args>
void log_info(Args... args) {
  ((std::cout << " " << args), ...) << std::endl;
}
template <typename... Args>
void log_err(Args... args) {
  ((std::cerr << " " << args), ...) << std::endl;
}
#ifdef NDEBUG
#define log_debug(...) ((void)0)
#else
template <typename... Args>
void log_debug(Args... args) {
  log_info(args...);
}
#endif

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


WPAController::WPAController() : m_ctrl_conn(nullptr) {}

WPAController::~WPAController() {
    close_connection();
}

bool WPAController::init(const std::string& interface) {
    close_connection();

    m_stop_thread = false;
    std::string path = "/var/run/wpa_supplicant/" + interface;
    m_ctrl_conn = wpa_ctrl_open(path.c_str());
    
    if (!m_ctrl_conn) {
        log_err("Error: failed to open control socket for ", interface);
        return false;
    }

    // open the socket for monitoring the events
    m_monitor_conn = wpa_ctrl_open(path.c_str());
    if (!m_monitor_conn) {
        wpa_ctrl_close(m_ctrl_conn);
        log_err("Error: failed to open monitor socket for ", interface);
        return false;
    }
    // Attach the monitor socket to receive async events
    if (wpa_ctrl_attach(m_monitor_conn) != 0) {
        log_err("Error: failed to attach monitor");
        wpa_ctrl_close(m_monitor_conn);
        wpa_ctrl_close(m_ctrl_conn);
        return false;
    }
    int sock_fd = wpa_ctrl_get_fd(m_monitor_conn);
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1) {
        wpa_ctrl_close(m_monitor_conn);
        wpa_ctrl_close(m_ctrl_conn);
        return false;
    }
    struct epoll_event ev;
    // only input events (wpa_supplicant will write to this socket when events occur)
    ev.events = EPOLLIN;
    ev.data.fd = sock_fd;
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev) == -1) {
        close(m_epoll_fd);
        wpa_ctrl_close(m_monitor_conn);
        wpa_ctrl_close(m_ctrl_conn);
        return false;
    }

    m_event_thread = std::thread(&WPAController::eventLoop, this);

    std::string connectedSSID = getActiveSSID();
    removeQuaotes(connectedSSID);

    emit connectedSSIDChanged(QString::fromStdString(connectedSSID));

    return true;
}

bool WPAController::sendCommand(const std::string& command, std::string& response, bool removeNewLineSymbols) {
    if (!m_ctrl_conn) return false;

    log_info("sendCommand:", command);

    char buf[4096];
    size_t len = sizeof(buf) - 1;
    
    if (wpa_ctrl_request(m_ctrl_conn, command.c_str(), command.length(), buf, &len, nullptr) < 0) {
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

bool WPAController::receiveEvent(std::string& eventStr) {
    if (!m_monitor_conn) return false;

    char buf[4096];
    size_t len = sizeof(buf) - 1;

    // Read the event from the monitor socket
    if (wpa_ctrl_recv(m_monitor_conn, buf, &len) != 0) {
        return false;
    }

    buf[len] = '\0';
    eventStr = buf;

    return true;
}

void WPAController::scan() {
    if (!m_ctrl_conn) return;
    
    std::string res;
    log_info("Starting scan...");
    
    // Just trigger the scan and exit, no sleep required
    if (!sendCommand("SCAN", res) || res != "OK") {
        log_err("Scan request failed");
        return;
    }
}

void WPAController::eventLoop() {
    log_info("eventLoop starts...", m_stop_thread.load());

    const int MAX_EVENTS = 1;
    struct epoll_event events[MAX_EVENTS];

    while (!m_stop_thread.load()) {
        // Wait for events with a 500ms timeout to allow safe thread termination
        int nfds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, 500);
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break; 
        }
        if (nfds > 0) {
            // Check if there are pending messages in the monitor socket
            while (wpa_ctrl_pending(m_monitor_conn) > 0) {
                std::string eventStr;

                if (receiveEvent(eventStr)) {
                    // Look for the scan completion event
                    if (eventStr.find("CTRL-EVENT-SCAN-RESULTS") != std::string::npos) {
                        log_info("Scan finished! Fetching results...");
                        
                        std::string res;
                        QList<Networks> foundNetworks;
                        std::map<std::string, int> bestNetworks; // unique SSIDs with their best quality

                        if (sendCommand("SCAN_RESULTS", res, false)) {
                            std::stringstream ss(res);
                            std::string line;
                            
                            // Skip header
                            if (!std::getline(ss, line)) continue;

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
                                        
                                        int signalInt = safeStoi(signal, -100); 
                                        int quality = dbmToQuality(signalInt);
                                        
                                        log_info("Network found: ", ssid, " [Quality: ", quality, "] [Frequency: ", formatFrequency(freq), "]");

                                        // Store the best quality for each SSID
                                        auto it = bestNetworks.find(ssid);
                                        if (it == bestNetworks.end() || quality > it->second) {
                                            bestNetworks[ssid] = quality;
                                        }
                                    }
                                }
                            }

                            // Convert best results to QString and populate the QList
                            for (const auto& [ssid, quality] : bestNetworks) {
                                foundNetworks.append({QString::fromStdString(ssid), quality});
                            }

                            // Emit the signal to the GUI (Qt signals are thread-safe)
                            emit resultsReady(foundNetworks);
                        }
                    }
                }
            }
        }
    }
}

std::string WPAController::getActiveSSID() {
    if (!m_ctrl_conn) return "";

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
        log_err("Failed to add network");
        return false;
    }
    std::string netId = res;

    // 2. Set SSID (must be in quotes)
    if (!sendCommand("SET_NETWORK " + netId + " ssid \"" + ssid + "\"", res) || res != "OK") {
        log_err("Failed to set SSID");
        return false;
    }

    // 3. Set Password (PSK, must be in quotes)
    if (!sendCommand("SET_NETWORK " + netId + " psk \"" + password + "\"", res) || res != "OK") {
        log_err("Failed to set password");
        return false;
    }

    // 4. Select and enable this network
    if (!sendCommand("SELECT_NETWORK " + netId, res) || res != "OK") {
        log_err("Failed to select network");
        return false;
    }

    // 5. Make changes persistent
    sendCommand("SAVE_CONFIG", res);

    log_info("Successfully connected to ", ssid, " (ID:", netId, ")");
    return true;
}

void WPAController::close_connection() {
    m_stop_thread = true;
    if (m_event_thread.joinable()) {
        m_event_thread.join();
    }

    if (m_epoll_fd != -1) {
        close(m_epoll_fd);
        m_epoll_fd = -1;
    }

    // close sockets and clean up
    if (m_monitor_conn) {
        wpa_ctrl_detach(m_monitor_conn);
        wpa_ctrl_close(m_monitor_conn);
        m_monitor_conn = nullptr;
    }
    if (m_ctrl_conn) {
        wpa_ctrl_close(m_ctrl_conn);
        m_ctrl_conn = nullptr;
    }
}

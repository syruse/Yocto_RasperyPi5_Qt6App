#ifndef WPA_CONTROLLER_H
#define WPA_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <atomic>
#include "wpa_ctrl.h"

class WPAController : public QObject {
    Q_OBJECT
public:
    struct Networks {
        QString ssid;
        int quality;
    };

    WPAController();
    ~WPAController();

    // Initialize connection to wpa_supplicant control socket
    bool init(const std::string& interface = "wlan0");
    
    // Trigger scan and print results to stdout
    void scan();
    
    // Configure a new network and connect to it
    bool select(const std::string& ssid, const std::string& password);

    // Disconnect from the current network
    bool disconnectNetwork();

signals:
    void resultsReady(const QList<WPAController::Networks> &networks);
    void connectedSSIDChanged(const QString &ssid);

private:
    struct wpa_ctrl* m_ctrl_conn{nullptr};
    struct wpa_ctrl *m_monitor_conn{nullptr};
    int m_epoll_fd = -1;
    std::thread m_event_thread;
    std::atomic<bool> m_stop_thread{false};
    
    bool sendCommand(const std::string& command, std::string& response, bool removeNewLineSymbols = true);
    bool receiveEvent(std::string& eventStr);
    void eventLoop();
    void close_connection();
    // Get and Update the currently connected SSID
    bool checkActiveSSID();
};

// Register the Networks struct as a Qt metatype for use in signals/slots in QThreads
Q_DECLARE_METATYPE(WPAController::Networks)

#endif // WPA_CONTROLLER_H
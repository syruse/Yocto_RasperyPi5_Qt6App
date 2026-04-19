#ifndef WPA_CONTROLLER_H
#define WPA_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <string>
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

signals:
    void resultsReady(const QList<WPAController::Networks> &networks);

private:
    struct wpa_ctrl* ctrl_conn;
    
    bool sendCommand(const std::string& command, std::string& response, bool removeNewLineSymbols = true);
    void close_connection();
    std::string getActiveSSID();
};

// Register the Networks struct as a Qt metatype for use in signals/slots in QThreads
Q_DECLARE_METATYPE(WPAController::Networks)

#endif // WPA_CONTROLLER_H
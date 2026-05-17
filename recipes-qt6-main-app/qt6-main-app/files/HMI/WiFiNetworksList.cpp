#include "WiFiNetworksList.h"
#include <QThread>
#include <QTimer>

WiFiNetworksList::WiFiNetworksList(QObject *parent) : QAbstractListModel(parent) {
    connect(&m_wpaCtrl, &WPAController::resultsReady, this, [this](const QList<WPAController::Networks> &networks) {
        beginResetModel();
        m_networks = networks;
        endResetModel();
        qDebug() << "Model updated with" << m_networks.size() << "networks";
        if (m_networks.isEmpty()) {
            if (++m_scanRetryCount >= 5) {
                qDebug() << "No networks found after 5 attempts, giving up.";
            } else {
                qDebug() << "No networks found, retrying scan... Attempt" << m_scanRetryCount;
                QTimer::singleShot(1500, this, [this]() {
                    m_wpaCtrl.scan();
                });
            }
        } else {
            m_scanRetryCount = 0; // reset retry count on success
        }
    });

    connect(&m_wpaCtrl, &WPAController::connectedSSIDChanged, this, [this](const QString &ssid) {
        setConnectedSSID(ssid);
    });

    QThread *workerThread = QThread::create([this]() {
        // waiting for wpa_supplicant socket to be created
        while (!m_wpaCtrl.init()) {
            qDebug() << "wpa_supplicant socket is not created yet";
            QThread::msleep(200);
        }
        qDebug() << "wpaCtrl is ready";

        m_wpaCtrl.scan();
        qDebug() << "Worker thread completed";
    });

    // Clean up the thread when it's done
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    
    workerThread->start();
}

void WiFiNetworksList::refresh() {
    qDebug() << "Refreshing WiFi networks list...";
    m_scanRetryCount = 0;
    m_wpaCtrl.scan();
}

bool WiFiNetworksList::connectToNetwork(const QString &ssid, const QString &password) {
    qDebug() << "Attempting to connect to network:" << ssid << "with password:" << password;
    return m_wpaCtrl.select(ssid.toStdString(), password.toStdString());
}

bool WiFiNetworksList::disconnectFromNetwork() {
    qDebug() << "Attempting to disconnect from current network";
    return m_wpaCtrl.disconnectNetwork();
}

void WiFiNetworksList::setConnectedSSID(const QString &ssid) {
    if (m_connectedSSID != ssid) {
        qDebug() << "Connected SSID changed to:" << ssid;
        m_connectedSSID = ssid;
        emit connectedSSIDChanged();
    }
}

int WiFiNetworksList::rowCount(const QModelIndex &parent) const {
    return m_networks.count();
}

QVariant WiFiNetworksList::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_networks.count()) return QVariant();

    const WPAController::Networks &network = m_networks[index.row()];
    if (role == SSIDRole) return network.ssid;
    if (role == SignalQualityRole) return network.quality;
    return QVariant();
}

QHash<int, QByteArray> WiFiNetworksList::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[SSIDRole] = "ssid";
    roles[SignalQualityRole] = "quality";
    return roles;
}

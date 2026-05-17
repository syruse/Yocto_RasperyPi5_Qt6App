#ifndef WIFINETWORKSLIST_H
#define WIFINETWORKSLIST_H

#include <QAbstractListModel>
#include "WPAController.h"

class WiFiNetworksList : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString connectedSSID READ connectedSSID WRITE setConnectedSSID NOTIFY connectedSSIDChanged)
public:
    enum WiFiNetworkRoles { SSIDRole = Qt::UserRole + 1, SignalQualityRole };

    explicit WiFiNetworksList(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString connectedSSID() const { return m_connectedSSID; }
    void setConnectedSSID(const QString &ssid);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool connectToNetwork(const QString &ssid, const QString &password);
    Q_INVOKABLE bool disconnectFromNetwork();

signals:
    void connectedSSIDChanged();

private:
    QString m_connectedSSID;
    QList<WPAController::Networks> m_networks;
    WPAController m_wpaCtrl;
    qint32_t m_scanRetryCount{0};
};

#endif // WIFINETWORKSLIST_H

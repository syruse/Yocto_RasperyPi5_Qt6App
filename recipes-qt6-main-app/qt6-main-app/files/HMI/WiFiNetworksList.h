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

signals:
    void connectedSSIDChanged();

private:
    QString m_connectedSSID;
    QList<WPAController::Networks> m_networks;
    WPAController m_wpaCtrl;
};

#endif // WIFINETWORKSLIST_H

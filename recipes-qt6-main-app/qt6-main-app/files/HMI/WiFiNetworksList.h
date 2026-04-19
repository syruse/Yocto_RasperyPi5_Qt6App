#ifndef WIFINETWORKSLIST_H
#define WIFINETWORKSLIST_H

#include <QAbstractListModel>
#include "WPAController.h"

class WiFiNetworksList : public QAbstractListModel
{
    Q_OBJECT
public:
    enum WiFiNetworkRoles { SSIDRole = Qt::UserRole + 1, SignalQualityRole };

    explicit WiFiNetworksList(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<WPAController::Networks> m_networks;
    WPAController m_wpaCtrl;
};

#endif // WIFINETWORKSLIST_H

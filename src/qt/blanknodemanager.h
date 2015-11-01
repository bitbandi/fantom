#ifndef BLANKNODEMANAGER_H
#define BLANKNODEMANAGER_H

#include "util.h"
#include "sync.h"

#include <QWidget>
#include <QTimer>

namespace Ui {
    class BlanknodeManager;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Blanknode Manager page widget */
class BlanknodeManager : public QWidget
{
    Q_OBJECT

public:
    explicit BlanknodeManager(QWidget *parent = 0);
    ~BlanknodeManager();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);


public slots:
    void updateNodeList();
    void updateBlankNode(QString alias, QString addr, QString privkey, QString collateral);

signals:

private:
    QTimer *timer;
    Ui::BlanknodeManager *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    CCriticalSection cs_storm;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

private slots:
    void on_copyAddressButton_clicked();
    void on_createButton_clicked();
    void on_editButton_clicked();
    void on_getConfigButton_clicked();
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_startAllButton_clicked();
    void on_stopAllButton_clicked();
    void on_removeButton_clicked();
    void on_tableWidget_2_itemSelectionChanged();
};

#endif // BLANKNODEMANAGER_H

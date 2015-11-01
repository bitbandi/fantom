#include "blanknodemanager.h"
#include "ui_blanknodemanager.h"
#include "addeditblanknode.h"
#include "blanknodeconfigdialog.h"

#include "sync.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "activeblanknode.h"
#include "blanknodeconfig.h"
#include "blanknode.h"
#include "walletdb.h"
#include "wallet.h"
#include "init.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QDebug>
#include <QScrollArea>
#include <QScroller>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

BlanknodeManager::BlanknodeManager(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlanknodeManager),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->editButton->setEnabled(false);
    ui->getConfigButton->setEnabled(false);
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
    ui->copyAddressButton->setEnabled(false);

    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    subscribeToCoreSignals();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    timer->start(30000);

    LOCK(cs_storm);
    BOOST_FOREACH(PAIRTYPE(std::string, CBlankNodeConfig) storm, pwalletMain->mapMyBlankNodes)
    {
        updateBlankNode(QString::fromStdString(storm.second.sAlias), QString::fromStdString(storm.second.sAddress), QString::fromStdString(storm.second.sBlanknodePrivKey), QString::fromStdString(storm.second.sCollateralAddress));
    }

    updateNodeList();
}

BlanknodeManager::~BlanknodeManager()
{
    delete ui;
}

static void NotifyBlankNodeUpdated(BlanknodeManager *page, CBlankNodeConfig nodeConfig)
{
    // alias, address, privkey, collateral address
    QString alias = QString::fromStdString(nodeConfig.sAlias);
    QString addr = QString::fromStdString(nodeConfig.sAddress);
    QString privkey = QString::fromStdString(nodeConfig.sBlanknodePrivKey);
    QString collateral = QString::fromStdString(nodeConfig.sCollateralAddress);
    
    QMetaObject::invokeMethod(page, "updateBlankNode", Qt::QueuedConnection,
                              Q_ARG(QString, alias),
                              Q_ARG(QString, addr),
                              Q_ARG(QString, privkey),
                              Q_ARG(QString, collateral)
                              );
}

void BlanknodeManager::subscribeToCoreSignals()
{
    // Connect signals to core
    uiInterface.NotifyBlankNodeChanged.connect(boost::bind(&NotifyBlankNodeUpdated, this, _1));
}

void BlanknodeManager::unsubscribeFromCoreSignals()
{
    // Disconnect signals from core
    uiInterface.NotifyBlankNodeChanged.disconnect(boost::bind(&NotifyBlankNodeUpdated, this, _1));
}

void BlanknodeManager::on_tableWidget_2_itemSelectionChanged()
{
    if(ui->tableWidget_2->selectedItems().count() > 0)
    {
        ui->editButton->setEnabled(true);
        ui->getConfigButton->setEnabled(true);
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(true);
	ui->copyAddressButton->setEnabled(true);
    }
}

void BlanknodeManager::updateBlankNode(QString alias, QString addr, QString privkey, QString collateral)
{
    LOCK(cs_storm);
    bool bFound = false;
    int nodeRow = 0;
    for(int i=0; i < ui->tableWidget_2->rowCount(); i++)
    {
        if(ui->tableWidget_2->item(i, 0)->text() == alias)
        {
            bFound = true;
            nodeRow = i;
            break;
        }
    }

    if(nodeRow == 0 && !bFound)
        ui->tableWidget_2->insertRow(0);

    QTableWidgetItem *aliasItem = new QTableWidgetItem(alias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(addr);
    QTableWidgetItem *statusItem = new QTableWidgetItem("");
    QTableWidgetItem *collateralItem = new QTableWidgetItem(collateral);

    ui->tableWidget_2->setItem(nodeRow, 0, aliasItem);
    ui->tableWidget_2->setItem(nodeRow, 1, addrItem);
    ui->tableWidget_2->setItem(nodeRow, 2, statusItem);
    ui->tableWidget_2->setItem(nodeRow, 3, collateralItem);
}

static QString seconds_to_DHMS(quint32 duration)
{
  QString res;
  int seconds = (int) (duration % 60);
  duration /= 60;
  int minutes = (int) (duration % 60);
  duration /= 60;
  int hours = (int) (duration % 24);
  int days = (int) (duration / 24);
  if((hours == 0)&&(days == 0))
      return res.sprintf("%02dm:%02ds", minutes, seconds);
  if (days == 0)
      return res.sprintf("%02dh:%02dm:%02ds", hours, minutes, seconds);
  return res.sprintf("%dd %02dh:%02dm:%02ds", days, hours, minutes, seconds);
}

void BlanknodeManager::updateNodeList()
{
    TRY_LOCK(cs_blanknodes, lockBlanknodes);
    if(!lockBlanknodes)
        return;

    ui->countLabel->setText("Updating...");
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(0);
    
    std::vector<CBlanknode> vBlanknodes = snodeman.GetFullBlanknodeVector();
    BOOST_FOREACH(CBlanknode& sn, vBlanknodes)
    {
        int snRow = 0;
        ui->tableWidget->insertRow(0);

 	// populate list
	// Address, Rank, Active, Active Seconds, Last Seen, Pub Key
	QTableWidgetItem *activeItem = new QTableWidgetItem(QString::number(sn.IsEnabled()));
	QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(sn.addr.ToString()));
	QTableWidgetItem *rankItem = new QTableWidgetItem(QString::number(snodeman.GetBlanknodeRank(sn.vin, pindexBest->nHeight)));
	QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(seconds_to_DHMS((qint64)(sn.lastTimeSeen - sn.sigTime)));
	QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat(sn.lastTimeSeen)));
	
	CScript pubkey;
    pubkey = GetScriptForDestination(sn.pubkey.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CFantomAddress address2(address1);
	QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(address2.ToString()));
	
	ui->tableWidget->setItem(snRow, 0, addressItem);
	ui->tableWidget->setItem(snRow, 1, rankItem);
	ui->tableWidget->setItem(snRow, 2, activeItem);
	ui->tableWidget->setItem(snRow, 3, activeSecondsItem);
	ui->tableWidget->setItem(snRow, 4, lastSeenItem);
	ui->tableWidget->setItem(snRow, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidget->rowCount()));
    
    if(pwalletMain)
    {
        LOCK(cs_blanknodes);
        BOOST_FOREACH(PAIRTYPE(std::string, CBlankNodeConfig) storm, pwalletMain->mapMyBlankNodes)
        {
            updateBlankNode(QString::fromStdString(storm.second.sAlias), QString::fromStdString(storm.second.sAddress), QString::fromStdString(storm.second.sBlanknodePrivKey), QString::fromStdString(storm.second.sCollateralAddress));
        }
    }
}

void BlanknodeManager::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
    }
}

void BlanknodeManager::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
    }

}

void BlanknodeManager::on_createButton_clicked()
{
    AddEditBlankNode* aenode = new AddEditBlankNode();
    aenode->exec();
}

void BlanknodeManager::on_copyAddressButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sCollateralAddress = ui->tableWidget_2->item(r, 3)->text().toStdString();

    QApplication::clipboard()->setText(QString::fromStdString(sCollateralAddress));
}

void BlanknodeManager::on_editButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();

    // get existing config entry

}

void BlanknodeManager::on_getConfigButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CBlankNodeConfig c = pwalletMain->mapMyBlankNodes[sAddress];
    std::string sPrivKey = c.sBlanknodePrivKey;
    BlankNodeConfigDialog* d = new BlankNodeConfigDialog(this, QString::fromStdString(sAddress), QString::fromStdString(sPrivKey));
    d->exec();
}

void BlanknodeManager::on_removeButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QMessageBox::StandardButton confirm;
    confirm = QMessageBox::question(this, "Delete Blanknode?", "Are you sure you want to delete this Blanknode configuration?", QMessageBox::Yes|QMessageBox::No);

    if(confirm == QMessageBox::Yes)
    {
        QModelIndex index = selected.at(0);
        int r = index.row();
        std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
        CBlankNodeConfig c = pwalletMain->mapMyBlankNodes[sAddress];
        CWalletDB walletdb(pwalletMain->strWalletFile);
        pwalletMain->mapMyBlankNodes.erase(sAddress);
        walletdb.EraseBlankNodeConfig(c.sAddress);
        ui->tableWidget_2->clearContents();
        ui->tableWidget_2->setRowCount(0);
        BOOST_FOREACH(PAIRTYPE(std::string, CBlankNodeConfig) storm, pwalletMain->mapMyBlankNodes)
        {
            updateBlankNode(QString::fromStdString(storm.second.sAlias), QString::fromStdString(storm.second.sAddress), QString::fromStdString(storm.second.sBlanknodePrivKey), QString::fromStdString(storm.second.sCollateralAddress));
        }
    }
}

void BlanknodeManager::on_startButton_clicked()
{
    // start the node
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CBlankNodeConfig c = pwalletMain->mapMyBlankNodes[sAddress];

    std::string errorMessage;
    bool result = activeBlanknode.RegisterByPubKey(c.sAddress, c.sBlanknodePrivKey, c.sCollateralAddress, errorMessage);

    QMessageBox msg;
    if(result)
        msg.setText("Blanknode at " + QString::fromStdString(c.sAddress) + " started.");
    else
        msg.setText("Error: " + QString::fromStdString(errorMessage));

    msg.exec();
}

void BlanknodeManager::on_stopButton_clicked()
{
    // start the node
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CBlankNodeConfig c = pwalletMain->mapMyBlankNodes[sAddress];

    std::string errorMessage;
    bool result = activeBlanknode.StopBlankNode(c.sAddress, c.sBlanknodePrivKey, errorMessage);
    QMessageBox msg;
    if(result)
    {
        msg.setText("Blanknode at " + QString::fromStdString(c.sAddress) + " stopped.");
    }
    else
    {
        msg.setText("Error: " + QString::fromStdString(errorMessage));
    }
    msg.exec();
}

void BlanknodeManager::on_startAllButton_clicked()
{
    std::string results;
    BOOST_FOREACH(PAIRTYPE(std::string, CBlankNodeConfig) storm, pwalletMain->mapMyBlankNodes)
    {
        CBlankNodeConfig c = storm.second;
	std::string errorMessage;
        bool result = activeBlanknode.RegisterByPubKey(c.sAddress, c.sBlanknodePrivKey, c.sCollateralAddress, errorMessage);
	if(result)
	{
   	    results += c.sAddress + ": STARTED\n";
	}	
	else
	{
	    results += c.sAddress + ": ERROR: " + errorMessage + "\n";
	}
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(results));
    msg.exec();
}

void BlanknodeManager::on_stopAllButton_clicked()
{
    std::string results;
    BOOST_FOREACH(PAIRTYPE(std::string, CBlankNodeConfig) storm, pwalletMain->mapMyBlankNodes)
    {
        CBlankNodeConfig c = storm.second;
	std::string errorMessage;
        bool result = activeBlanknode.StopBlankNode(c.sAddress, c.sBlanknodePrivKey, errorMessage);
	if(result)
	{
   	    results += c.sAddress + ": STOPPED\n";
	}	
	else
	{
	    results += c.sAddress + ": ERROR: " + errorMessage + "\n";
	}
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(results));
    msg.exec();
}

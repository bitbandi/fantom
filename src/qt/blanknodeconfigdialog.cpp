#include "blanknodeconfigdialog.h"
#include "ui_blanknodeconfigdialog.h"

#include <QModelIndex>

BlankNodeConfigDialog::BlankNodeConfigDialog(QWidget *parent, QString nodeAddress, QString privkey) :
    QDialog(parent),
    ui(new Ui::BlankNodeConfigDialog)
{
    ui->setupUi(this);
    QString desc = "rpcallowip=127.0.0.1<br>rpcuser=REPLACEME<br>rpcpassword=REPLACEME<br>server=1<br>listen=1<br>port=REPLACEMEWITHYOURPORT<br>blanknode=1<br>blanknodeaddr=" + nodeAddress + "<br>blanknodeprivkey=" + privkey + "<br>";
    ui->detailText->setHtml(desc);
}

BlankNodeConfigDialog::~BlankNodeConfigDialog()
{
    delete ui;
}

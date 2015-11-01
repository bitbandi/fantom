#include "zerosendconfig.h"
#include "ui_zerosendconfig.h"

#include "fantomunits.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "init.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QSettings>

ZerosendConfig::ZerosendConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZerosendConfig),
    model(0)
{
    ui->setupUi(this);

    connect(ui->buttonBasic, SIGNAL(clicked()), this, SLOT(clickBasic()));
    connect(ui->buttonHigh, SIGNAL(clicked()), this, SLOT(clickHigh()));
    connect(ui->buttonMax, SIGNAL(clicked()), this, SLOT(clickMax()));
}

ZerosendConfig::~ZerosendConfig()
{
    delete ui;
}

void ZerosendConfig::setModel(WalletModel *model)
{
    this->model = model;
}

void ZerosendConfig::clickBasic()
{
    configure(true, 1000, 2);

    QString strAmount(FantomUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Zerosend Configuration"),
        tr(
            "Zerosend was successfully set to basic (%1 and 2 rounds). You can change this at any time by opening Fantom's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ZerosendConfig::clickHigh()
{
    configure(true, 1000, 8);

    QString strAmount(FantomUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Zerosend Configuration"),
        tr(
            "Zerosend was successfully set to high (%1 and 8 rounds). You can change this at any time by opening Fantom's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ZerosendConfig::clickMax()
{
    configure(true, 1000, 16);

    QString strAmount(FantomUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Zerosend Configuration"),
        tr(
            "Zerosend was successfully set to maximum (%1 and 16 rounds). You can change this at any time by opening Fantom's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ZerosendConfig::configure(bool enabled, int coins, int rounds) {

    QSettings settings;

    settings.setValue("nZerosendRounds", rounds);
    settings.setValue("nAnonymizeFantomAmount", coins);

    nZerosendRounds = rounds;
    nAnonymizeFantomAmount = coins;
}

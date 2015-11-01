#ifndef ZEROSENDCONFIG_H
#define ZEROSENDCONFIG_H

#include <QDialog>

namespace Ui {
    class ZerosendConfig;
}
class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class ZerosendConfig : public QDialog
{
    Q_OBJECT

public:

    ZerosendConfig(QWidget *parent = 0);
    ~ZerosendConfig();

    void setModel(WalletModel *model);


private:
    Ui::ZerosendConfig *ui;
    WalletModel *model;
    void configure(bool enabled, int coins, int rounds);

private slots:

    void clickBasic();
    void clickHigh();
    void clickMax();
};

#endif // ZEROSENDCONFIG_H

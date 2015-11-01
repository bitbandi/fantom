#ifndef BLANKNODECONFIGDIALOG_H
#define BLANKNODECONFIGDIALOG_H

#include <QDialog>

namespace Ui {
    class BlankNodeConfigDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class BlankNodeConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BlankNodeConfigDialog(QWidget *parent = 0, QString nodeAddress = "123.456.789.123:31000", QString privkey="BLANKNODEPRIVKEY");
    ~BlankNodeConfigDialog();

private:
    Ui::BlankNodeConfigDialog *ui;
};

#endif // BLANKNODECONFIGDIALOG_H

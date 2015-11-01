#ifndef ADDEDITBLANKNODE_H
#define ADDEDITBLANKNODE_H

#include <QDialog>

namespace Ui {
class AddEditBlankNode;
}


class AddEditBlankNode : public QDialog
{
    Q_OBJECT

public:
    explicit AddEditBlankNode(QWidget *parent = 0);
    ~AddEditBlankNode();

protected:

private slots:
    void on_okButton_clicked();
    void on_cancelButton_clicked();

signals:

private:
    Ui::AddEditBlankNode *ui;
};

#endif // ADDEDITBLANKNODE_H

#ifndef FANTOMMARKET_H
#define FANTOMMARKET_H

#include <QWidget>
#include <QTimer>
#include <QListWidgetItem>

namespace Ui {
    class FantomMarket;
}

class FantomMarket : public QWidget
{
    Q_OBJECT

public:
    explicit FantomMarket(QWidget *parent = 0);
    ~FantomMarket();
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void updateCategories();

public slots:
    void updateCategory(QString category);

private:
    Ui::FantomMarket *ui;
    

private slots:
    void on_tableWidget_itemSelectionChanged();
    void on_categoriesListWidget_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void on_buyButton_clicked();
    void on_viewDetailsButton_clicked();
    void on_copyAddressButton_clicked();
};

#endif // FANTOMMARKET_H

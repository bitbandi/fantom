#ifndef FANTOMADDRESSVALIDATOR_H
#define FANTOMADDRESSVALIDATOR_H

#include <QValidator>

/** Base48 entry widget validator.
   Corrects near-miss characters and refuses characters that are no part of base48.
 */
class FantomAddressValidator : public QValidator
{
    Q_OBJECT

public:
    explicit FantomAddressValidator(QObject *parent = 0);

    State validate(QString &input, int &pos) const;

    static const int MaxAddressLength = 128;
};

#endif // FANTOMADDRESSVALIDATOR_H

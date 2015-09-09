#ifndef BITCOINUNITS_H
#define BITCOINUNITS_H

#include "walletmodel.h"
#include <QString>
#include <QAbstractListModel>

/** Bitcoin unit definitions. Encapsulates parsing and formatting
   and serves as list model for drop-down selection boxes.
*/
class BitcoinUnits: public QAbstractListModel
{
public:
    explicit BitcoinUnits(QObject *parent, WalletModel *wModel);

    /** Bitcoin units.
      @note Source: https://en.bitcoin.it/wiki/Units . Please add only sensible ones
     */
    enum Unit
    {
        SLR,
        mSLR,
        uSLR
    };

    //! @name Static API
    //! Unit conversion and formatting
    ///@{

    //! Get list of units, for drop-down box
    static QList<Unit> availableUnits();
    //! Is unit ID valid?
    static bool valid(int unit);
    //! Short name
    static QString name(int unit);
    //! Longer description
    static QString description(int unit);
    //! Number of Satoshis (1e-8) per unit
    static qint64 factor(int unit);
    //! Number of amount digits (to represent max number of coins)
    static int amountDigits(int unit);
    //! Maximum number of decimals left
    static int maxdecimals(int unit);
    //! Number of decimals from options left
    static int decimals(int unit);
    //! Format as string using decimal points defined in options
    static QString format(int unit, qint64 amount, bool plussign=false, bool hideamounts=false);
    //! Format as string using specified decimal points
    static QString formatMaxDecimals(int unit, qint64 n, int decimals, bool plussign=false, bool hideamounts=false, bool pretty=true);
    //! Format as string using max fee decimal points
    static QString formatFee(int unit, qint64 amount, bool plussign=false);
    //! Format as string (with unit) using decimal points defined in options
    static QString formatWithUnit(int unit, qint64 amount, bool plussign=false, bool hideamounts=false);
    //! Format as string (with unit) using specified decimal points
    static QString formatWithUnitWithMaxDecimals(int unit, qint64 amount, int decimals, bool plussign=false, bool hideamounts=false);
    //! Format as string (with unit) with max fee decimal points
    static QString formatWithUnitFee(int unit, qint64 amount, bool plussign=false);
    //! Parse string to coin amount
    static bool parse(int unit, const QString &value, qint64 *val_out);
    ///@}

    //! @name AbstractListModel implementation
    //! List model for unit drop-down selection box.
    ///@{
    enum RoleIndex {
        /** Unit identifier */
        UnitRole = Qt::UserRole
    };
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    ///@}
private:
    QList<BitcoinUnits::Unit> unitlist;
};
typedef BitcoinUnits::Unit BitcoinUnit;

#endif // BITCOINUNITS_H

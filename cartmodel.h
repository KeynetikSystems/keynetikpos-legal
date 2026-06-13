// =============================================================================
// cartmodel.h — CartModel + CartQtyDelegate: model/view adapter for the cart
// -----------------------------------------------------------------------------
// WHAT: CartModel is a QAbstractTableModel exposing the CURRENT cart's line
//       items (Product / Price / Qty / Subtotal / ✖) to a QTableView. Qty is
//       editable in place; clicking the ✖ column removes the row (handled by
//       the view's clicked() signal — no per-row widgets). CartQtyDelegate
//       gives the Qty column a QSpinBox capped at available stock.
// HOW:  The model holds a CartService pointer and resets on its change
//       signals. setData() routes edits back through the service so every
//       observer stays in sync; an internal-change guard swaps the full reset
//       for a dataChanged() on the edited row so the editor closes cleanly.
//       Stock limits come from an injected stockProvider callback (the main
//       window supplies a Database lookup) so the model stays DB-agnostic.
// WHY:  Replaces the QTableWidget that rebuilt per-row Edit/Remove buttons on
//       every change and resolved button rows by scanning cell widgets. A
//       model keeps the view in sync automatically and removes the row-index
//       bookkeeping hazards entirely.
// =============================================================================
#ifndef CARTMODEL_H
#define CARTMODEL_H

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <functional>

class CartService;

class CartModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColProduct = 0,
        ColPrice,
        ColQty,
        ColSubtotal,
        ColRemove,
        ColCount
    };

    static constexpr int ProductIdRole = Qt::UserRole + 1;

    explicit CartModel(CartService *service, QObject *parent = nullptr);

    // productId -> units in stock; supplied by the UI layer
    using StockProvider = std::function<int(int)>;
    void setStockProvider(StockProvider provider) { m_stock = std::move(provider); }
    int  stockFor(int productId) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;

private slots:
    void onCartChanged();

private:
    CartService  *m_service;
    StockProvider m_stock;
    bool          m_internalChange { false };
};

// -----------------------------------------------------------------------------
// CartQtyDelegate — spin-box editor for the Qty column, capped at stock
// -----------------------------------------------------------------------------
class CartQtyDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CartQtyDelegate(const CartModel *model, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

private:
    const CartModel *m_model;
};

#endif // CARTMODEL_H

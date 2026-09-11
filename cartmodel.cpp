// =============================================================================
// cartmodel.cpp — Implementation of CartModel + CartQtyDelegate (see
// cartmodel.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The model resets on every service change signal. Carts are small (tens
//    of lines at most), so a reset is simpler and fast enough; the
//    m_internalChange guard avoids resetting mid-commit when the edit came
//    from this model's own setData().
//  - Qty edits are clamped to [1, max(stock, current qty)] — the upper bound
//    allows keeping an existing quantity even if stock dropped underneath it;
//    Database::recordSale() remains the hard enforcement at checkout.
// =============================================================================
#include "cartmodel.h"
#include "cartservice.h"
#include "colorscheme.h"

#include <QFont>
#include <QBrush>
#include <QSpinBox>

namespace {
const Cart *currentCart(CartService *service)
{
    return service ? service->current() : nullptr;
}
} // namespace

CartModel::CartModel(CartService *service, QObject *parent)
    : QAbstractTableModel(parent)
    , m_service(service)
{
    connect(m_service, &CartService::cartContentChanged,
            this, &CartModel::onCartChanged);
    connect(m_service, &CartService::currentCartChanged,
            this, &CartModel::onCartChanged);
    connect(m_service, &CartService::cartListChanged,
            this, &CartModel::onCartChanged);
}

int CartModel::stockFor(int productId) const
{
    return m_stock ? m_stock(productId) : 9999;
}

void CartModel::onCartChanged()
{
    if (m_internalChange)
        return;   // setData() already emitted dataChanged for the edited row
    beginResetModel();
    endResetModel();
}

int CartModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    const Cart *cart = currentCart(m_service);
    return cart ? cart->items.size() : 0;
}

int CartModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant CartModel::data(const QModelIndex &index, int role) const
{
    const Cart *cart = currentCart(m_service);
    if (!cart || !index.isValid() ||
        index.row() < 0 || index.row() >= cart->items.size())
        return {};

    const CartItem &item = cart->items[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColProduct:  return item.name;
        case ColPrice:    return formatMoney(item.price);
        case ColDec:      return QStringLiteral("-");
        case ColQty:      return item.quantity;
        case ColInc:      return QStringLiteral("+");
        case ColSubtotal: return formatMoney(item.getSubtotal());
        case ColRemove:   return QStringLiteral("X");
        }
        break;

    case Qt::EditRole:
        if (index.column() == ColQty)
            return item.quantity;
        break;

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColQty:
        case ColDec:
        case ColInc:
        case ColRemove: return int(Qt::AlignCenter);
        }
        break;

    case Qt::FontRole:
        if (index.column() == ColDec || index.column() == ColInc) {
            // Larger than the surrounding table text so the tap targets read
            // as buttons at a glance, not just column labels.
            QFont f;
            f.setBold(true);
            f.setPointSize(f.pointSize() + 4);
            return f;
        }
        if (index.column() == ColProduct) {
            QFont f;
            f.setBold(true);
            return f;
        }
        break;

    case Qt::BackgroundRole:
        if (index.column() == ColDec)
            return QBrush(QColor(getColorScheme().accentPrimary));   // green
        if (index.column() == ColInc)
            return QBrush(QColor(getColorScheme().accentTertiary));  // mauve/purple
        break;

    case Qt::ForegroundRole:
        if (index.column() == ColRemove)
            return QBrush(QColor(getColorScheme().error));
        if (index.column() == ColDec || index.column() == ColInc)
            return QBrush(Qt::white);
        break;

    case Qt::ToolTipRole:
        if (index.column() == ColDec)    return QStringLiteral("Decrease quantity");
        if (index.column() == ColInc)    return QStringLiteral("Increase quantity");
        if (index.column() == ColQty)    return QStringLiteral("Tap - / + or double-click to edit quantity");
        if (index.column() == ColRemove) return QStringLiteral("Remove item");
        break;

    case ProductIdRole:
        return item.productId;
    }

    return {};
}

QVariant CartModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColProduct:  return QStringLiteral("Product");
    case ColPrice:    return QStringLiteral("Price");
    case ColDec:      return QString();
    case ColQty:      return QStringLiteral("Qty");
    case ColInc:      return QString();
    case ColSubtotal: return QStringLiteral("Subtotal");
    case ColRemove:   return QString();
    }
    return {};
}

Qt::ItemFlags CartModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.column() == ColQty)
        f |= Qt::ItemIsEditable;
    return f;
}

bool CartModel::setData(const QModelIndex &index, const QVariant &value,
                        int role)
{
    if (role != Qt::EditRole || index.column() != ColQty)
        return false;

    const Cart *cart = currentCart(m_service);
    if (!cart || index.row() < 0 || index.row() >= cart->items.size())
        return false;

    const CartItem &item = cart->items[index.row()];
    const int maxQty = qMax(item.quantity, stockFor(item.productId));
    const int newQty = qBound(1, value.toInt(), qMax(1, maxQty));

    m_internalChange = true;
    m_service->setItemQuantity(index.row(), newQty);
    m_internalChange = false;

    emit dataChanged(this->index(index.row(), ColQty),
                     this->index(index.row(), ColSubtotal));
    return true;
}

void CartModel::adjustQuantity(int row, int delta)
{
    const Cart *cart = currentCart(m_service);
    if (!cart || row < 0 || row >= cart->items.size())
        return;
    // Reuse setData()'s clamping so the − / + taps and the spin editor behave
    // identically. Decrementing past 1 is a no-op; ✖ removes a line.
    setData(index(row, ColQty), cart->items[row].quantity + delta, Qt::EditRole);
}

// =============================================================================
// CartQtyDelegate
// =============================================================================

CartQtyDelegate::CartQtyDelegate(const CartModel *model, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
{
}

QWidget *CartQtyDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(option)

    auto *spin = new QSpinBox(parent);
    const int productId  = index.data(CartModel::ProductIdRole).toInt();
    const int currentQty = index.data(Qt::EditRole).toInt();
    const int stock      = m_model ? m_model->stockFor(productId) : 9999;

    // Allow keeping the current quantity even if stock dropped underneath it;
    // Database::recordSale() is the hard enforcement at checkout.
    spin->setRange(1, qMax(1, qMax(stock, currentQty)));
    spin->setAlignment(Qt::AlignCenter);
    return spin;
}

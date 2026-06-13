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
        case ColPrice:    return formatKsh(item.price);
        case ColQty:      return item.quantity;
        case ColSubtotal: return formatKsh(item.getSubtotal());
        case ColRemove:   return QStringLiteral("✖");
        }
        break;

    case Qt::EditRole:
        if (index.column() == ColQty)
            return item.quantity;
        break;

    case Qt::TextAlignmentRole:
        if (index.column() == ColQty || index.column() == ColRemove)
            return int(Qt::AlignCenter);
        break;

    case Qt::FontRole:
        if (index.column() == ColProduct) {
            QFont f;
            f.setBold(true);
            return f;
        }
        break;

    case Qt::ForegroundRole:
        if (index.column() == ColRemove)
            return QBrush(QColor(getColorScheme().error));
        break;

    case Qt::ToolTipRole:
        if (index.column() == ColQty)    return QStringLiteral("Double-click to edit quantity");
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
    case ColQty:      return QStringLiteral("Qty");
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

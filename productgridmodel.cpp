// =============================================================================
// productgridmodel.cpp — Implementation of the product-grid model/view classes
// (see productgridmodel.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - setProducts() does a full model reset: the catalogue reloads only after
//    checkout / inventory edits, and the view has no per-row state worth
//    preserving.
//  - The delegate paints everything itself (no widget), reading the active
//    ColorScheme each call so theme toggles take effect on the next repaint.
// =============================================================================
#include "productgridmodel.h"
#include "colorscheme.h"
#include "inventorymanager.h"   // calculateStatus() — single severity rule
#include "cart.h"          // formatKsh()

#include <QPainter>
#include <QFontMetrics>
#include <QListView>

// ─────────────────────────────────────────────────────────────────────────────
// ProductGridModel
// ─────────────────────────────────────────────────────────────────────────────

ProductGridModel::ProductGridModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ProductGridModel::setProducts(const QVector<Product> &products)
{
    beginResetModel();
    m_products = products;
    endResetModel();
}

void ProductGridModel::updateStock(int productId, int newQty)
{
    for (int row = 0; row < m_products.size(); ++row) {
        if (m_products[row].id == productId) {
            m_products[row].stockQuantity = newQty;
            const QModelIndex idx = index(row);
            // Default (empty) roles == "all roles": repaints the card and lets
            // the view re-evaluate flags() so it enables/disables on stock-out.
            emit dataChanged(idx, idx);
            return;
        }
    }
}

int ProductGridModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_products.size();
}

QVariant ProductGridModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_products.size())
        return {};

    const Product &p = m_products[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:        return p.name;
    case Qt::ToolTipRole:
        return p.stockQuantity > 0
                   ? QString("Click to add %1 to cart").arg(p.name)
                   : QString("%1 is out of stock").arg(p.name);
    case ProductIdRole:   return p.id;
    case PriceRole:       return QVariant::fromValue<qlonglong>(p.price.cents());
    case StockRole:       return p.stockQuantity;
    case BarcodeRole:     return p.barcode;
    case CategoryRole:    return p.category;
    case ReorderLevelRole: return p.reorderLevel;
    }
    return {};
}

Qt::ItemFlags ProductGridModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    // Out-of-stock products stay visible but are not clickable.
    if (index.isValid() &&
        index.data(StockRole).toInt() <= 0)
        f &= ~Qt::ItemIsEnabled;
    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// ProductFilterProxy
// ─────────────────────────────────────────────────────────────────────────────

ProductFilterProxy::ProductFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

// invalidateFilter() is the only filter-refresh API stable across the Qt
// versions we build against (the newer begin/endFilterChange() is 6.11-only,
// and invalidateRowsFilter() is itself deprecated in 6.11). Silence the
// deprecation locally rather than fork on QT_VERSION.
void ProductFilterProxy::refreshFilter()
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    invalidateFilter();
    QT_WARNING_POP
}

void ProductFilterProxy::setSearchText(const QString &text)
{
    m_search = text.trimmed();
    refreshFilter();
}

void ProductFilterProxy::setCategory(const QString &category)
{
    m_category = (category == "All Categories") ? QString() : category;
    refreshFilter();
}

bool ProductFilterProxy::filterAcceptsRow(int sourceRow,
                                          const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!m_category.isEmpty() &&
        idx.data(ProductGridModel::CategoryRole).toString() != m_category)
        return false;

    if (m_search.isEmpty())
        return true;

    return idx.data(ProductGridModel::NameRole).toString()
                   .contains(m_search, Qt::CaseInsensitive)
           || idx.data(ProductGridModel::BarcodeRole).toString()
                   .contains(m_search, Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────────────────────────────────────
// ProductCardDelegate
// ─────────────────────────────────────────────────────────────────────────────

ProductCardDelegate::ProductCardDelegate(int criticalThreshold,
                                         int mediumThreshold, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_critical(criticalThreshold)
    , m_medium(mediumThreshold)
{
}

QSize ProductCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &) const
{
    // Scale the card with the active font/DPI instead of a fixed pixel size so
    // the grid stays legible on small cashier displays and crisp on hi-DPI.
    const QFontMetrics fm(option.font);
    // Min card width tuned so a typical cashier window fits ~6 columns; the grid
    // stays responsive (fewer columns on a narrow screen, more on a wide one)
    // since the column count below is derived from this width vs. the viewport.
    const int minW = qMax(150, fm.averageCharWidth() * 20);
    const int h    = qMax(96,  fm.height() * 5);

    // Stretch the card width so a full row of cards exactly fills the
    // viewport — otherwise QListView's IconMode grid wraps at a fixed card
    // width and leaves a dead gap on the right whenever the viewport width
    // isn't an exact multiple of (card + spacing).
    const auto *view = qobject_cast<const QListView *>(option.widget);
    if (!view)
        return QSize(minW, h);

    const int spacing = qMax(0, view->spacing());
    const int vw = view->viewport()->width();
    if (vw <= minW + 2 * spacing)
        return QSize(minW, h);

    const int columns = qMax(1, (vw - spacing) / (minW + spacing));
    const int w = (vw - spacing * (columns + 1)) / columns;
    return QSize(qMax(w, minW), h);
}

void ProductCardDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const ColorScheme scheme = getColorScheme();
    const int stock = index.data(ProductGridModel::StockRole).toInt();

    // Per-product reorder level drives severity (same rule as the rest of the
    // app); fall back to the delegate's medium threshold for any model that
    // doesn't supply the role.
    const QVariant rlVar = index.data(ProductGridModel::ReorderLevelRole);
    const int reorder = rlVar.isValid() ? rlVar.toInt() : m_medium;

    // Severity is conveyed by a TEXT label as well as colour, so the grid is
    // readable for colour-blind cashiers (and screen readers via the tooltip).
    QColor bg;
    QString severity;
    // Stock-health colours come from the dedicated status* fields, not the
    // warning/error/accentPrimary semantic colours — otherwise a low-stock
    // card and a "danger" action button look identical, which reads as the
    // grid itself being in some error state.
    switch (InventoryManager::calculateStatus(stock, reorder)) {
    case InventoryStatus::OutOfStock:
        bg = QColor(scheme.disabledBg);
        break;
    case InventoryStatus::Critical:
        bg = QColor(scheme.statusCritical);
        severity = QStringLiteral("CRITICAL");
        break;
    case InventoryStatus::Low:
        bg = QColor(scheme.statusLow);
        severity = QStringLiteral("LOW");
        break;
    case InventoryStatus::Healthy:
        bg = QColor(scheme.statusHealthy);
        break;
    }

    // Pick text colour by background luminance so it always meets contrast —
    // white on the orange "low" card failed WCAG AA.
    QColor fg;
    if (stock <= 0) {
        fg = QColor(scheme.disabledText);
    } else {
        const double lum = 0.299 * bg.red() + 0.587 * bg.green()
                           + 0.114 * bg.blue();
        // NOTE: intentionally NOT scheme.textPrimary here — this is a
        // contrast decision against the status chip's own background
        // luminance, not the theme's body-text colour. In Dark theme,
        // textPrimary (#e0e0e0) is itself near-white and would defeat the
        // luminance check on a light status colour (e.g. statusLow). A
        // fixed near-black/white pair is what the contrast formula actually
        // needs.
        fg = lum > 150 ? QColor(0x1a, 0x1a, 0x1a) : QColor(Qt::white);
    }

    if (stock > 0 && (option.state & (QStyle::State_MouseOver |
                                      QStyle::State_Selected)))
        bg = bg.darker(112);

    const QRect card = option.rect.adjusted(2, 2, -2, -2);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(card, 8, 8);

    const QRect inner = card.adjusted(10, 8, -10, -8);
    const int nameH = inner.height() * 55 / 100;

    QFont nameFont = option.font;
    nameFont.setPointSize(11);
    nameFont.setBold(true);

    painter->setPen(fg);
    painter->setFont(nameFont);
    painter->setClipRect(inner);
    painter->drawText(QRect(inner.left(), inner.top(), inner.width(), nameH),
                      Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                      index.data(ProductGridModel::NameRole).toString());

    QFont detailFont = option.font;
    detailFont.setPointSize(10);
    detailFont.setBold(true);
    painter->setFont(detailFont);

    const QString price =
        formatMoney(Money::fromCents(index.data(ProductGridModel::PriceRole).toLongLong()));
    const QString stockText =
        stock <= 0      ? QStringLiteral("OUT OF STOCK")
        : !severity.isEmpty() ? QString("%1 — %2 left").arg(severity).arg(stock)
                              : QString("Stock: %1").arg(stock);

    painter->drawText(QRect(inner.left(), inner.top() + nameH,
                            inner.width(), (inner.height() - nameH) / 2),
                      Qt::AlignHCenter | Qt::AlignVCenter, price);
    painter->drawText(QRect(inner.left(),
                            inner.top() + nameH + (inner.height() - nameH) / 2,
                            inner.width(), (inner.height() - nameH) / 2),
                      Qt::AlignHCenter | Qt::AlignVCenter, stockText);
    painter->restore();
}

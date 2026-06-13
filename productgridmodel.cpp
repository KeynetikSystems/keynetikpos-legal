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
#include "cart.h"          // formatKsh()

#include <QPainter>
#include <QFontMetrics>

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
    case PriceRole:       return p.price;
    case StockRole:       return p.stockQuantity;
    case BarcodeRole:     return p.barcode;
    case CategoryRole:    return p.category;
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

void ProductFilterProxy::setSearchText(const QString &text)
{
    m_search = text.trimmed();
    invalidateFilter();
}

void ProductFilterProxy::setCategory(const QString &category)
{
    m_category = (category == "All Categories") ? QString() : category;
    invalidateFilter();
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
    const int w = qMax(170, fm.averageCharWidth() * 24);
    const int h = qMax(96,  fm.height() * 5);
    return QSize(w, h);
}

void ProductCardDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const ColorScheme scheme = getColorScheme();
    const int stock = index.data(ProductGridModel::StockRole).toInt();

    // Severity is conveyed by a TEXT label as well as colour, so the grid is
    // readable for colour-blind cashiers (and screen readers via the tooltip).
    QColor bg;
    QString severity;
    if (stock <= 0) {
        bg = QColor(scheme.disabledBg);
    } else if (stock <= m_critical) {
        bg = QColor(scheme.error);
        severity = QStringLiteral("CRITICAL");
    } else if (stock <= m_medium) {
        bg = QColor(scheme.warning);
        severity = QStringLiteral("LOW");
    } else {
        bg = QColor(scheme.accentPrimary);
    }

    // Pick text colour by background luminance so it always meets contrast —
    // white on the orange "low" card failed WCAG AA.
    QColor fg;
    if (stock <= 0) {
        fg = QColor(scheme.disabledText);
    } else {
        const double lum = 0.299 * bg.red() + 0.587 * bg.green()
                           + 0.114 * bg.blue();
        fg = lum > 150 ? QColor("#1a1a1a") : QColor(Qt::white);
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
        formatKsh(index.data(ProductGridModel::PriceRole).toDouble());
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

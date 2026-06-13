// =============================================================================
// productgridmodel.h — model/view classes for the POS product grid
// -----------------------------------------------------------------------------
// WHAT: ProductGridModel is a flat QAbstractListModel over the product
//       catalogue; ProductFilterProxy filters it by search text (name or
//       barcode) and category; ProductCardDelegate paints each product as a
//       stock-coloured card (green = healthy, orange = low, red = critical,
//       grey = out of stock).
// HOW:  MainWindow shows the proxy in a QListView in IconMode. Filtering is a
//       proxy invalidation (no widget churn), so search updates per keystroke
//       without a debounce. The delegate reads colours from getColorScheme()
//       at paint time, so a theme toggle recolours the grid automatically.
// WHY:  The grid used to be rebuilt as a wall of QPushButtons on every
//       search/category change and after every checkout — O(products) widget
//       creation per keystroke pause. Model/view makes the catalogue size
//       irrelevant and removes the per-button stylesheet strings.
// =============================================================================
#ifndef PRODUCTGRIDMODEL_H
#define PRODUCTGRIDMODEL_H

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVector>

#include "database.h"   // Product

// ─────────────────────────────────────────────────────────────────────────────
// ProductGridModel
// ─────────────────────────────────────────────────────────────────────────────
class ProductGridModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ProductIdRole = Qt::UserRole + 1,
        NameRole,
        PriceRole,
        StockRole,
        BarcodeRole,
        CategoryRole,
    };

    explicit ProductGridModel(QObject *parent = nullptr);

    void setProducts(const QVector<Product> &products);

    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QVector<Product> m_products;
};

// ─────────────────────────────────────────────────────────────────────────────
// ProductFilterProxy — search text (name/barcode) + category
// ─────────────────────────────────────────────────────────────────────────────
class ProductFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ProductFilterProxy(QObject *parent = nullptr);

    void setSearchText(const QString &text);
    void setCategory(const QString &category);   // "All Categories" / "" = all

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    QString m_search;
    QString m_category;
};

// ─────────────────────────────────────────────────────────────────────────────
// ProductCardDelegate — paints one product card
// ─────────────────────────────────────────────────────────────────────────────
class ProductCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // Thresholds match the rest of the app (POSConfig in mainwindow.cpp):
    // stock 0 = out (grey), <= critical = red, <= medium = orange, else green.
    ProductCardDelegate(int criticalThreshold, int mediumThreshold,
                        QObject *parent = nullptr);

    void  paint(QPainter *painter, const QStyleOptionViewItem &option,
                const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    int m_critical;
    int m_medium;
};

#endif // PRODUCTGRIDMODEL_H

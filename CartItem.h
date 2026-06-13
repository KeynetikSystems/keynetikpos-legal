// =============================================================================
// CartItem.h — One line item in a shopping cart
// -----------------------------------------------------------------------------
// WHAT: Plain value struct for a single cart line: product id, name, selling
//       price, cost price, quantity, category, plus getSubtotal()/getProfit().
// HOW:  Header-only copyable POD with inline arithmetic; no Qt object model,
//       no database access.
// WHY:  Carts are transient UI state, so a lightweight struct avoids any DB
//       coupling. costPrice is carried through the cart so checkout can write
//       profit data into sale_items without re-querying products mid-
//       transaction.
// =============================================================================
#ifndef CART_ITEM_H
#define CART_ITEM_H

#include <QString>

struct CartItem {
    int productId;
    QString name;
    double price;       // Selling price
    double costPrice;   // Cost price
    int quantity;
    QString category;

    CartItem() : productId(0), price(0.0), costPrice(0.0), quantity(0) {}

    CartItem(int id, const QString &n, double p, int q = 1, const QString &cat = "", double cost = 0.0)
        : productId(id), name(n), price(p), costPrice(cost), quantity(q), category(cat) {}

    double getSubtotal() const {
        return price * quantity;
    }

    double getProfit() const {
        return (price - costPrice) * quantity;
    }
};

#endif // CART_ITEM_H

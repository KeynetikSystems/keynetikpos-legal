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
#include "money.h"

struct CartItem {
    int productId;
    QString name;
    Money price;        // Selling price (per unit)
    Money costPrice;    // Cost price (per unit)
    int quantity;
    QString category;

    CartItem() : productId(0), quantity(0) {}

    CartItem(int id, const QString &n, Money p, int q = 1, const QString &cat = "", Money cost = Money())
        : productId(id), name(n), price(p), costPrice(cost), quantity(q), category(cat) {}

    Money getSubtotal() const {
        return price * quantity;
    }

    Money getProfit() const {
        return (price - costPrice) * quantity;
    }
};

#endif // CART_ITEM_H

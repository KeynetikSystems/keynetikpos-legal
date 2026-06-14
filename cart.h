// =============================================================================
// cart.h — Cart domain type + money helpers
// -----------------------------------------------------------------------------
// WHAT: Cart models one open transaction (line items, discount + reason,
//       timestamps, subtotal/item-count helpers). The Money type and the
//       formatMoney()/currency helpers now live in money.h (included here);
//       cart subtotals are Money.
// HOW:  Header-only value struct, no Qt object model, no database access.
//       Multi-cart bookkeeping (which carts exist, which is current) lives in
//       CartService; this type is purely the data.
// WHY:  The domain type used to be declared inside mainwindow.h, which forced
//       every consumer of "a cart" to include the whole main window. Splitting
//       it out lets CartService/CartModel/CheckoutService depend on the data
//       without depending on the UI.
// =============================================================================
#ifndef CART_H
#define CART_H

#include <QString>
#include <QVector>
#include <QDateTime>

#include "money.h"        // Money, currencySymbol(), formatMoney()
#include "CartItem.h"

// -----------------------------------------------------------------------------
// Cart — represents one open shopping cart / transaction
// -----------------------------------------------------------------------------
struct Cart {
    int     id          { 0 };
    QString name;
    QVector<CartItem> items;
    Money   discount;
    QString discountReason;
    QDateTime createdTime;
    QDateTime lastModified;

    Cart() = default;
    Cart(int id_, const QString &name_)
        : id(id_), name(name_)
        , createdTime(QDateTime::currentDateTime())
        , lastModified(QDateTime::currentDateTime())
    {}

    void touch() { lastModified = QDateTime::currentDateTime(); }

    Money getSubtotal() const {
        Money s;
        for (const CartItem &i : items) s += i.getSubtotal();
        return s;
    }

    int getItemCount() const {
        int c = 0;
        for (const CartItem &i : items) c += i.quantity;
        return c;
    }
};

#endif // CART_H

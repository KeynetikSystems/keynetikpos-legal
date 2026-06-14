// =============================================================================
// cartservice.h — CartService: multi-cart state and operations
// -----------------------------------------------------------------------------
// WHAT: Owns every open Cart (create, switch, merge, remove) plus the item
//       operations on the current cart (add/merge line, remove line, edit
//       quantity, clear, apply discount). Emits change signals so the UI
//       (tab bar, CartModel, totals panel) can react.
// HOW:  Carts live in a QMap<int, Cart> keyed by id with a current-cart id.
//       The constructor creates the first cart, and removeCart() refuses to
//       delete the last one, so current() is never null in normal operation.
//       Three signals: cartListChanged (carts created/removed/merged),
//       currentCartChanged (switch), cartContentChanged (items/discount).
// WHY:  Multiple carts mirror real retail — a cashier parks one customer's
//       order while serving another. This state machine used to live inside
//       MainWindow; extracting it gives the cart lifecycle a single owner with
//       no widget dependencies, testable without a UI.
// =============================================================================
#ifndef CARTSERVICE_H
#define CARTSERVICE_H

#include <QObject>
#include <QMap>
#include <QList>

#include "cart.h"

class CartService : public QObject
{
    Q_OBJECT

public:
    explicit CartService(QObject *parent = nullptr);

    // ── Cart lifecycle ──────────────────────────────────────────────────────
    int  createCart(const QString &name = QString());
    bool removeCart(int cartId);            // refuses to remove the last cart
    bool switchTo(int cartId);
    bool mergeInto(int sourceCartId);       // merge source into CURRENT cart

    // ── Accessors ───────────────────────────────────────────────────────────
    int         currentId() const { return m_currentId; }
    int         count()     const { return m_carts.size(); }
    Cart       *current();
    const Cart *cartById(int cartId) const;
    QList<int>  sortedIds() const;

    // ── Current-cart item operations ────────────────────────────────────────
    bool addItem(const CartItem &item);     // returns true if merged into an
                                            // existing line, false if appended
    bool removeItemAt(int index);
    bool setItemQuantity(int index, int quantity);
    void clearCurrent();                    // items + discount
    void setDiscount(Money amount, const QString &reason);

signals:
    void cartListChanged();
    void currentCartChanged(int cartId);
    void cartContentChanged(int cartId);

private:
    QString makeCartName() const;

    QMap<int, Cart> m_carts;
    int m_currentId { 0 };
    int m_nextId    { 1 };
};

#endif // CARTSERVICE_H

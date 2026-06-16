// =============================================================================
// cartservice.cpp — Implementation of CartService (see cartservice.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The constructor creates the first cart so current() is valid from the
//    start; removeCart() refuses to delete the last cart, preserving the
//    "always at least one open cart" invariant the UI relies on.
//  - Auto-generated cart names are just a per-day sequence ("Cart 1", "Cart 2",
//    ...): the Nth cart opened today. The counter persists in QSettings so it
//    keeps counting across restarts within the same day and resets at midnight.
//    The wall-clock time of creation lives in Cart::createdTime (backend record
//    only) — it is not part of the title.
//  - Item mutations emit cartContentChanged; CartModel listens and resets,
//    MainWindow listens and refreshes totals + the current tab label.
// =============================================================================
#include "cartservice.h"

#include <QDate>
#include <QDebug>
#include <QSettings>

CartService::CartService(QObject *parent)
    : QObject(parent)
{
    createCart(QString());   // invariant: at least one cart always exists
}

// =============================================================================
// Cart lifecycle
// =============================================================================

QString CartService::makeCartName() const
{
    // Day-relative sequence number: the Nth cart opened today. Persisted so it
    // survives restarts within the same day; resets when the date rolls over.
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    const QString today   = QDate::currentDate().toString(Qt::ISODate);
    const QString lastDay = settings.value("cart/counterDate").toString();
    const int next = (lastDay == today ? settings.value("cart/counter").toInt() : 0) + 1;
    settings.setValue("cart/counterDate", today);
    settings.setValue("cart/counter", next);
    return QStringLiteral("Cart %1").arg(next);
}

int CartService::createCart(const QString &name)
{
    QString cartName = name.trimmed();
    if (cartName.isEmpty())
        cartName = makeCartName();

    const int cartId = m_nextId++;
    m_carts.insert(cartId, Cart(cartId, cartName));
    if (m_currentId == 0)
        m_currentId = cartId;   // first cart becomes current

    qDebug() << "Created cart:" << cartName << "ID:" << cartId;
    emit cartListChanged();
    return cartId;
}

bool CartService::removeCart(int cartId)
{
    if (!m_carts.contains(cartId) || m_carts.size() <= 1)
        return false;

    m_carts.remove(cartId);

    const bool wasCurrent = (cartId == m_currentId);
    if (wasCurrent)
        m_currentId = m_carts.firstKey();

    emit cartListChanged();
    if (wasCurrent)
        emit currentCartChanged(m_currentId);
    return true;
}

bool CartService::switchTo(int cartId)
{
    if (!m_carts.contains(cartId)) return false;
    if (m_currentId == cartId)     return true;
    m_currentId = cartId;
    emit currentCartChanged(cartId);
    return true;
}

bool CartService::mergeInto(int sourceCartId)
{
    if (sourceCartId == m_currentId)        return false;
    if (!m_carts.contains(sourceCartId))    return false;
    if (!m_carts.contains(m_currentId))     return false;

    const Cart &source = m_carts[sourceCartId];
    Cart &target = m_carts[m_currentId];

    for (const CartItem &item : source.items) {
        bool found = false;
        for (CartItem &ti : target.items) {
            if (ti.productId == item.productId) {
                ti.quantity += item.quantity;
                found = true;
                break;
            }
        }
        if (!found)
            target.items.append(item);
    }
    target.touch();
    m_carts.remove(sourceCartId);

    emit cartListChanged();
    emit cartContentChanged(m_currentId);
    return true;
}

// =============================================================================
// Accessors
// =============================================================================

Cart *CartService::current()
{
    if (!m_carts.contains(m_currentId)) return nullptr;
    return &m_carts[m_currentId];
}

const Cart *CartService::cartById(int cartId) const
{
    const auto it = m_carts.constFind(cartId);
    return it == m_carts.cend() ? nullptr : &it.value();
}

QList<int> CartService::sortedIds() const
{
    return m_carts.keys();   // QMap keys are already sorted ascending
}

// =============================================================================
// Current-cart item operations
// =============================================================================

bool CartService::addItem(const CartItem &item)
{
    Cart *cart = current();
    if (!cart) return false;

    for (CartItem &existing : cart->items) {
        if (existing.productId == item.productId) {
            existing.quantity += item.quantity;
            cart->touch();
            emit cartContentChanged(m_currentId);
            return true;   // merged into existing line
        }
    }

    cart->items.append(item);
    cart->touch();
    emit cartContentChanged(m_currentId);
    return false;          // appended as a new line
}

bool CartService::removeItemAt(int index)
{
    Cart *cart = current();
    if (!cart || index < 0 || index >= cart->items.size())
        return false;

    cart->items.removeAt(index);
    cart->touch();
    emit cartContentChanged(m_currentId);
    return true;
}

bool CartService::setItemQuantity(int index, int quantity)
{
    Cart *cart = current();
    if (!cart || index < 0 || index >= cart->items.size())
        return false;

    cart->items[index].quantity = qMax(1, quantity);
    cart->touch();
    emit cartContentChanged(m_currentId);
    return true;
}

void CartService::clearCurrent()
{
    Cart *cart = current();
    if (!cart) return;

    cart->items.clear();
    cart->discount       = Money();
    cart->discountReason = "";
    cart->touch();
    emit cartContentChanged(m_currentId);
}

void CartService::setDiscount(Money amount, const QString &reason)
{
    Cart *cart = current();
    if (!cart) return;

    cart->discount       = amount;
    cart->discountReason = reason;
    cart->touch();
    emit cartContentChanged(m_currentId);
}

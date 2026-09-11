// =============================================================================
// barcodescan.h — Shared "a barcode arrived -> add it to the cart" logic
// -----------------------------------------------------------------------------
// WHAT: addBarcodeToCart() looks up a product by barcode, validates stock via
//       InventoryManager::canSell(), and adds it to the current cart. Used by
//       both the serial barcode scanner (MainWindow::onBarcodeScanned) and the
//       LAN mobile-scanner API (PosApiServer).
// HOW:  Plain free function, no widget dependency — callers render the
//       BarcodeScanResult however fits them (a toast, an HTTP JSON body, ...).
// WHY:  Before this, stock validation was duplicated: onBarcodeScanned had its
//       own stockQuantity<=0 early-exit ahead of addToCart()'s real canSell()
//       check. Two copies of the same rule is how they drift apart; a second
//       caller (the phone) must see exactly the same stock behaviour as the
//       wired scanner, so this gives the rule one home.
// =============================================================================
#ifndef BARCODESCAN_H
#define BARCODESCAN_H

#include <QString>

#include "database.h"   // Product, Database

class InventoryManager;
class CartService;

struct BarcodeScanResult {
    enum class Status { Added, NotFound, OutOfStock } status;
    Product product;         // populated for Added and OutOfStock
    bool    merged = false;  // meaningful only when status == Added
};

BarcodeScanResult addBarcodeToCart(const QString &barcode, Database &db,
                                    InventoryManager &inventory, CartService &cart);

#endif // BARCODESCAN_H

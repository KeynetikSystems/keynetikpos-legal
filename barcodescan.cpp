#include "barcodescan.h"

#include "productrepository.h"
#include "inventorymanager.h"
#include "cartservice.h"
#include "CartItem.h"

BarcodeScanResult addBarcodeToCart(const QString &barcode, Database &db,
                                    InventoryManager &inventory, CartService &cart)
{
    BarcodeScanResult result;

    const QString code = barcode.trimmed();
    const Product product = db.products().getProductByBarcode(code);

    if (product.id <= 0) {
        result.status = BarcodeScanResult::Status::NotFound;
        return result;
    }

    if (!inventory.canSell(product.id, 1)) {
        result.status  = BarcodeScanResult::Status::OutOfStock;
        result.product = product;
        return result;
    }

    const CartItem ci(product.id, product.name, product.price, 1,
                       product.category, product.costPrice);
    result.status  = BarcodeScanResult::Status::Added;
    result.product = product;
    result.merged  = cart.addItem(ci);
    return result;
}

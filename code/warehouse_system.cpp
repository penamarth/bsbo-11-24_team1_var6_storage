#include "warehouse_system.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

WarehouseSystem::WarehouseSystem() {
    warehouse_ = std::make_unique<Warehouse>(1, "Центральный склад", "г. Москва, ул. Складская, 1");

    auto cabinet = std::make_unique<StorageCabinet>(2, "Шкаф A");
    auto shelf = std::make_unique<Shelf>(3, "Полка A-1");
    auto cell = std::make_unique<StorageCell>(4, "A-1-1", 100.0);
    demoCell_ = cell.get();
    shelf->addCell(std::move(cell));
    cabinet->addShelf(std::move(shelf));
    warehouse_->addCabinet(std::move(cabinet));
    warehouse_->addBufferZone(std::make_unique<BufferZone>(5, "Зона отгрузки", 100.0));

    products_.push_back(std::make_unique<Product>(10, "Молоко 3,2%", "MLK-001", 0.02));
    products_.push_back(std::make_unique<Product>(11, "Мука пшеничная", "FLR-001", 0.05));
}

void WarehouseSystem::printClassChain(const std::string& chain) const {
    std::cout << "\nЦепочка вызовов: " << chain << "\n";
}

void WarehouseSystem::printMenu() const {
    std::cout << "\033[2J\033[H";
    std::cout << "=== Информационная система склада ===\n";
    std::cout << "Введите номер нужного пункта и нажмите Enter.\n\n";

    const std::vector<std::string> items = {
        "Показать структуру склада",
        "Подготовка и приёмка товара",
        "Размещение товара",
        "Создание и комплектация заказа",
        "Инвентаризация",
        "Отгрузка товара",
        "Показать уведомления",
        "Выход"
    };

    for (std::size_t index = 0; index < items.size(); ++index) {
        std::cout << (index + 1) << ". " << items[index] << "\n";
    }
}

int WarehouseSystem::readMenuChoice() const {
    int choice = 0;
    while (true) {
        std::cout << "\nВаш выбор: ";
        if (std::cin >> choice) {
            return choice;
        }

        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Ошибка: введите номер пункта от 1 до 8.\n";
    }
}

void WarehouseSystem::pauseAfterScenario() const {
    std::cout << "\nНажмите Enter, чтобы вернуться в меню...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

Product* WarehouseSystem::getDemoProduct(std::size_t index) const {
    return index < products_.size() ? products_[index].get() : nullptr;
}

void WarehouseSystem::runMenu() {
    while (true) {
        printMenu();
        const int choice = readMenuChoice();

        switch (choice) {
            case 1: showWarehouseStructure(); break;
            case 2: runReceivingScenario(); break;
            case 3: runPlacementScenario(); break;
            case 4: runOrderScenario(); break;
            case 5: runStocktakeScenario(); break;
            case 6: runShipmentScenario(); break;
            case 7: showNotifications(); break;
            case 8: return;
            default:
                std::cout << "Ошибка: выберите пункт от 1 до 8.\n";
                continue;
        }

        pauseAfterScenario();
    }
}

void WarehouseSystem::showWarehouseStructure() const {
    printClassChain("WarehouseSystem -> Warehouse -> StorageCabinet -> Shelf -> StorageCell");
    std::cout << "WarehouseSystem::showWarehouseStructure: вызывается инвентаризация склада.\n";
    warehouse_->inventory();
    warehouse_->getAvailableVolume();
}

void WarehouseSystem::runReceivingScenario() {
    printClassChain("WarehouseSystem -> DeliveryNote -> DeliveryNoteItem -> Warehouse -> StorageCell -> GoodsBatch");
    std::cout << "WarehouseSystem::runReceivingScenario: регистрация и приёмка поставки.\n";

    Product* product = getDemoProduct(0);
    auto note = std::make_shared<DeliveryNote>(nextId_++, "ПН-0001", "14.10.2026", "17.10.2026");
    auto item = std::make_unique<DeliveryNoteItem>(nextId_++, product, 50);
    DeliveryNoteItem* itemPointer = note->addItem(std::move(item));
    note->setStatus(DeliveryNoteStatus::Expected);

    StorageCell* cell = warehouse_->findFreeCell(product->getUnitVolume() * 50);
    if (cell == nullptr) {
        std::cout << "Свободная ячейка не найдена.\n";
        return;
    }

    cell->reserveSpace(product->getUnitVolume() * 50);
    itemPointer->setStorageCell(cell, product->getUnitVolume() * 50);

    auto delivery = std::make_shared<Delivery>(nextId_++, note);
    delivery->arrive();
    itemPointer->setActualQuantity(50);
    itemPointer->calculateDifference();

    demoBatch_ = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * 50,
                                              product->getName(), 50, product);
    demoBatch_->receive();
    cell->releaseSpace(itemPointer->getReservedVolume());
    cell->placeBatch(demoBatch_);
    itemPointer->isQuantityCorrect();
    note->checkQuantity();
    note->setStatus(DeliveryNoteStatus::Completed);
    delivery->accept();
    deliveries_.push_back(delivery);

    auto notification = std::make_unique<Notification>(
        nextId_++, "Поставка " + std::string("ПН-0001") + " принята", "Приёмка", "Кладовщик");
    notification->send();
    notifications_.push_back(std::move(notification));

    std::cout << "Приёмка завершена: партия размещена в ячейке " << cell->getAddress() << ".\n";
}

void WarehouseSystem::runPlacementScenario() {
    printClassChain("WarehouseSystem -> Warehouse -> StorageCabinet -> Shelf -> StorageCell -> GoodsBatch");
    std::cout << "WarehouseSystem::runPlacementScenario: поиск места для новой партии.\n";

    Product* product = getDemoProduct(1);
    auto batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * 20,
                                              product->getName(), 20, product);
    batch->receive();
    warehouse_->placeBatch(batch);
    std::cout << "Размещение партии завершено.\n";
}

void WarehouseSystem::runOrderScenario() {
    printClassChain("WarehouseSystem -> WarehouseOrder -> OrderItem -> GoodsBatch");
    std::cout << "WarehouseSystem::runOrderScenario: создание и комплектация заказа.\n";

    if (!demoBatch_) {
        std::cout << "Сначала выберите приёмку товара.\n";
        return;
    }

    auto order = std::make_unique<WarehouseOrder>(nextId_++);
    Product* product = getDemoProduct(0);
    auto item = std::make_unique<OrderItem>(nextId_++, product, 10);
    OrderItem* itemPointer = order->addItem(std::move(item));

    const int pickedQuantity = std::min(10, demoBatch_->getAvailableQuantity());
    demoBatch_->reserveQuantity(pickedQuantity);
    itemPointer->addBatch(demoBatch_);
    itemPointer->pick(pickedQuantity);
    demoBatch_->decreaseQuantity(pickedQuantity);
    order->complete();
    order->printInfo();

    demoOrder_ = order.get();
    orders_.push_back(std::move(order));
}

void WarehouseSystem::runStocktakeScenario() {
    printClassChain("WarehouseSystem -> StocktakeSession -> StocktakeItem -> StorageCell -> Product");
    std::cout << "WarehouseSystem::runStocktakeScenario: запуск инвентаризации.\n";

    if (!demoBatch_ || demoCell_ == nullptr) {
        std::cout << "Сначала выберите приёмку товара.\n";
        return;
    }

    auto session = std::make_unique<StocktakeSession>(nextId_++);
    session->start();
    auto item = std::make_unique<StocktakeItem>(nextId_++, getDemoProduct(0), demoCell_, demoBatch_->quantity);
    StocktakeItem* itemPointer = session->addItem(std::move(item));
    itemPointer->setActualQuantity(demoBatch_->quantity);
    itemPointer->calculateDifference();
    itemPointer->isCorrect();
    session->isComplete();
    session->finish();
    stocktakes_.push_back(std::move(session));
}

void WarehouseSystem::runShipmentScenario() {
    printClassChain("WarehouseSystem -> Shipment -> ShipmentItem -> WarehouseOrder");
    std::cout << "WarehouseSystem::runShipmentScenario: подготовка отгрузки.\n";

    if (demoOrder_ == nullptr) {
        std::cout << "Сначала выберите создание и комплектацию заказа.\n";
        return;
    }

    auto shipment = std::make_unique<Shipment>(nextId_++, demoOrder_);
    for (const auto& orderItem : demoOrder_->getItems()) {
        shipment->addItem(std::make_unique<ShipmentItem>(
            nextId_++, orderItem->getProduct(), orderItem.get(),
            orderItem->getQuantity(), orderItem->getPickedQuantity()));
    }

    if (shipment->checkReadiness()) {
        shipment->dispatch();
    }
    shipment->printInfo();
    shipments_.push_back(std::move(shipment));
}

void WarehouseSystem::showNotifications() const {
    printClassChain("WarehouseSystem -> Notification");
    if (notifications_.empty()) {
        std::cout << "Уведомлений пока нет.\n";
        return;
    }
    for (const auto& notification : notifications_) notification->printInfo();
}

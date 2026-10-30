#include "warehouse_system.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace {

const char* const kLine = "==================================================";
const char* const kThinLine = "--------------------------------------------------";

std::string formatDate(int daysFromToday) {
    const std::time_t now = std::time(nullptr) + static_cast<std::time_t>(daysFromToday) * 24 * 60 * 60;
    const std::tm* local = std::localtime(&now);
    char buffer[16] = {};
    std::strftime(buffer, sizeof(buffer), "%d.%m.%Y", local);
    return buffer;
}

std::string formatNumber(const std::string& prefix, int number) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%s%04d", prefix.c_str(), number);
    return buffer;
}

std::string formatVolume(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

} // namespace

WarehouseSystem::WarehouseSystem() {
    warehouse_ = std::make_unique<Warehouse>(1, "Центральный склад", "г. Москва, ул. Складская, 1");

    // Шкаф A: две полки по две ячейки.
    auto cabinetA = std::make_unique<StorageCabinet>(2, "A");
    for (int shelfNumber = 1; shelfNumber <= 2; ++shelfNumber) {
        const std::string shelfName = "A-" + std::to_string(shelfNumber);
        auto shelf = std::make_unique<Shelf>(nextId_++, shelfName);
        for (int cellNumber = 1; cellNumber <= 2; ++cellNumber) {
            shelf->addCell(std::make_unique<StorageCell>(
                nextId_++, shelfName + "-" + std::to_string(cellNumber), 1.5));
        }
        cabinetA->addShelf(std::move(shelf));
    }
    warehouse_->addCabinet(std::move(cabinetA));

    // Шкаф B: одна полка с двумя большими ячейками.
    auto cabinetB = std::make_unique<StorageCabinet>(3, "B");
    auto shelfB = std::make_unique<Shelf>(nextId_++, "B-1");
    shelfB->addCell(std::make_unique<StorageCell>(nextId_++, "B-1-1", 3.0));
    shelfB->addCell(std::make_unique<StorageCell>(nextId_++, "B-1-2", 3.0));
    cabinetB->addShelf(std::move(shelfB));
    warehouse_->addCabinet(std::move(cabinetB));

    receivingZone_ = warehouse_->addBufferZone(std::make_unique<BufferZone>(4, "Зона приёмки", 3.0));
    shippingZone_ = warehouse_->addBufferZone(std::make_unique<BufferZone>(5, "Зона отгрузки", 3.0));

    products_.push_back(std::make_unique<Product>(10, "Молоко 3,2%", "MLK-001", 0.02));
    products_.push_back(std::make_unique<Product>(11, "Мука пшеничная 2 кг", "FLR-002", 0.05));
    products_.push_back(std::make_unique<Product>(12, "Сахар 1 кг", "SGR-001", 0.03));

    // Начальные остатки, чтобы любой сценарий можно было показать сразу.
    auto milk = std::make_shared<GoodsBatch>(nextId_++, 0.02 * 30, "Молоко 3,2%", 30, products_[0].get());
    warehouse_->placeBatch(milk);
    auto sugar = std::make_shared<GoodsBatch>(nextId_++, 0.03 * 40, "Сахар 1 кг", 40, products_[2].get());
    warehouse_->placeBatch(sugar);
}

// ---------- Вывод на экран ----------

void WarehouseSystem::printStatusBar() const {
    double capacity = 0.0;
    double used = 0.0;
    double reserved = 0.0;
    int batchCount = 0;
    for (StorageCell* cell : warehouse_->getAllCells()) {
        capacity += cell->getCapacity();
        used += cell->getUsedVolume();
        reserved += cell->getReservedVolume();
        batchCount += static_cast<int>(cell->getBatches().size());
    }

    std::cout << " Ячейки: занято " << formatVolume(used) << " из " << formatVolume(capacity)
              << " м³ (резерв " << formatVolume(reserved) << " м³), партий: " << batchCount << "\n";
    std::cout << " Зона приёмки: " << receivingZone_->getBatches().size()
              << " парт. | Зона отгрузки: " << shippingZone_->getBatches().size() << " парт.\n";
    std::cout << " Заказов всего: " << orders_.size() << " | готовы к отгрузке: " << countReadyOrders()
              << " | новых уведомлений: " << countUnreadNotifications() << "\n";
}

void WarehouseSystem::printMenu() const {
    clearScreen();
    std::cout << kLine << "\n";
    std::cout << "        ИНФОРМАЦИОННАЯ СИСТЕМА СКЛАДА\n";
    std::cout << " " << warehouse_->getName() << ", " << warehouse_->getAddress() << "\n";
    std::cout << kLine << "\n";
    printStatusBar();
    std::cout << kThinLine << "\n";

    std::cout << " 1. Показать структуру склада\n";
    std::cout << " 2. Подготовка и приёмка товара\n";
    std::cout << " 3. Размещение товара";
    if (!receivingZone_->getBatches().empty()) {
        std::cout << "   (ждут размещения: " << receivingZone_->getBatches().size() << ")";
    }
    std::cout << "\n";
    std::cout << " 4. Создание и комплектация заказа\n";
    std::cout << " 5. Инвентаризация и списание\n";
    std::cout << " 6. Отгрузка товара";
    if (countReadyOrders() > 0) {
        std::cout << "   (готово заказов: " << countReadyOrders() << ")";
    }
    std::cout << "\n";
    std::cout << " 7. Показать уведомления";
    if (countUnreadNotifications() > 0) {
        std::cout << "   (новых: " << countUnreadNotifications() << ")";
    }
    std::cout << "\n";
    std::cout << " 8. Выход\n";
    std::cout << kThinLine << "\n";

    if (!menuMessage_.empty()) {
        std::cout << " ! " << menuMessage_ << "\n";
    }
}

void WarehouseSystem::printScenarioTitle(const std::string& title) const {
    clearScreen();
    std::cout << kLine << "\n " << title << "\n" << kLine << "\n";
}

void WarehouseSystem::printClassChain(const std::vector<std::string>& chain) const {
    std::cout << "Цепочка вызовов:\n";
    for (std::size_t index = 0; index < chain.size(); ++index) {
        std::cout << (index == 0 ? "" : " -> ") << chain[index] << "\n";
    }
    std::cout << kThinLine << "\n";
}

void WarehouseSystem::printProducts() const {
    std::cout << "Товары:\n";
    for (std::size_t index = 0; index < products_.size(); ++index) {
        const Product& product = *products_[index];
        std::cout << "  " << (index + 1) << ". " << product.getName() << " (" << product.getSku()
                  << "), " << product.getUnitVolume() << " м³/шт.\n";
    }
}

// ---------- Ввод с клавиатуры ----------

bool WarehouseSystem::readLine(std::string& line) {
    if (inputClosed_ || !std::getline(std::cin, line)) {
        inputClosed_ = true;
        line.clear();
        return false;
    }
    // Убираем пробелы и \r (файлы, набранные в Windows).
    const auto first = line.find_first_not_of(" \t\r");
    const auto last = line.find_last_not_of(" \t\r");
    line = first == std::string::npos ? "" : line.substr(first, last - first + 1);
    return true;
}

int WarehouseSystem::readMenuChoice() {
    std::cout << "Введите номер пункта (1–8) и нажмите Enter: ";
    std::string line;
    if (!readLine(line)) {
        return 8;
    }
    if (line.size() == 1 && line[0] >= '1' && line[0] <= '8') {
        return line[0] - '0';
    }
    menuMessage_ = line.empty() ? "Вы ничего не ввели. Выберите пункт от 1 до 8."
                                : "Пункта «" + line + "» нет. Выберите пункт от 1 до 8.";
    return 0;
}

int WarehouseSystem::readInt(const std::string& prompt, int minValue, int maxValue, int defaultValue) {
    while (true) {
        std::cout << prompt << " [" << minValue << "–" << maxValue
                  << ", Enter = " << defaultValue << "]: ";
        std::string line;
        if (!readLine(line) || line.empty()) {
            if (inputClosed_) std::cout << "\n";
            return defaultValue;
        }

        std::istringstream input(line);
        int value = 0;
        char extra = 0;
        if (input >> value && !(input >> extra) && value >= minValue && value <= maxValue) {
            return value;
        }
        std::cout << "  Ошибка: нужно целое число от " << minValue << " до " << maxValue << ".\n";
    }
}

Product* WarehouseSystem::chooseProduct(int defaultNumber) {
    printProducts();
    const int number = readInt("Номер товара", 1, static_cast<int>(products_.size()), defaultNumber);
    return products_[static_cast<std::size_t>(number - 1)].get();
}

void WarehouseSystem::waitForEnter() {
    if (inputClosed_) {
        return;
    }
    std::cout << "\n" << kThinLine << "\nНажмите Enter, чтобы вернуться в меню...";
    std::string line;
    readLine(line);
}

// ---------- Служебное ----------

void WarehouseSystem::addNotification(const std::string& message, const std::string& type, const std::string& role) {
    auto notification = std::make_unique<Notification>(nextId_++, message, type, role);
    notification->send();
    notifications_.push_back(std::move(notification));
}

int WarehouseSystem::countReadyOrders() const {
    return static_cast<int>(std::count_if(orders_.begin(), orders_.end(),
        [](const std::unique_ptr<WarehouseOrder>& order) { return order->getStatus() == OrderStatus::Completed; }));
}

int WarehouseSystem::countUnreadNotifications() const {
    return static_cast<int>(std::count_if(notifications_.begin(), notifications_.end(),
        [](const std::unique_ptr<Notification>& notification) { return !notification->isRead(); }));
}

// ---------- Меню ----------

void WarehouseSystem::runMenu() {
    while (!inputClosed_) {
        printMenu();
        menuMessage_.clear();
        const int choice = readMenuChoice();

        switch (choice) {
            case 1: showWarehouseStructure(); break;
            case 2: runReceivingScenario(); break;
            case 3: runPlacementScenario(); break;
            case 4: runOrderScenario(); break;
            case 5: runStocktakeScenario(); break;
            case 6: runShipmentScenario(); break;
            case 7: showNotifications(); break;
            case 8:
                std::cout << "\nРабота завершена. До свидания!\n";
                return;
            default:
                continue;   // ошибка ввода — сообщение покажется под меню
        }
        waitForEnter();
    }
}

// ---------- 1. Структура склада ----------

void WarehouseSystem::showWarehouseStructure() const {
    printScenarioTitle("1. Структура склада");
    printClassChain({"WarehouseSystem", "Warehouse", "StorageCabinet", "Shelf", "StorageCell", "GoodsBatch"});
    std::cout << "WarehouseSystem::showWarehouseStructure: вызывается инвентаризация склада.\n\n";
    warehouse_->inventory();
    std::cout << "\n";
    warehouse_->getAvailableVolume();
}

// ---------- 2. Приёмка ----------

void WarehouseSystem::runReceivingScenario() {
    printScenarioTitle("2. Подготовка и приёмка товара");
    printClassChain({"WarehouseSystem", "DeliveryNote", "DeliveryNoteItem", "Warehouse", "StorageCell", "GoodsBatch"});

    const std::string number = formatNumber("ПН-", deliveryCounter_ + 1);
    std::cout << "Накладная " << number << ". Заполните данные (Enter — значение по умолчанию):\n";
    Product* product = chooseProduct(1);
    const int expected = readInt("Количество по накладной, шт.", 1, 500, 50);
    const int actual = readInt("Фактически привезли, шт.", 0, 1000, expected);

    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
    std::cout << "WarehouseSystem::runReceivingScenario: подготовка к приёмке поставки " << number << ".\n";

    // Подготовка: накладная и резерв места под ожидаемый товар.
    ++deliveryCounter_;
    auto note = std::make_shared<DeliveryNote>(nextId_++, number, formatDate(0), formatDate(3));
    DeliveryNoteItem* item = note->addItem(std::make_unique<DeliveryNoteItem>(nextId_++, product, expected));
    note->setStatus(DeliveryNoteStatus::Expected);

    const double expectedVolume = product->getUnitVolume() * expected;
    StorageCell* cell = warehouse_->findFreeCell(expectedVolume);
    if (cell != nullptr && cell->reserveSpace(expectedVolume)) {
        item->setStorageCell(cell, expectedVolume);
    } else {
        cell = nullptr;
        if (!receivingZone_->hasSpace(expectedVolume)) {
            note->setStatus(DeliveryNoteStatus::Created);
            addNotification("Поставка " + number + " отклонена: на складе нет места под " +
                            formatVolume(expectedVolume) + " м³", "Приёмка", "Менеджер");
            std::cout << "\nИтог: поставка " << number << " отклонена — не хватает места ни в ячейках, ни в зоне приёмки.\n";
            return;
        }
        std::cout << "WarehouseSystem::runReceivingScenario: подходящей ячейки нет, товар примем в зону приёмки.\n";
    }

    // Приёмка: поставка прибыла, пересчитываем товар.
    auto delivery = std::make_shared<Delivery>(nextId_++, note);
    deliveries_.push_back(delivery);
    delivery->arrive();
    item->setActualQuantity(actual);
    item->calculateDifference();
    note->setStatus(DeliveryNoteStatus::Received);

    std::string placeText = "товар не поступил";
    if (cell != nullptr) {
        cell->releaseSpace(item->getReservedVolume());
    }
    if (actual > 0) {
        auto batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * actual,
                                                  product->getName(), actual, product);
        batch->receive();
        receivingZone_->placeBatch(batch);       // товар выгрузили в зону приёмки

        bool placed = cell != nullptr && cell->placeBatch(batch);
        if (!placed && cell != nullptr) {
            // Привезли больше, чем ждали, и в назначенную ячейку не влезло — ищем другое место.
            placed = warehouse_->placeBatch(batch);
        }
        placeText = batch->location != nullptr ? batch->location->getTitle() : "место не найдено";
        if (!placed) {
            addNotification("Партия #" + std::to_string(batch->id) + " («" + product->getName() +
                            "») ждёт размещения: " + placeText, "Размещение", "Кладовщик");
        }
    }

    const bool quantityOk = note->checkQuantity();
    if (quantityOk) {
        note->setStatus(DeliveryNoteStatus::Completed);
        delivery->accept();
        addNotification("Поставка " + number + " принята", "Приёмка", "Кладовщик");
    } else {
        note->setStatus(DeliveryNoteStatus::Discrepancy);
        delivery->acceptWithDiscrepancy();
        addNotification("Поставка " + number + " принята с расхождением: ожидалось " +
                        std::to_string(expected) + ", получено " + std::to_string(actual) + " шт.",
                        "Приёмка", "Менеджер");
    }

    std::cout << "\nИтог: поставка " << number << " — " << (quantityOk ? "принята" : "принята с расхождением")
              << "; «" << product->getName() << "» " << actual << " шт.; место: " << placeText << ".\n";
}

// ---------- 3. Размещение ----------

void WarehouseSystem::runPlacementScenario() {
    printScenarioTitle("3. Размещение товара");
    printClassChain({"WarehouseSystem", "Warehouse", "StorageCabinet", "Shelf", "StorageCell", "GoodsBatch"});

    const std::size_t waitingCount = receivingZone_->getBatches().size();
    int mode = 1;
    if (waitingCount > 0) {
        std::cout << "В зоне приёмки ждут размещения партий: " << waitingCount << ".\n";
        std::cout << "  1. Разместить новую партию\n  2. Разместить партии из зоны приёмки\n";
        mode = readInt("Что делаем", 1, 2, 2);
    }

    if (mode == 2) {
        std::cout << "\n" << kThinLine << "\nРабота методов:\n";
        std::cout << "WarehouseSystem::runPlacementScenario: размещаем партии из зоны приёмки.\n";
        // Копия списка: при размещении партии удаляются из зоны приёмки.
        const std::vector<std::shared_ptr<GoodsBatch>> waiting = receivingZone_->getBatches();
        int placedCount = 0;
        for (const auto& batch : waiting) {
            if (warehouse_->placeBatch(batch)) ++placedCount;
        }
        std::cout << "\nИтог: размещено партий " << placedCount << " из " << waiting.size() << ".\n";
        return;
    }

    std::cout << "Новая партия (Enter — значение по умолчанию):\n";
    Product* product = chooseProduct(2);
    const int quantity = readInt("Количество, шт.", 1, 500, 20);

    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
    std::cout << "WarehouseSystem::runPlacementScenario: поиск места для новой партии.\n";
    auto batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * quantity,
                                              product->getName(), quantity, product);
    batch->receive();
    receivingZone_->placeBatch(batch);
    const bool placed = warehouse_->placeBatch(batch);

    const std::string placeText = batch->location != nullptr ? batch->location->getTitle() : "место не найдено";
    if (!placed) {
        addNotification("Партия #" + std::to_string(batch->id) + " не размещена: " + placeText,
                        "Размещение", "Кладовщик");
    }
    std::cout << "\nИтог: партия #" << batch->id << " («" << product->getName() << "», " << quantity
              << " шт.) — " << placeText << ".\n";
}

// ---------- 4. Заказ ----------

void WarehouseSystem::runOrderScenario() {
    printScenarioTitle("4. Создание и комплектация заказа");
    printClassChain({"WarehouseSystem", "WarehouseOrder", "OrderItem", "GoodsBatch"});

    std::cout << "Новый заказ (Enter — значение по умолчанию):\n";
    Product* product = chooseProduct(1);
    const int quantity = readInt("Количество, шт.", 1, 1000, 10);

    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
    std::cout << "WarehouseSystem::runOrderScenario: создание и комплектация заказа.\n";

    auto order = std::make_unique<WarehouseOrder>(++orderCounter_);
    OrderItem* item = order->addItem(std::make_unique<OrderItem>(nextId_++, product, quantity));
    order->startPicking();

    // Определяем, где лежит товар, и отбираем его из партий по очереди.
    int need = quantity;
    for (const auto& batch : warehouse_->findBatches(product->getName())) {
        if (need == 0) break;
        const int take = std::min(need, batch->getAvailableQuantity());
        if (take <= 0 || !batch->reserveQuantity(take)) continue;

        item->addBatch(batch);
        item->pick(take);
        batch->decreaseQuantity(take, true);
        if (batch->quantity == 0 && batch->location != nullptr) {
            batch->location->removeBatch(batch.get());   // партия закончилась — освобождаем ячейку
        }
        need -= take;
    }

    const int picked = item->getPickedQuantity();
    if (picked > 0) {
        // Собранный товар переносим в зону отгрузки отдельной партией.
        auto pickedBatch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * picked,
                                                        product->getName(), picked, product);
        shippingZone_->placeBatch(pickedBatch);
        item->setPickedBatch(pickedBatch);
    }
    order->complete();
    order->printInfo();

    const std::string orderName = "Заказ #" + std::to_string(order->getId());
    if (order->getStatus() == OrderStatus::Completed) {
        addNotification(orderName + " укомплектован и ждёт отгрузки", "Заказ", "Кладовщик");
    } else {
        addNotification(orderName + " собран частично: " + std::to_string(picked) + " из " +
                        std::to_string(quantity) + " шт. «" + product->getName() + "»", "Заказ", "Менеджер");
    }

    std::cout << "\nИтог: " << orderName << " — " << WarehouseOrder::statusText(order->getStatus())
              << ", собрано " << picked << " из " << quantity << " шт.\n";
    orders_.push_back(std::move(order));
}

// ---------- 5. Инвентаризация ----------

void WarehouseSystem::runStocktakeScenario() {
    printScenarioTitle("5. Инвентаризация и списание");
    printClassChain({"WarehouseSystem", "StocktakeSession", "StocktakeItem", "StorageCell", "Product"});

    // Что лежит в ячейках по данным учёта.
    struct CheckLine {
        StorageCell* cell;
        std::shared_ptr<GoodsBatch> batch;
        int actual;
    };
    std::vector<CheckLine> lines;
    for (StorageCell* cell : warehouse_->getAllCells()) {
        for (const auto& batch : cell->getBatches()) lines.push_back({cell, batch, batch->quantity});
    }
    if (lines.empty()) {
        std::cout << "В ячейках нет товара — проверять нечего.\n";
        return;
    }

    std::cout << "Пересчитайте товар. Enter — количество совпадает с учётом.\n"
              << "Меньше учёта — недостача будет списана, больше — излишек оприходован.\n";
    for (auto& line : lines) {
        line.actual = readInt("  " + line.cell->getAddress() + ", «" + line.batch->productName +
                              "», по учёту " + std::to_string(line.batch->quantity) + " шт.",
                              0, 10000, line.batch->quantity);
    }

    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
    std::cout << "WarehouseSystem::runStocktakeScenario: запуск инвентаризации.\n";
    auto session = std::make_unique<StocktakeSession>(++stocktakeCounter_);
    session->start();

    int differences = 0;
    for (const auto& line : lines) {
        StocktakeItem* item = session->addItem(std::make_unique<StocktakeItem>(
            nextId_++, line.batch->product, line.cell, line.batch->quantity));
        item->setActualQuantity(line.actual);
        const int difference = item->calculateDifference();
        if (difference == 0) continue;

        ++differences;
        const std::string what = difference < 0
            ? "недостача " + std::to_string(-difference) + " шт. списана"
            : "излишек " + std::to_string(difference) + " шт. оприходован";
        std::cout << "WarehouseSystem::runStocktakeScenario: " << line.cell->getAddress() << " — " << what << ".\n";
        line.batch->changeQuantity(line.actual);
        if (line.actual == 0) {
            line.cell->removeBatch(line.batch.get());
        }
        addNotification("Инвентаризация, " + line.cell->getAddress() + ", «" + line.batch->productName +
                        "»: " + what, "Инвентаризация", "Менеджер");
    }

    session->isComplete();
    session->finish();
    session->printInfo();
    stocktakes_.push_back(std::move(session));

    std::cout << "\nИтог: проверено позиций " << lines.size() << ", расхождений " << differences << ".\n";
}

// ---------- 6. Отгрузка ----------

void WarehouseSystem::runShipmentScenario() {
    printScenarioTitle("6. Отгрузка товара");
    printClassChain({"WarehouseSystem", "Shipment", "ShipmentItem", "WarehouseOrder"});

    std::vector<WarehouseOrder*> candidates;
    int defaultNumber = 0;
    for (const auto& order : orders_) {
        if (order->getStatus() == OrderStatus::Shipped) continue;
        candidates.push_back(order.get());
        if (defaultNumber == 0 && order->getStatus() == OrderStatus::Completed) {
            defaultNumber = static_cast<int>(candidates.size());
        }
    }
    if (candidates.empty()) {
        std::cout << "Нет заказов для отгрузки. Сначала создайте заказ (пункт 4).\n";
        return;
    }

    std::cout << "Заказы, которые ещё не отгружены:\n";
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const WarehouseOrder* order = candidates[index];
        std::cout << "  " << (index + 1) << ". Заказ #" << order->getId() << " — "
                  << WarehouseOrder::statusText(order->getStatus()) << ":";
        for (const auto& item : order->getItems()) {
            std::cout << " «" << item->getProduct()->getName() << "» " << item->getPickedQuantity()
                      << " из " << item->getQuantity() << " шт.";
        }
        std::cout << "\n";
    }
    WarehouseOrder* order = candidates[static_cast<std::size_t>(
        readInt("Номер в списке", 1, static_cast<int>(candidates.size()), std::max(defaultNumber, 1)) - 1)];

    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
    std::cout << "WarehouseSystem::runShipmentScenario: подготовка отгрузки заказа #" << order->getId() << ".\n";
    auto shipment = std::make_unique<Shipment>(++shipmentCounter_, order);
    for (const auto& orderItem : order->getItems()) {
        shipment->addItem(std::make_unique<ShipmentItem>(
            nextId_++, orderItem->getProduct(), orderItem.get(),
            orderItem->getQuantity(), orderItem->getPickedQuantity()));
    }

    const bool shipped = shipment->dispatch();
    shipment->printInfo();
    const std::string orderName = "Заказ #" + std::to_string(order->getId());
    if (shipped) {
        addNotification(orderName + " отгружен", "Отгрузка", "Менеджер");
    } else {
        addNotification(orderName + " нельзя отгрузить: собран не полностью", "Отгрузка", "Менеджер");
    }
    shipments_.push_back(std::move(shipment));

    std::cout << "\nИтог: " << orderName << (shipped ? " отгружен." : " не отгружен — заказ собран не полностью.") << "\n";
}

// ---------- 7. Уведомления ----------

void WarehouseSystem::showNotifications() {
    printScenarioTitle("7. Уведомления");
    printClassChain({"WarehouseSystem", "Notification"});
    if (notifications_.empty()) {
        std::cout << "Уведомлений пока нет.\n";
        return;
    }
    std::cout << "* — новое уведомление\n\n";
    for (const auto& notification : notifications_) {
        notification->printInfo();
        notification->markAsRead();
    }
}

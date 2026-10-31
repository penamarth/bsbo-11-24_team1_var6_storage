#include "warehouse_system.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace {

const char* const kLine = "==================================================";
const char* const kThinLine = "--------------------------------------------------";

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

    // Буферные зоны: склад их использует, но не владеет ими (агрегация).
    bufferZones_.push_back(std::make_unique<BufferZone>(4, "Зона приёмки", 8.0));
    bufferZones_.push_back(std::make_unique<BufferZone>(5, "Зона отгрузки", 4.0));
    receivingZone_ = bufferZones_[0].get();
    shippingZone_ = bufferZones_[1].get();
    warehouse_->addBufferZone(receivingZone_);
    warehouse_->addBufferZone(shippingZone_);

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
        used += cell->getCurrentVolume();
        reserved += cell->getReservedVolume();
        batchCount += static_cast<int>(cell->getBatches().size());
    }

    std::cout << " Ячейки: занято " << formatVolume(used) << " из " << formatVolume(capacity)
              << " м³ (резерв " << formatVolume(reserved) << " м³), партий: " << batchCount << "\n";
    std::cout << " Зона приёмки: " << receivingZone_->getBatches().size()
              << " парт. | Зона отгрузки: " << shippingZone_->getBatches().size() << " парт.\n";
    std::cout << " Поставок ожидается: " << getExpectedNotes().size()
              << " | заказов: " << orders_.size()
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

    const std::size_t expected = getExpectedNotes().size();
    const std::size_t waiting = receivingZone_->getBatches().size();
    const std::size_t picking = getOrdersWithStatus(OrderStatus::Picking).size();
    const std::size_t ready = getOrdersWithStatus(OrderStatus::Completed).size();
    const int unread = countUnreadNotifications();

    std::cout << " 1. Показать структуру склада\n";
    std::cout << " 2. Подготовка и приёмка товара";
    if (expected > 0) std::cout << "   (ожидается поставок: " << expected << ")";
    std::cout << "\n 3. Размещение товара";
    if (waiting > 0) std::cout << "   (ждут размещения: " << waiting << ")";
    std::cout << "\n 4. Создание и комплектация заказа";
    if (picking > 0) std::cout << "   (не собрано заказов: " << picking << ")";
    std::cout << "\n 5. Инвентаризация";
    std::cout << "\n 6. Отгрузка товара";
    if (ready > 0) std::cout << "   (готово заказов: " << ready << ")";
    std::cout << "\n 7. Показать уведомления";
    if (unread > 0) std::cout << "   (новых: " << unread << ")";
    std::cout << "\n 8. Выход\n";
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

void WarehouseSystem::printMethodsHeader() const {
    std::cout << "\n" << kThinLine << "\nРабота методов:\n";
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
        std::cout << prompt << " [" << minValue << "…" << maxValue
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
    const int number = readInt("  Номер товара", 1, static_cast<int>(products_.size()), defaultNumber);
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

void WarehouseSystem::addNotification(std::unique_ptr<Notification> notification) {
    notification->send();
    notifications_.push_back(std::move(notification));
}

void WarehouseSystem::addNotification(const std::string& message, const std::string& type, const std::string& role) {
    addNotification(std::make_unique<Notification>(nextId_++, message, type, role));
}

std::vector<std::shared_ptr<DeliveryNote>> WarehouseSystem::getExpectedNotes() const {
    std::vector<std::shared_ptr<DeliveryNote>> result;
    for (const auto& note : deliveryNotes_) {
        if (note->getStatus() == DeliveryNoteStatus::Expected) result.push_back(note);
    }
    return result;
}

std::vector<WarehouseOrder*> WarehouseSystem::getOrdersWithStatus(OrderStatus status) const {
    std::vector<WarehouseOrder*> result;
    for (const auto& order : orders_) {
        if (order->getStatus() == status) result.push_back(order.get());
    }
    return result;
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
            case 2: deliveryMenu(); break;
            case 3: placeGoods(); break;
            case 4: orderMenu(); break;
            case 5: startStocktake(); break;
            case 6: createShipment(); break;
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

// ---------- 2. Подготовка и приёмка ----------

void WarehouseSystem::deliveryMenu() {
    printScenarioTitle("2. Подготовка и приёмка товара");
    const std::size_t expected = getExpectedNotes().size();
    std::cout << "  1. Зарегистрировать поставку (оператор)\n";
    std::cout << "  2. Принять поставку (кладовщик)   [ожидается: " << expected << "]\n";
    std::cout << "  3. Проверить сроки поставок\n";
    const int choice = readInt("Что делаем", 1, 3, expected > 0 ? 2 : 1);
    std::cout << "\n";

    if (choice == 1) registerDelivery();
    else if (choice == 2) receiveDelivery();
    else checkDeliveryDates();
}

void WarehouseSystem::registerDelivery() {
    printClassChain({"WarehouseSystem", "DeliveryNote", "DeliveryNoteItem", "Warehouse", "StorageCell"});

    const std::string number = formatNumber("ПН-", deliveryCounter_ + 1);
    std::cout << "Регистрация поставки " << number << " (Enter — значение по умолчанию).\n";
    printProducts();
    const int positions = readInt("Сколько позиций в накладной", 1, 3, 1);
    std::vector<std::pair<Product*, int>> lines;
    for (int index = 0; index < positions; ++index) {
        std::cout << "Позиция " << (index + 1) << ":\n";
        Product* product = chooseProduct(index + 1);
        const int quantity = readInt("  Ожидаемое количество, шт.", 1, 300, 50);
        lines.emplace_back(product, quantity);
    }
    const int days = readInt("Срок поставки — через сколько дней", 0, 30, 3);

    printMethodsHeader();
    std::cout << "WarehouseSystem::registerDelivery: регистрация поставки " << number << ".\n";
    ++deliveryCounter_;
    auto note = std::make_shared<DeliveryNote>(nextId_++, number, "приходная", currentDate(days));

    std::string prepared;
    for (const auto& line : lines) {
        Product* product = line.first;
        DeliveryNoteItem* item = note->addItem(std::make_unique<DeliveryNoteItem>(nextId_++, product, line.second));
        const double volume = product->getUnitVolume() * line.second;

        StorageCell* cell = warehouse_->findFreeCell(volume);
        if (cell != nullptr && cell->reserveSpace(volume)) {
            item->setStorageCell(cell, volume);
            prepared += (prepared.empty() ? "" : ", ") + cell->getAddress();
        } else {
            addNotification(note->createNotification(
                nextId_++, "нет места для «" + product->getName() + "» (" + formatVolume(volume) + " м³)",
                "Оператор"));
        }
    }
    note->setStatus(DeliveryNoteStatus::Expected);
    deliveryNotes_.push_back(note);

    if (!prepared.empty()) {
        addNotification(note->createNotification(nextId_++, "подготовлены ячейки " + prepared, "Оператор"));
    }
    std::cout << "\nИтог: поставка " << number << " зарегистрирована, срок " << note->getDueDate()
              << "; подготовлены ячейки: " << (prepared.empty() ? "нет" : prepared) << ".\n";
}

void WarehouseSystem::receiveDelivery() {
    printClassChain({"WarehouseSystem", "DeliveryNote", "DeliveryNoteItem", "Delivery", "GoodsBatch", "StorageCell"});

    const std::vector<std::shared_ptr<DeliveryNote>> expected = getExpectedNotes();
    if (expected.empty()) {
        std::cout << "Нет поставок, ожидающих приёмки. Сначала зарегистрируйте поставку.\n";
        return;
    }

    std::cout << "Поставки, ожидающие приёмки:\n";
    for (std::size_t index = 0; index < expected.size(); ++index) {
        std::cout << "  " << (index + 1) << ". " << expected[index]->getNumber()
                  << ", срок " << expected[index]->getDueDate() << ":";
        for (const auto& item : expected[index]->getItems()) {
            std::cout << " «" << item->getProduct()->getName() << "» " << item->getExpectedQuantity() << " шт.;";
        }
        std::cout << "\n";
    }
    const std::shared_ptr<DeliveryNote> note = expected[static_cast<std::size_t>(
        readInt("Номер в списке", 1, static_cast<int>(expected.size()), 1) - 1)];

    // Кладовщик вводит фактическое количество; при расхождении оператор решает, что делать.
    std::cout << "Введите фактическое количество (Enter — совпадает с накладной):\n";
    struct Decision {
        int actual;
        bool accept;
    };
    std::vector<Decision> decisions;
    for (const auto& item : note->getItems()) {
        const int actual = readInt("  «" + item->getProduct()->getName() + "», по накладной " +
                                   std::to_string(item->getExpectedQuantity()) + " шт. — привезли",
                                   0, 1000, item->getExpectedQuantity());
        bool accept = true;
        if (actual != item->getExpectedQuantity()) {
            accept = readInt("    Расхождение! 1 — принять с расхождением, 2 — отклонить позицию", 1, 2, 1) == 1;
        }
        decisions.push_back({actual, accept});
    }

    printMethodsHeader();
    std::cout << "WarehouseSystem::receiveDelivery: приёмка по накладной " << note->getNumber() << ".\n";
    auto delivery = std::make_shared<Delivery>(nextId_++, note);
    deliveries_.push_back(delivery);
    delivery->arrive();

    int batchesWaiting = 0;
    for (std::size_t index = 0; index < note->getItems().size(); ++index) {
        DeliveryNoteItem* item = note->getItems()[index].get();
        const Decision& decision = decisions[index];
        Product* product = item->getProduct();

        item->setActualQuantity(decision.actual);
        const int difference = item->calculateDifference();
        StorageCell* cell = item->getStorageCell();
        if (cell != nullptr) {
            cell->releaseSpace(item->getReservedVolume());   // резерв снимается после приёмки
        }

        if (!decision.accept) {
            item->reject();
            addNotification(note->createNotification(
                nextId_++, "позиция «" + product->getName() + "» отклонена", "Оператор"));
            continue;
        }
        if (difference != 0) {
            addNotification(note->createNotification(
                nextId_++, "расхождение по «" + product->getName() + "»: ожидалось " +
                    std::to_string(item->getExpectedQuantity()) + ", получено " +
                    std::to_string(decision.actual) + " шт.", "Кладовщик"));
        }
        if (decision.actual == 0) {
            continue;
        }

        // Партия создаётся по фактическому количеству и ждёт размещения в зоне приёмки.
        auto batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * decision.actual,
                                                  product->getName(), decision.actual, product);
        batch->receive();
        if (receivingZone_->placeBatch(batch)) {
            ++batchesWaiting;
        } else if (!warehouse_->placeBatch(batch)) {
            item->reject();
            addNotification(note->createNotification(
                nextId_++, "нет места для «" + product->getName() + "», позиция не принята", "Оператор"));
        }
    }

    const bool quantityOk = note->checkQuantity();
    if (quantityOk) {
        note->setStatus(DeliveryNoteStatus::Completed);
        delivery->accept();
        addNotification(note->createNotification(nextId_++, "принята", "Оператор"));
    } else {
        note->setStatus(DeliveryNoteStatus::Discrepancy);
        delivery->acceptWithDiscrepancy();
        addNotification(note->createNotification(nextId_++, "принята с расхождением", "Оператор"));
    }
    if (batchesWaiting > 0) {
        addNotification("Партий в зоне приёмки ждут размещения: " + std::to_string(batchesWaiting),
                        "Размещение", "Кладовщик");
    }

    std::cout << "\nИтог: поставка " << note->getNumber() << " — "
              << (quantityOk ? "принята" : "принята с расхождением")
              << "; партий ждут размещения (пункт 3): " << batchesWaiting << ".\n";
}

void WarehouseSystem::checkDeliveryDates() {
    printClassChain({"WarehouseSystem", "DeliveryNote", "Notification"});
    std::cout << "WarehouseSystem::checkDeliveryDates: проверка сроков ожидаемых поставок.\n";

    const std::vector<std::shared_ptr<DeliveryNote>> expected = getExpectedNotes();
    if (expected.empty()) {
        std::cout << "\nИтог: ожидаемых поставок нет.\n";
        return;
    }

    const int today = dateKey(currentDate());
    int overdue = 0;
    for (const auto& note : expected) {
        note->printInfo();
        const int due = dateKey(note->getDueDate());
        if (due < today) {
            ++overdue;
            addNotification(note->createNotification(nextId_++, "срок поставки " + note->getDueDate() +
                                                     " истёк", "Менеджер"));
        } else if (due == today) {
            addNotification(note->createNotification(nextId_++, "ожидается сегодня", "Оператор"));
        } else {
            std::cout << "WarehouseSystem::checkDeliveryDates: " << note->getNumber() << " — срок не истёк.\n";
        }
    }
    std::cout << "\nИтог: ожидается поставок " << expected.size() << ", просрочено " << overdue << ".\n";
}

// ---------- 3. Размещение ----------

void WarehouseSystem::placeGoods() {
    printScenarioTitle("3. Размещение товара");
    printClassChain({"WarehouseSystem", "Warehouse", "StorageCabinet", "Shelf", "StorageCell", "GoodsBatch"});

    // Копия списка: при размещении партии удаляются из зоны приёмки.
    const std::vector<std::shared_ptr<GoodsBatch>> waiting = receivingZone_->getBatches();
    if (waiting.empty()) {
        std::cout << "Нет партий, ожидающих размещения. Сначала примите поставку (пункт 2).\n";
        return;
    }

    std::cout << "Партии в зоне приёмки:\n";
    for (std::size_t index = 0; index < waiting.size(); ++index) {
        std::cout << "  " << (index + 1) << ". Партия #" << waiting[index]->id << " «"
                  << waiting[index]->productName << "», " << waiting[index]->quantity << " шт., "
                  << formatVolume(waiting[index]->volume) << " м³\n";
    }
    std::cout << "  0. Разместить все\n";
    const int choice = readInt("Какую партию размещаем", 0, static_cast<int>(waiting.size()), 0);

    printMethodsHeader();
    int placed = 0;
    int total = 0;
    for (std::size_t index = 0; index < waiting.size(); ++index) {
        if (choice != 0 && static_cast<std::size_t>(choice - 1) != index) continue;
        const std::shared_ptr<GoodsBatch>& batch = waiting[index];
        ++total;

        std::cout << "WarehouseSystem::placeGoods: размещение партии #" << batch->id << ".\n";
        StorageCell* cell = warehouse_->findFreeCell(batch->volume);
        if (cell != nullptr && cell->getAvailableVolume() > 0.0 && cell->placeBatch(batch)) {
            ++placed;
            std::cout << "WarehouseSystem::placeGoods: кладовщик подтвердил размещение в ячейке "
                      << cell->getAddress() << ".\n";
        } else {
            addNotification("Нет места для партии #" + std::to_string(batch->id) + " («" + batch->productName +
                            "»), она остаётся в зоне приёмки", "Размещение", "Кладовщик");
        }
    }
    std::cout << "\nИтог: размещено партий " << placed << " из " << total << ".\n";
}

// ---------- 4. Заказ ----------

void WarehouseSystem::orderMenu() {
    printScenarioTitle("4. Создание и комплектация заказа");
    const std::vector<WarehouseOrder*> picking = getOrdersWithStatus(OrderStatus::Picking);
    if (picking.empty()) {
        createOrder();
        return;
    }

    std::cout << "  1. Создать новый заказ\n";
    for (std::size_t index = 0; index < picking.size(); ++index) {
        std::cout << "  " << (index + 2) << ". Дособрать заказ #" << picking[index]->getId() << ":";
        for (const auto& item : picking[index]->getItems()) {
            std::cout << " «" << item->getProduct()->getName() << "» " << item->getPickedQuantity()
                      << " из " << item->getQuantity() << " шт.;";
        }
        std::cout << "\n";
    }
    const int choice = readInt("Что делаем", 1, static_cast<int>(picking.size()) + 1, 1);
    std::cout << "\n";
    if (choice == 1) {
        createOrder();
        return;
    }
    printClassChain({"WarehouseSystem", "WarehouseOrder", "OrderItem", "GoodsBatch", "StorageCell"});
    printMethodsHeader();
    pickOrder(picking[static_cast<std::size_t>(choice - 2)]);
}

void WarehouseSystem::createOrder() {
    printClassChain({"WarehouseSystem", "WarehouseOrder", "OrderItem", "GoodsBatch", "StorageCell"});

    std::cout << "Новый заказ (Enter — значение по умолчанию).\n";
    printProducts();
    const int positions = readInt("Сколько позиций в заказе", 1, 3, 1);
    std::vector<std::pair<Product*, int>> lines;
    for (int index = 0; index < positions; ++index) {
        std::cout << "Позиция " << (index + 1) << ":\n";
        Product* product = chooseProduct(index + 1);
        const int quantity = readInt("  Количество, шт.", 1, 1000, 10);
        lines.emplace_back(product, quantity);
    }

    printMethodsHeader();
    std::cout << "WarehouseSystem::createOrder: создание заказа.\n";
    auto order = std::make_unique<WarehouseOrder>(++orderCounter_);
    for (const auto& line : lines) {
        order->addItem(std::make_unique<OrderItem>(nextId_++, line.first, line.second));
    }
    WarehouseOrder* orderPointer = order.get();
    orders_.push_back(std::move(order));
    pickOrder(orderPointer);
}

void WarehouseSystem::pickOrder(WarehouseOrder* order) {
    std::cout << "WarehouseSystem::pickOrder: комплектация заказа #" << order->getId() << ".\n";
    order->startPicking();

    int pickedNow = 0;
    for (const auto& item : order->getItems()) {
        Product* product = item->getProduct();
        int need = item->getQuantity() - item->getPickedQuantity();
        if (need <= 0) continue;

        // Определяем, где лежит товар, и отбираем его из партий по очереди.
        for (const auto& batch : warehouse_->findBatches(product->getName())) {
            if (need == 0) break;
            const int take = std::min(need, batch->getAvailableQuantity());
            if (take <= 0 || !batch->reserveQuantity(take)) continue;

            std::cout << "WarehouseSystem::pickOrder: задание кладовщику — взять " << take << " шт., место: "
                      << batch->location->getTitle() << ".\n";
            item->addBatch(batch);
            item->pick(take);
            batch->decreaseQuantity(take, true);
            if (batch->quantity == 0) {
                batch->location->removeBatch(batch.get());   // партия закончилась — ячейка освобождается
            }
            need -= take;
            pickedNow += take;
        }
        if (need > 0) {
            addNotification("Заказ #" + std::to_string(order->getId()) + ": не хватает " +
                            std::to_string(need) + " шт. «" + product->getName() + "»", "Заказ", "Оператор");
        }
    }

    const std::string orderName = "Заказ #" + std::to_string(order->getId());
    if (order->checkCompleteness()) {
        order->complete();
        addNotification(orderName + " укомплектован", "Заказ", "Оператор");
    } else {
        addNotification(orderName + " не укомплектован", "Заказ", "Оператор");
    }
    order->printInfo();

    std::cout << "\nИтог: " << orderName << " — " << WarehouseOrder::statusText(order->getStatus())
              << ", сейчас отобрано " << pickedNow << " шт.\n";
}

// ---------- 5. Инвентаризация ----------

void WarehouseSystem::startStocktake() {
    printScenarioTitle("5. Инвентаризация");
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

    std::cout << "Пересчитайте товар. Enter — совпадает с учётом, -1 — ячейку проверить не удалось.\n"
              << "Недостача списывается, излишек оприходуется.\n";
    for (auto& line : lines) {
        line.actual = readInt("  " + line.cell->getAddress() + ", «" + line.batch->productName +
                              "», по учёту " + std::to_string(line.batch->quantity) + " шт.",
                              -1, 10000, line.batch->quantity);
    }

    printMethodsHeader();
    std::cout << "WarehouseSystem::startStocktake: запуск инвентаризации.\n";
    auto session = std::make_unique<StocktakeSession>(++stocktakeCounter_);
    session->start();

    int differences = 0;
    int skipped = 0;
    for (const auto& line : lines) {
        StocktakeItem* item = session->addItem(std::make_unique<StocktakeItem>(
            nextId_++, line.batch->product, line.cell, line.batch->quantity));
        if (line.actual < 0) {
            item->markForRecheck();
            ++skipped;
            continue;
        }
        item->setActualQuantity(line.actual);
        if (item->calculateDifference() == 0) continue;

        ++differences;
        addNotification(item->createNotification(nextId_++));
        std::cout << "WarehouseSystem::startStocktake: " << line.cell->getAddress() << " — "
                  << (item->getDifference() < 0 ? "недостача списана" : "излишек оприходован") << ".\n";
        line.batch->changeQuantity(line.actual);
        if (line.actual == 0) {
            line.cell->removeBatch(line.batch.get());
        }
    }

    const bool complete = session->isComplete();
    if (complete) {
        session->finish();
        addNotification("Инвентаризация #" + std::to_string(stocktakeCounter_) + " завершена, расхождений: " +
                        std::to_string(differences), "Инвентаризация", "Оператор");
    } else {
        addNotification("Инвентаризация #" + std::to_string(stocktakeCounter_) +
                        " не завершена: есть непроверенные ячейки", "Инвентаризация", "Оператор");
    }
    session->printInfo();
    stocktakes_.push_back(std::move(session));

    std::cout << "\nИтог: позиций " << lines.size() << ", проверено " << lines.size() - skipped
              << ", расхождений " << differences
              << (complete ? ", инвентаризация завершена.\n" : ", инвентаризация НЕ завершена.\n");
}

// ---------- 6. Отгрузка ----------

void WarehouseSystem::createShipment() {
    printScenarioTitle("6. Отгрузка товара");
    printClassChain({"WarehouseSystem", "WarehouseOrder", "Shipment", "ShipmentItem", "GoodsBatch", "BufferZone"});

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
                      << " из " << item->getQuantity() << " шт.;";
        }
        std::cout << "\n";
    }
    WarehouseOrder* order = candidates[static_cast<std::size_t>(
        readInt("Номер в списке", 1, static_cast<int>(candidates.size()), std::max(defaultNumber, 1)) - 1)];

    printMethodsHeader();
    const std::string orderName = "Заказ #" + std::to_string(order->getId());
    std::cout << "WarehouseSystem::createShipment: оформление отгрузки, " << orderName << ".\n";

    if (!order->checkCompleteness()) {
        addNotification(orderName + " нельзя отгрузить: заказ не укомплектован", "Отгрузка", "Менеджер");
        std::cout << "\nИтог: " << orderName << " не отгружен — сначала дособерите его (пункт 4).\n";
        return;
    }

    auto shipment = std::make_unique<Shipment>(++shipmentCounter_, order);
    for (const auto& orderItem : order->getItems()) {
        Product* product = orderItem->getProduct();
        const int picked = orderItem->getPickedQuantity();

        // Собранный товар оформляется партией и переносится в зону отгрузки.
        std::shared_ptr<GoodsBatch> batch = orderItem->getPickedBatch();
        if (!batch) {
            batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * picked,
                                                 product->getName(), picked, product);
            batch->reserveQuantity(picked);
            orderItem->setPickedBatch(batch);
        }
        if (batch->location != shippingZone_ && !shippingZone_->placeBatch(batch)) {
            addNotification("В зоне отгрузки нет места для «" + product->getName() + "»", "Отгрузка", "Оператор");
        }
        shipment->addItem(std::make_unique<ShipmentItem>(nextId_++, product, orderItem.get(),
                                                         orderItem->getQuantity(), picked));
    }

    bool shipped = false;
    if (shipment->checkReadiness()) {
        const bool confirmed =
            readInt("Оператор: подтвердить отгрузку? 1 — да, 2 — нет", 1, 2, 1) == 1;
        if (confirmed) {
            shipped = shipment->dispatch();
            addNotification(orderName + " отгружен", "Отгрузка", "Менеджер");
        } else {
            std::cout << "WarehouseSystem::createShipment: отгрузка не подтверждена, товар остаётся в зоне отгрузки.\n";
        }
    } else {
        for (const auto& item : shipment->getItems()) {
            if (!item->isCorrect()) addNotification(item->createNotification(nextId_++));
        }
    }
    shipment->printInfo();
    shipments_.push_back(std::move(shipment));

    std::cout << "\nИтог: " << orderName << (shipped ? " отгружен." : " не отгружен.") << "\n";
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

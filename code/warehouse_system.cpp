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
        const bool waiting = note->getStatus() == DeliveryNoteStatus::Expected ||
                             note->getStatus() == DeliveryNoteStatus::Created;
        if (note->getType() == "приходная" && waiting) result.push_back(note);
    }
    return result;
}

std::shared_ptr<DeliveryNote> WarehouseSystem::chooseExpectedNote() {
    const std::vector<std::shared_ptr<DeliveryNote>> expected = getExpectedNotes();
    if (expected.empty()) {
        std::cout << "Нет ожидаемых поставок. Сначала зарегистрируйте поставку.\n";
        return nullptr;
    }
    std::cout << "Ожидаемые поставки:\n";
    for (std::size_t index = 0; index < expected.size(); ++index) {
        std::cout << "  " << (index + 1) << ". " << expected[index]->getNumber()
                  << ", срок " << expected[index]->getDueDate() << ":";
        for (const auto& item : expected[index]->getItems()) {
            std::cout << " «" << item->getProduct()->getName() << "» " << item->getExpectedQuantity() << " шт.;";
        }
        if (expected[index]->getStatus() == DeliveryNoteStatus::Created) {
            std::cout << "  [места не подготовлены]";
        }
        std::cout << "\n";
    }
    return expected[static_cast<std::size_t>(
        readInt("Номер в списке", 1, static_cast<int>(expected.size()), 1) - 1)];
}

int WarehouseSystem::countFreeUnits(double unitVolume) const {
    int units = 0;
    for (StorageCell* cell : warehouse_->getAllCells()) {
        const double freeVolume = cell->getCapacity() - cell->getCurrentVolume() - cell->getReservedVolume();
        if (freeVolume > 0.0) units += static_cast<int>((freeVolume + 0.0001) / unitVolume);
    }
    return units;
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

    // Отобранный по заказам товар уже не в ячейках, но ещё не в зоне отгрузки.
    bool header = false;
    for (const auto& order : orders_) {
        if (order->getStatus() == OrderStatus::Shipped) continue;
        for (const auto& item : order->getItems()) {
            const int inZone = item->getPickedBatch() ? item->getPickedBatch()->quantity : 0;
            const int notInZone = item->getPickedQuantity() - inZone;
            if (notInZone <= 0) continue;
            if (!header) {
                std::cout << "Отобрано по заказам, ждёт отгрузки:\n";
                header = true;
            }
            std::cout << "  Заказ #" << order->getId() << ": «" << item->getProduct()->getName() << "» "
                      << notInZone << " шт.\n";
        }
    }
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
    std::cout << "  4. Отменить поставку\n";
    const int choice = readInt("Что делаем", 1, 4, expected > 0 ? 2 : 1);
    std::cout << "\n";

    if (choice == 1) registerDelivery();
    else if (choice == 2) receiveDelivery();
    else if (choice == 3) checkDeliveryDates();
    else cancelDelivery();
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
    bool allPrepared = true;
    for (const auto& line : lines) {
        DeliveryNoteItem* item = note->addItem(std::make_unique<DeliveryNoteItem>(nextId_++, line.first, line.second));
        if (!prepareItem(*note, item, prepared)) allPrepared = false;
    }
    deliveryNotes_.push_back(note);
    if (allPrepared) {
        note->setStatus(DeliveryNoteStatus::Expected);
        addNotification(note->createNotification(nextId_++, "подготовлены ячейки " + prepared, "Оператор"));
    } else {
        // Поставка остаётся неподготовленной, пока не освободится место (альтернатива 1).
        addNotification(note->createNotification(
            nextId_++, "не подготовлена: не хватает места, подготовка повторится при приёмке", "Оператор"));
    }
    std::cout << "\nИтог: поставка " << number << " зарегистрирована, срок " << note->getDueDate() << "; "
              << (allPrepared ? "подготовлены ячейки: " + prepared
                              : std::string("места подготовлены не для всех позиций")) << ".\n";
}

bool WarehouseSystem::prepareItem(DeliveryNote& note, DeliveryNoteItem* item, std::string& prepared) {
    Product* product = item->getProduct();
    const int quantity = item->getExpectedQuantity();
    const double volume = product->getUnitVolume() * quantity;
    const auto addAddress = [&prepared](const std::string& address) {
        if (prepared.find(address) == std::string::npos) {
            prepared += (prepared.empty() ? "" : ", ") + address;
        }
    };

    StorageCell* cell = warehouse_->findFreeCell(volume);
    if (cell != nullptr && cell->reserveSpace(volume)) {
        item->setStorageCell(cell, volume);
        addAddress(cell->getAddress());
        return true;
    }

    // В одну ячейку не помещается — резервируем место в нескольких, если хватает в сумме.
    const double unit = product->getUnitVolume();
    if (countFreeUnits(unit) < quantity) {
        addNotification(note.createNotification(
            nextId_++, "нет места для «" + product->getName() + "» (" + formatVolume(volume) + " м³)", "Оператор"));
        return false;
    }
    std::cout << "WarehouseSystem::prepareItem: в одну ячейку не помещается, резервируем несколько.\n";
    int left = quantity;
    while (left > 0) {
        cell = warehouse_->findFreeCell(unit);
        if (cell == nullptr) break;
        const int fit = std::min(left, static_cast<int>((cell->getAvailableVolume() + 0.0001) / unit));
        if (fit <= 0 || !cell->reserveSpace(fit * unit)) break;
        item->setStorageCell(cell, fit * unit);
        addAddress(cell->getAddress());
        left -= fit;
    }
    return left == 0;
}

void WarehouseSystem::scanProduct(const Product* product) {
    while (true) {
        std::cout << "  Отсканируйте «" << product->getName() << "» — артикул [Enter = " << product->getSku() << "]: ";
        std::string line;
        if (!readLine(line) || line.empty() || line == product->getSku()) {
            if (inputClosed_) std::cout << "\n";
            return;
        }
        std::cout << "  Ошибка: артикул " << line << " не совпадает с позицией накладной, отсканируйте ещё раз.\n";
    }
}

void WarehouseSystem::receiveDelivery() {
    printClassChain({"WarehouseSystem", "DeliveryNote", "DeliveryNoteItem", "Delivery", "GoodsBatch", "StorageCell"});

    const std::shared_ptr<DeliveryNote> note = chooseExpectedNote();
    if (!note) {
        return;
    }

    // Поставка не была подготовлена — пробуем зарезервировать место сейчас.
    if (note->getStatus() == DeliveryNoteStatus::Created) {
        std::cout << "\nПодготовка мест для поставки " << note->getNumber() << ":\n";
        std::string prepared;
        bool allPrepared = true;
        for (const auto& item : note->getItems()) {
            if (item->getReservations().empty() && !prepareItem(*note, item.get(), prepared)) allPrepared = false;
        }
        if (!allPrepared) {
            std::cout << "\nИтог: места по-прежнему не хватает, поставка " << note->getNumber()
                      << " остаётся неподготовленной. Освободите место или отмените поставку.\n";
            return;
        }
        note->setStatus(DeliveryNoteStatus::Expected);
        std::cout << "Места подготовлены.\n\n";
    }

    // Кладовщик сканирует товар и вводит фактическое количество; при расхождении оператор решает, что делать.
    std::cout << "Сканирование и пересчёт (Enter — совпадает с накладной):\n";
    struct Decision {
        int actual;
        bool accept;
    };
    std::vector<Decision> decisions;
    for (const auto& item : note->getItems()) {
        scanProduct(item->getProduct());
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
    note->setStatus(DeliveryNoteStatus::Received);

    int batchesWaiting = 0;
    for (std::size_t index = 0; index < note->getItems().size(); ++index) {
        DeliveryNoteItem* item = note->getItems()[index].get();
        const Decision& decision = decisions[index];
        Product* product = item->getProduct();

        item->setActualQuantity(decision.actual);
        const int difference = item->calculateDifference();
        for (const auto& reservation : item->getReservations()) {
            reservation.cell->releaseSpace(reservation.volume);   // резерв снимается после приёмки
        }

        if (!decision.accept) {
            item->reject();
            addNotification(note->createNotification(
                nextId_++, "позиция «" + product->getName() + "» отклонена", "Оператор"));
            continue;
        }
        if (difference != 0) {
            note->setStatus(DeliveryNoteStatus::Discrepancy);   // запись о расхождении
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

    // Все позиции обработаны — накладная завершена.
    const bool quantityOk = note->checkQuantity();
    note->setStatus(DeliveryNoteStatus::Completed);
    if (quantityOk) {
        delivery->accept();
        addNotification(note->createNotification(nextId_++, "принята", "Оператор"));
    } else {
        delivery->acceptWithDiscrepancy();
        addNotification(note->createNotification(nextId_++, "принята с расхождением", "Оператор"));
    }
    if (batchesWaiting > 0) {
        addNotification("Партий в зоне приёмки ждут размещения: " + std::to_string(batchesWaiting),
                        "Размещение", "Кладовщик");
    }

    note->printInfo();
    delivery->printInfo();

    std::cout << "\nИтог: поставка " << note->getNumber() << " — "
              << (quantityOk ? "принята" : "принята с расхождением")
              << "; партий ждут размещения (пункт 3): " << batchesWaiting << ".\n";
}

void WarehouseSystem::cancelDelivery() {
    printClassChain({"WarehouseSystem", "DeliveryNote", "DeliveryNoteItem", "StorageCell"});
    const std::shared_ptr<DeliveryNote> note = chooseExpectedNote();
    if (!note) {
        return;
    }

    printMethodsHeader();
    std::cout << "WarehouseSystem::cancelDelivery: отмена поставки " << note->getNumber() << ".\n";
    for (const auto& item : note->getItems()) {
        for (const auto& reservation : item->getReservations()) {
            reservation.cell->releaseSpace(reservation.volume);
        }
    }
    addNotification(note->createNotification(nextId_++, "отменена, резерв ячеек снят", "Оператор"));
    deliveryNotes_.erase(std::remove(deliveryNotes_.begin(), deliveryNotes_.end(), note), deliveryNotes_.end());

    std::cout << "\nИтог: поставка " << note->getNumber() << " отменена.\n";
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
        if (cell != nullptr) {
            cell->getAvailableVolume();
            const bool confirmed = readInt("Кладовщик: товар положен в ячейку " + cell->getAddress() +
                                           "? 1 — да, 2 — нет", 1, 2, 1) == 1;
            if (!confirmed) {
                std::cout << "WarehouseSystem::placeGoods: размещение не подтверждено, задание остаётся открытым.\n";
            } else if (cell->placeBatch(batch)) {
                ++placed;
                std::cout << "WarehouseSystem::placeGoods: кладовщик подтвердил размещение в ячейке "
                          << cell->getAddress() << ".\n";
            }
            continue;
        }

        // Целиком партия не помещается ни в одну ячейку — раскладываем её по частям.
        std::cout << "WarehouseSystem::placeGoods: партия целиком не помещается, раскладываем по частям.\n";
        while (batch->location == receivingZone_ && batch->quantity > 0) {
            cell = warehouse_->findFreeCell(batch->getUnitVolume());
            if (cell == nullptr) break;
            const int fit = static_cast<int>((cell->getAvailableVolume() + 0.0001) / batch->getUnitVolume());
            if (fit >= batch->quantity) {
                cell->placeBatch(batch);
                break;
            }
            const std::shared_ptr<GoodsBatch> part = batch->split(nextId_++, fit);
            if (!part) break;
            if (!cell->placeBatch(part)) {
                receivingZone_->placeBatch(part);   // не должно случаться, но товар не теряем
                break;
            }
        }

        if (batch->location != receivingZone_) {
            ++placed;
            std::cout << "WarehouseSystem::placeGoods: партия #" << batch->id << " размещена по частям.\n";
        } else {
            addNotification("Нет места для " + std::to_string(batch->quantity) + " шт. «" + batch->productName +
                            "» (партия #" + std::to_string(batch->id) + "), они остаются в зоне приёмки",
                            "Размещение", "Кладовщик");
        }
    }
    std::cout << "\nИтог: размещено партий " << placed << " из " << total << ".\n";
}

// ---------- 4. Заказ ----------

void WarehouseSystem::orderMenu() {
    printScenarioTitle("4. Создание и комплектация заказа");

    // Заказы, которые ещё не отгружены: недособранные можно дособрать, любые — отменить.
    std::vector<WarehouseOrder*> open = getOrdersWithStatus(OrderStatus::Picking);
    for (WarehouseOrder* order : getOrdersWithStatus(OrderStatus::Completed)) open.push_back(order);
    if (open.empty()) {
        createOrder();
        return;
    }

    std::cout << "  1. Создать новый заказ\n";
    for (std::size_t index = 0; index < open.size(); ++index) {
        std::cout << "  " << (index + 2) << ". Заказ #" << open[index]->getId() << " ("
                  << WarehouseOrder::statusText(open[index]->getStatus()) << "):";
        for (const auto& item : open[index]->getItems()) {
            std::cout << " «" << item->getProduct()->getName() << "» " << item->getPickedQuantity()
                      << " из " << item->getQuantity() << " шт.;";
        }
        std::cout << "\n";
    }
    const int choice = readInt("Что делаем", 1, static_cast<int>(open.size()) + 1, 1);
    if (choice == 1) {
        std::cout << "\n";
        createOrder();
        return;
    }

    WarehouseOrder* order = open[static_cast<std::size_t>(choice - 2)];
    bool cancel = false;
    if (order->getStatus() == OrderStatus::Picking) {
        cancel = readInt("1 — дособрать заказ, 2 — отменить заказ", 1, 2, 1) == 2;
    } else {
        cancel = readInt("Заказ уже собран. Отменить его? 1 — да, 2 — нет", 1, 2, 2) == 1;
        if (!cancel) return;
    }
    std::cout << "\n";
    if (cancel) {
        cancelOrder(order);
        return;
    }
    printClassChain({"WarehouseSystem", "WarehouseOrder", "OrderItem", "GoodsBatch", "StorageCell"});
    printMethodsHeader();
    pickOrder(order);
}

void WarehouseSystem::cancelOrder(WarehouseOrder* order) {
    printClassChain({"WarehouseSystem", "WarehouseOrder", "OrderItem", "GoodsBatch", "BufferZone"});
    printMethodsHeader();
    const std::string orderName = "Заказ #" + std::to_string(order->getId());
    std::cout << "WarehouseSystem::cancelOrder: отмена, " << orderName << ".\n";

    // Уже отобранный товар возвращается на склад через зону приёмки.
    int returned = 0;
    for (const auto& item : order->getItems()) {
        const int picked = item->getPickedQuantity();
        if (picked == 0) continue;
        Product* product = item->getProduct();

        // Если товар уже перенесли в зону отгрузки, забираем его оттуда.
        const std::shared_ptr<GoodsBatch>& pickedBatch = item->getPickedBatch();
        if (pickedBatch && pickedBatch->quantity > 0) {
            if (pickedBatch->location != nullptr) pickedBatch->location->removeBatch(pickedBatch.get());
            pickedBatch->decreaseQuantity(pickedBatch->quantity, true);
        }
        auto batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * picked,
                                                  product->getName(), picked, product);
        if (!receivingZone_->placeBatch(batch)) {
            warehouse_->placeBatch(batch);
        }
        returned += picked;
    }

    shipments_.erase(std::remove_if(shipments_.begin(), shipments_.end(),
        [order](const std::unique_ptr<Shipment>& shipment) { return shipment->getOrder() == order; }),
        shipments_.end());
    orders_.erase(std::remove_if(orders_.begin(), orders_.end(),
        [order](const std::unique_ptr<WarehouseOrder>& item) { return item.get() == order; }), orders_.end());
    addNotification(orderName + " отменён, возвращено на склад " + std::to_string(returned) + " шт.",
                    "Заказ", "Оператор");
    std::cout << "\nИтог: " << orderName << " отменён; товар (" << returned
              << " шт.) ждёт размещения в зоне приёмки (пункт 3).\n";
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
            if (take <= 0) continue;

            std::cout << "WarehouseSystem::pickOrder: задание кладовщику — взять " << take << " шт., место: "
                      << batch->location->getTitle() << ".\n";
            const int picked = readInt("Кладовщик: сколько отобрано (меньше — нехватка или брак)", 0, take, take);
            if (picked < take) {
                addNotification("Заказ #" + std::to_string(order->getId()) + ": отобрано " + std::to_string(picked) +
                                " из " + std::to_string(take) + " шт. «" + product->getName() + "» (" +
                                batch->location->getTitle() + ")", "Заказ", "Оператор");
            }
            if (picked == 0 || !batch->reserveQuantity(picked)) continue;

            item->addBatch(batch);
            item->pick(picked);
            batch->decreaseQuantity(picked, true);
            if (batch->quantity == 0) {
                warehouse_->removeBatch(batch.get());   // партия закончилась — ячейка освобождается
            }
            need -= picked;
            pickedNow += picked;
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
    bool saved = false;
    while (!saved) {
        for (auto& line : lines) {
            line.actual = readInt("  " + line.cell->getAddress() + ", «" + line.batch->productName +
                                  "», по учёту " + std::to_string(line.batch->quantity) + " шт.",
                                  -1, 10000, line.batch->quantity);
        }
        saved = readInt("Сохранить результаты? 1 — да, 2 — ввести заново", 1, 2, 1) == 1;
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
        line.batch->product->printInfo();
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
            warehouse_->removeBatch(line.batch.get());
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
    printClassChain({"WarehouseSystem", "WarehouseOrder", "Shipment", "DeliveryNote", "ShipmentItem", "GoodsBatch", "BufferZone"});

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

    // У заказа одна отгрузка: если её уже оформляли, но не подтвердили — продолжаем её.
    Shipment* shipment = nullptr;
    for (const auto& existing : shipments_) {
        if (existing->getOrder() == order) shipment = existing.get();
    }
    if (shipment == nullptr) {
        shipments_.push_back(std::make_unique<Shipment>(++shipmentCounter_, order));
        shipment = shipments_.back().get();

        // Исходящая накладная: товары и ожидаемое количество, связана с заказом через отгрузку.
        auto note = std::make_shared<DeliveryNote>(nextId_++, formatNumber("РН-", ++outgoingCounter_),
                                                   "расходная", currentDate());
        for (const auto& orderItem : order->getItems()) {
            note->addItem(std::make_unique<DeliveryNoteItem>(nextId_++, orderItem->getProduct(),
                                                             orderItem->getQuantity()));
            shipment->addItem(std::make_unique<ShipmentItem>(nextId_++, orderItem->getProduct(), orderItem.get(),
                                                             orderItem->getQuantity(), 0));
        }
        deliveryNotes_.push_back(note);
        shipment->setDeliveryNote(note);
    } else {
        std::cout << "WarehouseSystem::createShipment: продолжаем ранее оформленную отгрузку.\n";
    }
    const std::shared_ptr<DeliveryNote>& note = shipment->getDeliveryNote();

    bool allInZone = true;
    for (std::size_t index = 0; index < order->getItems().size(); ++index) {
        OrderItem* orderItem = order->getItems()[index].get();
        Product* product = orderItem->getProduct();
        const int picked = orderItem->getPickedQuantity();

        // Собранный товар оформляется партией (или дополняется, если заказ дособирали).
        std::shared_ptr<GoodsBatch> batch = orderItem->getPickedBatch();
        if (!batch) {
            batch = std::make_shared<GoodsBatch>(nextId_++, product->getUnitVolume() * picked,
                                                 product->getName(), picked, product);
            batch->reserveQuantity(picked);
            orderItem->setPickedBatch(batch);
        } else if (batch->quantity < picked) {
            const int added = picked - batch->quantity;
            batch->changeQuantity(picked);
            batch->reserveQuantity(added);
        }

        // Кладовщик переносит товар в зону отгрузки и вводит фактическое количество.
        const int brought = readInt("Кладовщик: перенесено в зону отгрузки «" + product->getName() +
                                    "», шт. (меньше — брак)", 0, picked, picked);
        if (brought < picked) {
            const int missing = picked - brought;
            batch->decreaseQuantity(missing, true);   // повреждённый товар списывается
            orderItem->cancelPick(missing);
            addNotification("Заказ #" + std::to_string(order->getId()) + ": при отгрузке «" + product->getName() +
                            "» обнаружено " + std::to_string(missing) + " шт. брака, нужно дособрать",
                            "Отгрузка", "Оператор");
        }

        if (batch->quantity == 0) {
            if (batch->location != nullptr) batch->location->removeBatch(batch.get());
        } else if (batch->location != shippingZone_ && batch->volume > shippingZone_->getCapacity()) {
            // Крупный заказ больше всей зоны — его грузят в транспорт сразу.
            std::cout << "WarehouseSystem::createShipment: «" << product->getName()
                      << "» больше зоны отгрузки, грузится сразу в транспорт.\n";
        } else if (batch->location != shippingZone_ && !shippingZone_->placeBatch(batch)) {
            allInZone = false;
            addNotification("В зоне отгрузки нет места для «" + product->getName() + "»", "Отгрузка", "Оператор");
        }

        shipment->getItems()[index]->setActualQuantity(brought);
        note->getItems()[index]->setActualQuantity(brought);
    }

    bool shipped = false;
    if (!allInZone) {
        std::cout << "WarehouseSystem::createShipment: не весь товар в зоне отгрузки — "
                     "сначала отгрузите или подтвердите другие заказы.\n";
    } else if (shipment->checkReadiness()) {
        const bool confirmed =
            readInt("Оператор: товар и документы проверены, подтвердить отгрузку? 1 — да, 2 — нет", 1, 2, 1) == 1;
        if (confirmed) {
            shipped = shipment->dispatch();
            addNotification(orderName + " отгружен по накладной " + note->getNumber(), "Отгрузка", "Менеджер");
        } else {
            std::cout << "WarehouseSystem::createShipment: отгрузка не подтверждена, товар остаётся в зоне отгрузки.\n";
        }
    } else {
        // Расхождение: запись о нём и дополнительная проверка — заказ возвращается на комплектацию.
        note->setStatus(DeliveryNoteStatus::Discrepancy);
        for (const auto& item : shipment->getItems()) {
            if (!item->isCorrect()) addNotification(item->createNotification(nextId_++));
        }
        order->startPicking();
        std::cout << "WarehouseSystem::createShipment: заказ возвращён на комплектацию (пункт 4), "
                     "собранный товар ждёт в зоне отгрузки.\n";
    }
    note->printInfo();
    shipment->printInfo();

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

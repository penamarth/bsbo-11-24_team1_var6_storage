#include "warehouse_entities.h"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <utility>

namespace {

const char* deliveryStatusText(DeliveryStatus status) {
    switch (status) {
        case DeliveryStatus::Expected: return "ожидается";
        case DeliveryStatus::Arrived: return "прибыла";
        case DeliveryStatus::Accepted: return "принята";
        case DeliveryStatus::Discrepancy: return "принята с расхождением";
    }
    return "неизвестно";
}

const char* shipmentStatusText(ShipmentStatus status) {
    switch (status) {
        case ShipmentStatus::Created: return "создана";
        case ShipmentStatus::Preparing: return "готовится";
        case ShipmentStatus::Ready: return "готова";
        case ShipmentStatus::Shipped: return "отгружена";
    }
    return "неизвестно";
}

} // namespace

// Даты

std::string currentDate(int daysFromToday) {
    const std::time_t now = std::time(nullptr) + static_cast<std::time_t>(daysFromToday) * 24 * 60 * 60;
    const std::tm* local = std::localtime(&now);
    char buffer[16] = {};
    std::strftime(buffer, sizeof(buffer), "%d.%m.%Y", local);
    return buffer;
}

int dateKey(const std::string& date) {
    // ДД.ММ.ГГГГ
    if (date.size() != 10) {
        return 0;
    }
    const int day = std::stoi(date.substr(0, 2));
    const int month = std::stoi(date.substr(3, 2));
    const int year = std::stoi(date.substr(6, 4));
    return year * 10000 + month * 100 + day;
}

// Product

Product::Product(int id, std::string name, std::string sku, double unitVolume)
    : id_(id), name_(std::move(name)), sku_(std::move(sku)), unitVolume_(unitVolume) {
    std::cout << "Product::Product: товар «" << name_ << "» создан.\n";
}

void Product::printInfo() const {
    std::cout << "Product::printInfo: #" << id_ << " «" << name_ << "», артикул "
              << sku_ << ", объём единицы " << unitVolume_ << " м³.\n";
}

int Product::getId() const { return id_; }
const std::string& Product::getName() const { return name_; }
const std::string& Product::getSku() const { return sku_; }
double Product::getUnitVolume() const { return unitVolume_; }

// Notification

Notification::Notification(int id, std::string message, std::string type, std::string recipientRole)
    : id_(id), message_(std::move(message)), date_(currentDate()), type_(std::move(type)),
      recipientRole_(std::move(recipientRole)) {
    std::cout << "Notification::Notification: уведомление создано.\n";
}

void Notification::send() const {
    std::cout << "Notification::send: «" << message_ << "» отправлено роли «" << recipientRole_ << "».\n";
}

void Notification::markAsRead() {
    isRead_ = true;
}

void Notification::printInfo() const {
    std::cout << "Notification::printInfo: #" << id_ << (isRead_ ? "   " : " * ") << date_
              << " [" << type_ << "] " << message_ << " (получатель: " << recipientRole_ << ")\n";
}

bool Notification::isRead() const { return isRead_; }

// DeliveryNoteItem

DeliveryNoteItem::DeliveryNoteItem(int id, Product* product, int expectedQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), product_(product) {
    std::cout << "DeliveryNoteItem::DeliveryNoteItem: позиция «" << product_->getName()
              << "» создана, ожидается " << expectedQuantity_ << " шт.\n";
}

void DeliveryNoteItem::setActualQuantity(int quantity) {
    actualQuantity_ = quantity;
    std::cout << "DeliveryNoteItem::setActualQuantity: фактически получено " << quantity << " шт.\n";
}

void DeliveryNoteItem::setStorageCell(StorageCell* cell, double reservedVolume) {
    storageCell_ = cell;
    reservedVolume_ = reservedVolume;
    std::cout << "DeliveryNoteItem::setStorageCell: для позиции назначена ячейка "
              << (cell != nullptr ? cell->getAddress() : "—") << ".\n";
}

int DeliveryNoteItem::calculateDifference() const {
    const int result = actualQuantity_ - expectedQuantity_;
    std::cout << "DeliveryNoteItem::calculateDifference: расхождение — " << result << " шт.\n";
    return result;
}

bool DeliveryNoteItem::isQuantityCorrect() const {
    const bool result = !rejected_ && actualQuantity_ == expectedQuantity_;
    std::cout << "DeliveryNoteItem::isQuantityCorrect: позиция «" << product_->getName() << "» — "
              << (rejected_ ? "отклонена" : (result ? "количество совпадает" : "количество не совпадает"))
              << ".\n";
    return result;
}

void DeliveryNoteItem::reject() {
    rejected_ = true;
    std::cout << "DeliveryNoteItem::reject: позиция «" << product_->getName() << "» отклонена.\n";
}

void DeliveryNoteItem::printInfo() const {
    std::cout << "DeliveryNoteItem::printInfo: позиция #" << id_ << " «" << product_->getName()
              << "», ожидается " << expectedQuantity_ << ", фактически " << actualQuantity_ << " шт.\n";
}

Product* DeliveryNoteItem::getProduct() const { return product_; }
int DeliveryNoteItem::getExpectedQuantity() const { return expectedQuantity_; }
int DeliveryNoteItem::getActualQuantity() const { return actualQuantity_; }
StorageCell* DeliveryNoteItem::getStorageCell() const { return storageCell_; }
double DeliveryNoteItem::getReservedVolume() const { return reservedVolume_; }
bool DeliveryNoteItem::isRejected() const { return rejected_; }

// DeliveryNote

DeliveryNote::DeliveryNote(int id, std::string number, std::string type, std::string dueDate)
    : id_(id), number_(std::move(number)), date_(currentDate()), dueDate_(std::move(dueDate)),
      type_(std::move(type)) {
    std::cout << "DeliveryNote::DeliveryNote: накладная " << number_ << " (" << type_
              << ") создана, срок поставки " << dueDate_ << ".\n";
}

DeliveryNoteItem* DeliveryNote::addItem(std::unique_ptr<DeliveryNoteItem> item) {
    DeliveryNoteItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "DeliveryNote::addItem: позиция добавлена в накладную " << number_ << ".\n";
    return result;
}

bool DeliveryNote::checkQuantity() const {
    bool result = !items_.empty();
    for (const auto& item : items_) {
        if (!item->isQuantityCorrect()) result = false;
    }
    std::cout << "DeliveryNote::checkQuantity: по накладной " << number_ << " количество "
              << (result ? "сходится" : "НЕ сходится") << ".\n";
    return result;
}

void DeliveryNote::setStatus(DeliveryNoteStatus status) {
    status_ = status;
    std::cout << "DeliveryNote::setStatus: статус накладной " << number_ << " — "
              << statusText(status_) << ".\n";
}

void DeliveryNote::setDelivery(Delivery* delivery) {
    delivery_ = delivery;
    std::cout << "DeliveryNote::setDelivery: к накладной " << number_ << " привязана поставка.\n";
}

std::unique_ptr<Notification> DeliveryNote::createNotification(int id, const std::string& message,
                                                               const std::string& role) const {
    std::cout << "DeliveryNote::createNotification: уведомление по накладной " << number_ << ".\n";
    return std::make_unique<Notification>(id, "Поставка " + number_ + ": " + message, "Приёмка", role);
}

void DeliveryNote::printInfo() const {
    std::cout << "DeliveryNote::printInfo: накладная #" << id_ << " " << number_ << " (" << type_
              << ") от " << date_ << ", срок " << dueDate_ << ", позиций " << items_.size()
              << ", статус " << statusText(status_) << ".\n";
}

const char* DeliveryNote::statusText(DeliveryNoteStatus status) {
    switch (status) {
        case DeliveryNoteStatus::Created: return "создана";
        case DeliveryNoteStatus::Expected: return "ожидается";
        case DeliveryNoteStatus::Received: return "товар получен";
        case DeliveryNoteStatus::Completed: return "завершена";
        case DeliveryNoteStatus::Discrepancy: return "расхождение";
    }
    return "неизвестно";
}

const std::vector<std::unique_ptr<DeliveryNoteItem>>& DeliveryNote::getItems() const { return items_; }
const std::string& DeliveryNote::getNumber() const { return number_; }
const std::string& DeliveryNote::getDueDate() const { return dueDate_; }
DeliveryNoteStatus DeliveryNote::getStatus() const { return status_; }
Delivery* DeliveryNote::getDelivery() const { return delivery_; }

// Delivery

Delivery::Delivery(int id, std::shared_ptr<DeliveryNote> deliveryNote)
    : id_(id), deliveryNote_(std::move(deliveryNote)) {
    std::cout << "Delivery::Delivery: поставка #" << id_ << " по накладной "
              << deliveryNote_->getNumber() << " создана.\n";
    deliveryNote_->setDelivery(this);
}

void Delivery::arrive() {
    status_ = DeliveryStatus::Arrived;
    arrivalDate_ = currentDate();
    std::cout << "Delivery::arrive: поставка прибыла на склад " << arrivalDate_ << ".\n";
}

void Delivery::accept() {
    status_ = DeliveryStatus::Accepted;
    std::cout << "Delivery::accept: поставка принята.\n";
}

void Delivery::acceptWithDiscrepancy() {
    status_ = DeliveryStatus::Discrepancy;
    std::cout << "Delivery::acceptWithDiscrepancy: поставка принята с расхождением.\n";
}

void Delivery::printInfo() const {
    std::cout << "Delivery::printInfo: поставка #" << id_ << ", прибыла " << arrivalDate_
              << ", статус " << deliveryStatusText(status_) << ".\n";
}

const std::shared_ptr<DeliveryNote>& Delivery::getDeliveryNote() const { return deliveryNote_; }
DeliveryStatus Delivery::getStatus() const { return status_; }

// OrderItem

OrderItem::OrderItem(int id, Product* product, int quantity)
    : id_(id), quantity_(quantity), product_(product) {
    std::cout << "OrderItem::OrderItem: позиция «" << product_->getName() << "» создана, нужно "
              << quantity_ << " шт.\n";
}

void OrderItem::pick(int quantity) {
    pickedQuantity_ += quantity;
    std::cout << "OrderItem::pick: отобрано " << quantity << " шт., всего "
              << pickedQuantity_ << " из " << quantity_ << " шт.\n";
}

bool OrderItem::isComplete() const {
    const bool result = pickedQuantity_ >= quantity_;
    std::cout << "OrderItem::isComplete: позиция «" << product_->getName() << "» "
              << (result ? "собрана" : "собрана не полностью") << ".\n";
    return result;
}

void OrderItem::addBatch(const std::shared_ptr<GoodsBatch>& batch) {
    batches_.push_back(batch);
    std::cout << "OrderItem::addBatch: партия #" << batch->id << " связана с позицией заказа.\n";
}

void OrderItem::setPickedBatch(const std::shared_ptr<GoodsBatch>& batch) {
    pickedBatch_ = batch;
    std::cout << "OrderItem::setPickedBatch: собранный товар оформлен партией #" << batch->id << ".\n";
}

void OrderItem::printInfo() const {
    std::cout << "OrderItem::printInfo: позиция #" << id_ << " «" << product_->getName() << "», отобрано "
              << pickedQuantity_ << " из " << quantity_ << " шт.\n";
}

Product* OrderItem::getProduct() const { return product_; }
int OrderItem::getQuantity() const { return quantity_; }
int OrderItem::getPickedQuantity() const { return pickedQuantity_; }
const std::shared_ptr<GoodsBatch>& OrderItem::getPickedBatch() const { return pickedBatch_; }

// WarehouseOrder

WarehouseOrder::WarehouseOrder(int id) : id_(id), createdAt_(currentDate()) {
    std::cout << "WarehouseOrder::WarehouseOrder: заказ #" << id_ << " создан " << createdAt_ << ".\n";
}

OrderItem* WarehouseOrder::addItem(std::unique_ptr<OrderItem> item) {
    OrderItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "WarehouseOrder::addItem: позиция добавлена в заказ #" << id_ << ".\n";
    return result;
}

void WarehouseOrder::startPicking() {
    status_ = OrderStatus::Picking;
    std::cout << "WarehouseOrder::startPicking: начата комплектация заказа #" << id_ << ".\n";
}

bool WarehouseOrder::checkCompleteness() const {
    bool result = !items_.empty();
    for (const auto& item : items_) {
        if (!item->isComplete()) result = false;
    }
    std::cout << "WarehouseOrder::checkCompleteness: заказ #" << id_
              << (result ? " укомплектован" : " укомплектован не полностью") << ".\n";
    return result;
}

void WarehouseOrder::complete() {
    if (status_ == OrderStatus::Shipped) {
        std::cout << "WarehouseOrder::complete: заказ #" << id_ << " уже отгружен.\n";
        return;
    }
    status_ = OrderStatus::Completed;
    std::cout << "WarehouseOrder::complete: заказ #" << id_ << " получил статус «"
              << statusText(status_) << "».\n";
}

void WarehouseOrder::ship() {
    if (status_ != OrderStatus::Completed) {
        std::cout << "WarehouseOrder::ship: заказ #" << id_ << " нельзя отгрузить, статус — "
                  << statusText(status_) << ".\n";
        return;
    }
    status_ = OrderStatus::Shipped;
    std::cout << "WarehouseOrder::ship: заказ #" << id_ << " получил статус «" << statusText(status_) << "».\n";
}

void WarehouseOrder::printInfo() const {
    std::cout << "WarehouseOrder::printInfo: заказ #" << id_ << " от " << createdAt_ << ", статус "
              << statusText(status_) << ".\n";
    for (const auto& item : items_) item->printInfo();
}

const char* WarehouseOrder::statusText(OrderStatus status) {
    switch (status) {
        case OrderStatus::Created: return "создан";
        case OrderStatus::Picking: return "комплектуется";
        case OrderStatus::Completed: return "укомплектован";
        case OrderStatus::Shipped: return "отгружен";
    }
    return "неизвестно";
}

const std::vector<std::unique_ptr<OrderItem>>& WarehouseOrder::getItems() const { return items_; }
int WarehouseOrder::getId() const { return id_; }
OrderStatus WarehouseOrder::getStatus() const { return status_; }

// StocktakeItem

StocktakeItem::StocktakeItem(int id, Product* product, StorageCell* cell, int expectedQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), product_(product), storageCell_(cell) {
    std::cout << "StocktakeItem::StocktakeItem: позиция создана, ячейка " << storageCell_->getAddress()
              << ", товар «" << product_->getName() << "», по учёту " << expectedQuantity_ << " шт.\n";
}

void StocktakeItem::setActualQuantity(int quantity) {
    actualQuantity_ = quantity;
    std::cout << "StocktakeItem::setActualQuantity: фактически насчитано " << quantity << " шт.\n";
}

int StocktakeItem::calculateDifference() {
    difference_ = actualQuantity_ - expectedQuantity_;
    if (difference_ == 0) {
        status_ = "проверено";
    } else {
        status_ = difference_ < 0 ? "недостача" : "излишек";
    }
    std::cout << "StocktakeItem::calculateDifference: расхождение — " << difference_
              << " шт., статус позиции — " << status_ << ".\n";
    return difference_;
}

bool StocktakeItem::isCorrect() const {
    const bool result = isChecked() && difference_ == 0;
    std::cout << "StocktakeItem::isCorrect: остаток " << (result ? "корректен" : "НЕ совпадает") << ".\n";
    return result;
}

void StocktakeItem::markForRecheck() {
    status_ = "повторная проверка";
    std::cout << "StocktakeItem::markForRecheck: ячейка " << storageCell_->getAddress()
              << " отмечена для повторной проверки.\n";
}

bool StocktakeItem::isChecked() const {
    return status_ != "не проверено" && status_ != "повторная проверка";
}

std::unique_ptr<Notification> StocktakeItem::createNotification(int id) const {
    std::cout << "StocktakeItem::createNotification: уведомление о расхождении.\n";
    const int amount = difference_ < 0 ? -difference_ : difference_;
    return std::make_unique<Notification>(
        id, "Ячейка " + storageCell_->getAddress() + ", «" + product_->getName() + "»: " + status_ +
            " " + std::to_string(amount) + " шт.",
        "Инвентаризация", "Оператор");
}

void StocktakeItem::printInfo() const {
    std::cout << "StocktakeItem::printInfo: позиция #" << id_ << ", ячейка " << storageCell_->getAddress()
              << ", товар «" << product_->getName() << "», по учёту " << expectedQuantity_
              << ", фактически " << actualQuantity_ << " шт., статус — " << status_ << ".\n";
}

int StocktakeItem::getDifference() const { return difference_; }
int StocktakeItem::getActualQuantity() const { return actualQuantity_; }
const std::string& StocktakeItem::getStatus() const { return status_; }

// StocktakeSession

StocktakeSession::StocktakeSession(int id) : id_(id), date_(currentDate()) {
    std::cout << "StocktakeSession::StocktakeSession: сессия инвентаризации #" << id_
              << " от " << date_ << " создана.\n";
}

void StocktakeSession::start() {
    status_ = StocktakeStatus::InProgress;
    std::cout << "StocktakeSession::start: инвентаризация начата.\n";
}

void StocktakeSession::finish() {
    status_ = StocktakeStatus::Completed;
    std::cout << "StocktakeSession::finish: инвентаризация завершена.\n";
}

StocktakeItem* StocktakeSession::addItem(std::unique_ptr<StocktakeItem> item) {
    StocktakeItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "StocktakeSession::addItem: позиция добавлена в сессию.\n";
    return result;
}

bool StocktakeSession::isComplete() const {
    const auto unchecked = std::count_if(items_.begin(), items_.end(),
        [](const std::unique_ptr<StocktakeItem>& item) { return !item->isChecked(); });
    std::cout << "StocktakeSession::isComplete: ";
    if (unchecked == 0) {
        std::cout << "все ячейки проверены.\n";
    } else {
        std::cout << "не проверено позиций — " << unchecked << ".\n";
    }
    return unchecked == 0;
}

void StocktakeSession::printInfo() const {
    const char* statusText = status_ == StocktakeStatus::Completed ? "завершена"
                           : status_ == StocktakeStatus::InProgress ? "не завершена" : "создана";
    std::cout << "StocktakeSession::printInfo: сессия #" << id_ << " от " << date_
              << ", позиций " << items_.size() << ", статус — " << statusText << ".\n";
}

// ShipmentItem

ShipmentItem::ShipmentItem(int id, Product* product, OrderItem* orderItem,
                           int expectedQuantity, int actualQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), actualQuantity_(actualQuantity),
      product_(product), orderItem_(orderItem) {
    std::cout << "ShipmentItem::ShipmentItem: позиция «" << product_->getName() << "» создана, по заказу "
              << expectedQuantity_ << ", подготовлено " << actualQuantity_ << " шт.\n";
}

int ShipmentItem::calculateDifference() const {
    const int result = actualQuantity_ - expectedQuantity_;
    std::cout << "ShipmentItem::calculateDifference: расхождение — " << result << " шт.\n";
    return result;
}

bool ShipmentItem::isCorrect() const {
    const bool result = calculateDifference() == 0;
    std::cout << "ShipmentItem::isCorrect: позиция " << (result ? "соответствует заказу" : "НЕ соответствует заказу") << ".\n";
    return result;
}

std::unique_ptr<Notification> ShipmentItem::createNotification(int id) const {
    std::cout << "ShipmentItem::createNotification: уведомление о расхождении при отгрузке.\n";
    return std::make_unique<Notification>(
        id, "Расхождение при отгрузке «" + product_->getName() + "»: по заказу " +
            std::to_string(expectedQuantity_) + ", подготовлено " + std::to_string(actualQuantity_) + " шт.",
        "Отгрузка", "Оператор");
}

void ShipmentItem::printInfo() const {
    std::cout << "ShipmentItem::printInfo: позиция #" << id_ << ", товар «" << product_->getName()
              << "», по заказу " << expectedQuantity_ << ", подготовлено " << actualQuantity_ << " шт.\n";
}

OrderItem* ShipmentItem::getOrderItem() const { return orderItem_; }

// Shipment

Shipment::Shipment(int id, WarehouseOrder* warehouseOrder)
    : id_(id), date_(currentDate()), warehouseOrder_(warehouseOrder) {
    std::cout << "Shipment::Shipment: отгрузка #" << id_ << " по заказу #" << warehouseOrder_->getId()
              << " создана.\n";
}

ShipmentItem* Shipment::addItem(std::unique_ptr<ShipmentItem> item) {
    ShipmentItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "Shipment::addItem: позиция добавлена в отгрузку.\n";
    return result;
}

bool Shipment::checkReadiness() {
    bool result = !items_.empty() && warehouseOrder_->getStatus() == OrderStatus::Completed;
    for (const auto& item : items_) {
        if (!item->isCorrect()) result = false;
    }
    status_ = result ? ShipmentStatus::Ready : ShipmentStatus::Preparing;
    std::cout << "Shipment::checkReadiness: отгрузка " << (result ? "готова" : "НЕ готова") << ".\n";
    return result;
}

bool Shipment::dispatch() {
    if (status_ != ShipmentStatus::Ready) {
        std::cout << "Shipment::dispatch: отгрузка #" << id_ << " не готова, отгрузить нельзя.\n";
        return false;
    }

    // Собранный товар покидает зону отгрузки.
    for (const auto& item : items_) {
        const std::shared_ptr<GoodsBatch> batch = item->getOrderItem()->getPickedBatch();
        if (!batch) {
            continue;
        }
        if (batch->location != nullptr) {
            batch->location->removeBatch(batch.get());
        }
        batch->decreaseQuantity(batch->quantity, true);
    }

    warehouseOrder_->ship();
    status_ = ShipmentStatus::Shipped;
    std::cout << "Shipment::dispatch: отгрузка #" << id_ << " выполнена " << date_ << ".\n";
    return true;
}

void Shipment::printInfo() const {
    std::cout << "Shipment::printInfo: отгрузка #" << id_ << " от " << date_ << " по заказу #"
              << warehouseOrder_->getId() << ", позиций " << items_.size()
              << ", статус " << shipmentStatusText(status_) << ".\n";
}

const std::vector<std::unique_ptr<ShipmentItem>>& Shipment::getItems() const { return items_; }

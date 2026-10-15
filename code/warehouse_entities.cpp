#include "warehouse_entities.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace {

const char* deliveryNoteStatusText(DeliveryNoteStatus status) {
    switch (status) {
        case DeliveryNoteStatus::Created: return "создана";
        case DeliveryNoteStatus::Expected: return "ожидается";
        case DeliveryNoteStatus::Received: return "принята";
        case DeliveryNoteStatus::Completed: return "завершена";
        case DeliveryNoteStatus::Discrepancy: return "расхождение";
    }
    return "неизвестно";
}

const char* deliveryStatusText(DeliveryStatus status) {
    switch (status) {
        case DeliveryStatus::Expected: return "ожидается";
        case DeliveryStatus::Arrived: return "прибыла";
        case DeliveryStatus::Accepted: return "принята";
        case DeliveryStatus::Discrepancy: return "расхождение";
    }
    return "неизвестно";
}

const char* orderStatusText(OrderStatus status) {
    switch (status) {
        case OrderStatus::Created: return "создан";
        case OrderStatus::Picking: return "комплектуется";
        case OrderStatus::Completed: return "укомплектован";
        case OrderStatus::Shipped: return "отгружен";
    }
    return "неизвестно";
}

} // namespace

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
double Product::getUnitVolume() const { return unitVolume_; }

// DeliveryNoteItem

DeliveryNoteItem::DeliveryNoteItem(int id, Product* product, int expectedQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), product_(product) {
    std::cout << "DeliveryNoteItem::DeliveryNoteItem: позиция накладной создана.\n";
}

void DeliveryNoteItem::setActualQuantity(int quantity) {
    actualQuantity_ = quantity;
    std::cout << "DeliveryNoteItem::setActualQuantity: фактическое количество сохранено.\n";
}

void DeliveryNoteItem::setStorageCell(StorageCell* cell, double reservedVolume) {
    storageCell_ = cell;
    reservedVolume_ = reservedVolume;
    std::cout << "DeliveryNoteItem::setStorageCell: для позиции назначена ячейка.\n";
}

int DeliveryNoteItem::calculateDifference() const {
    const int result = actualQuantity_ - expectedQuantity_;
    std::cout << "DeliveryNoteItem::calculateDifference: расхождение — " << result << " шт.\n";
    return result;
}

bool DeliveryNoteItem::isQuantityCorrect() const {
    const bool result = actualQuantity_ == expectedQuantity_;
    std::cout << "DeliveryNoteItem::isQuantityCorrect: количество совпадает — "
              << std::boolalpha << result << ".\n";
    return result;
}

void DeliveryNoteItem::printInfo() const {
    std::cout << "DeliveryNoteItem::printInfo: позиция #" << id_ << ", ожидается " << expectedQuantity_
              << ", фактически " << actualQuantity_ << " шт.\n";
}

Product* DeliveryNoteItem::getProduct() const { return product_; }
int DeliveryNoteItem::getExpectedQuantity() const { return expectedQuantity_; }
int DeliveryNoteItem::getActualQuantity() const { return actualQuantity_; }
StorageCell* DeliveryNoteItem::getStorageCell() const { return storageCell_; }
double DeliveryNoteItem::getReservedVolume() const { return reservedVolume_; }

// DeliveryNote

DeliveryNote::DeliveryNote(int id, std::string number, std::string date, std::string dueDate)
    : id_(id), number_(std::move(number)), date_(std::move(date)), dueDate_(std::move(dueDate)) {
    std::cout << "DeliveryNote::DeliveryNote: накладная " << number_ << " создана.\n";
}

DeliveryNoteItem* DeliveryNote::addItem(std::unique_ptr<DeliveryNoteItem> item) {
    DeliveryNoteItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "DeliveryNote::addItem: позиция добавлена в накладную.\n";
    return result;
}

bool DeliveryNote::checkQuantity() const {
    const bool result = !items_.empty() && std::all_of(
        items_.begin(), items_.end(),
        [](const std::unique_ptr<DeliveryNoteItem>& item) { return item->isQuantityCorrect(); });
    std::cout << "DeliveryNote::checkQuantity: проверка количества — " << std::boolalpha << result << ".\n";
    return result;
}

void DeliveryNote::setStatus(DeliveryNoteStatus status) {
    status_ = status;
    std::cout << "DeliveryNote::setStatus: статус накладной — " << deliveryNoteStatusText(status_) << ".\n";
}

void DeliveryNote::printInfo() const {
    std::cout << "DeliveryNote::printInfo: накладная #" << id_ << " " << number_
              << ", позиций " << items_.size() << ".\n";
}

const std::vector<std::unique_ptr<DeliveryNoteItem>>& DeliveryNote::getItems() const { return items_; }
DeliveryNoteStatus DeliveryNote::getStatus() const { return status_; }

// Delivery

Delivery::Delivery(int id, std::shared_ptr<DeliveryNote> deliveryNote)
    : id_(id), deliveryNote_(std::move(deliveryNote)) {
    std::cout << "Delivery::Delivery: объект поставки создан.\n";
}

void Delivery::arrive() {
    status_ = DeliveryStatus::Arrived;
    std::cout << "Delivery::arrive: поставка прибыла на склад.\n";
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
    std::cout << "Delivery::printInfo: поставка #" << id_ << ", статус "
              << deliveryStatusText(status_) << ".\n";
}

const std::shared_ptr<DeliveryNote>& Delivery::getDeliveryNote() const { return deliveryNote_; }
DeliveryStatus Delivery::getStatus() const { return status_; }

// OrderItem

OrderItem::OrderItem(int id, Product* product, int quantity)
    : id_(id), quantity_(quantity), product_(product) {
    std::cout << "OrderItem::OrderItem: позиция заказа создана.\n";
}

void OrderItem::pick(int quantity) {
    pickedQuantity_ += quantity;
    std::cout << "OrderItem::pick: отобрано " << quantity << " шт. товара.\n";
}

bool OrderItem::isComplete() const {
    const bool result = pickedQuantity_ >= quantity_;
    std::cout << "OrderItem::isComplete: позиция собрана — " << std::boolalpha << result << ".\n";
    return result;
}

void OrderItem::addBatch(const std::shared_ptr<GoodsBatch>& batch) {
    batches_.push_back(batch);
    std::cout << "OrderItem::addBatch: партия добавлена к позиции заказа.\n";
}

void OrderItem::printInfo() const {
    std::cout << "OrderItem::printInfo: позиция #" << id_ << ", отобрано "
              << pickedQuantity_ << " из " << quantity_ << " шт.\n";
}

Product* OrderItem::getProduct() const { return product_; }
int OrderItem::getQuantity() const { return quantity_; }
int OrderItem::getPickedQuantity() const { return pickedQuantity_; }

// WarehouseOrder

WarehouseOrder::WarehouseOrder(int id) : id_(id) {
    std::cout << "WarehouseOrder::WarehouseOrder: заказ #" << id_ << " создан.\n";
}

OrderItem* WarehouseOrder::addItem(std::unique_ptr<OrderItem> item) {
    OrderItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "WarehouseOrder::addItem: позиция добавлена в заказ.\n";
    return result;
}

bool WarehouseOrder::checkCompleteness() const {
    const bool result = !items_.empty() && std::all_of(
        items_.begin(), items_.end(),
        [](const std::unique_ptr<OrderItem>& item) { return item->isComplete(); });
    std::cout << "WarehouseOrder::checkCompleteness: заказ укомплектован — " << std::boolalpha << result << ".\n";
    return result;
}

void WarehouseOrder::complete() {
    status_ = checkCompleteness() ? OrderStatus::Completed : OrderStatus::Picking;
    std::cout << "WarehouseOrder::complete: статус заказа — " << orderStatusText(status_) << ".\n";
}

void WarehouseOrder::printInfo() const {
    std::cout << "WarehouseOrder::printInfo: заказ #" << id_ << ", статус "
              << orderStatusText(status_) << ".\n";
}

const std::vector<std::unique_ptr<OrderItem>>& WarehouseOrder::getItems() const { return items_; }
OrderStatus WarehouseOrder::getStatus() const { return status_; }

// StocktakeItem

StocktakeItem::StocktakeItem(int id, Product* product, StorageCell* cell, int expectedQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), product_(product), storageCell_(cell) {
    std::cout << "StocktakeItem::StocktakeItem: позиция инвентаризации создана.\n";
}

void StocktakeItem::setActualQuantity(int quantity) {
    actualQuantity_ = quantity;
    std::cout << "StocktakeItem::setActualQuantity: фактическое количество сохранено.\n";
}

int StocktakeItem::calculateDifference() {
    difference_ = actualQuantity_ - expectedQuantity_;
    std::cout << "StocktakeItem::calculateDifference: расхождение — " << difference_ << " шт.\n";
    return difference_;
}

bool StocktakeItem::isCorrect() const {
    const bool result = difference_ == 0;
    std::cout << "StocktakeItem::isCorrect: остаток корректен — " << std::boolalpha << result << ".\n";
    return result;
}

void StocktakeItem::printInfo() const {
    const std::string productName = product_ == nullptr ? "не указан" : product_->getName();
    const std::string cellAddress = storageCell_ == nullptr ? "не назначена" : storageCell_->getAddress();
    std::cout << "StocktakeItem::printInfo: позиция #" << id_ << ", товар «" << productName
              << "», ячейка " << cellAddress << ", ожидается " << expectedQuantity_
              << ", фактически " << actualQuantity_ << " шт.\n";
}

// StocktakeSession

StocktakeSession::StocktakeSession(int id) : id_(id) {
    std::cout << "StocktakeSession::StocktakeSession: сессия инвентаризации создана.\n";
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
    const bool result = !items_.empty() && std::all_of(
        items_.begin(), items_.end(),
        [](const std::unique_ptr<StocktakeItem>& item) { return item->isCorrect(); });
    std::cout << "StocktakeSession::isComplete: инвентаризация завершена — " << std::boolalpha << result << ".\n";
    return result;
}

void StocktakeSession::printInfo() const {
    std::cout << "StocktakeSession::printInfo: сессия #" << id_
              << ", позиций проверяется — " << items_.size() << ".\n";
}

// ShipmentItem

ShipmentItem::ShipmentItem(int id, Product* product, OrderItem* orderItem,
                           int expectedQuantity, int actualQuantity)
    : id_(id), expectedQuantity_(expectedQuantity), actualQuantity_(actualQuantity),
      product_(product), orderItem_(orderItem) {
    std::cout << "ShipmentItem::ShipmentItem: позиция отгрузки создана.\n";
}

int ShipmentItem::calculateDifference() const {
    const int result = actualQuantity_ - expectedQuantity_;
    std::cout << "ShipmentItem::calculateDifference: расхождение — " << result << " шт.\n";
    return result;
}

bool ShipmentItem::isCorrect() const {
    const bool result = calculateDifference() == 0;
    std::cout << "ShipmentItem::isCorrect: позиция корректна — " << std::boolalpha << result << ".\n";
    return result;
}

void ShipmentItem::printInfo() const {
    const std::string productName = product_ == nullptr ? "не указан" : product_->getName();
    std::cout << "ShipmentItem::printInfo: позиция #" << id_ << ", товар «" << productName
              << "», связана с позицией заказа — " << (orderItem_ != nullptr ? "да" : "нет")
              << "; ожидается " << expectedQuantity_ << ", фактически "
              << actualQuantity_ << " шт.\n";
}

// Shipment

Shipment::Shipment(int id, WarehouseOrder* warehouseOrder)
    : id_(id), warehouseOrder_(warehouseOrder) {
    std::cout << "Shipment::Shipment: отгрузка создана.\n";
}

ShipmentItem* Shipment::addItem(std::unique_ptr<ShipmentItem> item) {
    ShipmentItem* result = item.get();
    items_.push_back(std::move(item));
    std::cout << "Shipment::addItem: позиция добавлена в отгрузку.\n";
    return result;
}

bool Shipment::checkReadiness() const {
    const bool result = !items_.empty() && std::all_of(
        items_.begin(), items_.end(),
        [](const std::unique_ptr<ShipmentItem>& item) { return item->isCorrect(); });
    std::cout << "Shipment::checkReadiness: отгрузка готова — " << std::boolalpha << result << ".\n";
    return result;
}

void Shipment::dispatch() {
    status_ = checkReadiness() ? ShipmentStatus::Shipped : ShipmentStatus::Preparing;
    std::cout << "Shipment::dispatch: операция отгрузки выполнена.\n";
}

void Shipment::printInfo() const {
    const std::size_t orderItems = warehouseOrder_ == nullptr ? 0 : warehouseOrder_->getItems().size();
    std::cout << "Shipment::printInfo: отгрузка #" << id_ << ", позиций заказа "
              << orderItems << ", позиций отгрузки — " << items_.size() << ".\n";
}

// Notification

Notification::Notification(int id, std::string message, std::string type, std::string recipientRole)
    : id_(id), message_(std::move(message)), type_(std::move(type)), recipientRole_(std::move(recipientRole)) {
    std::cout << "Notification::Notification: уведомление создано.\n";
}

void Notification::send() const {
    std::cout << "Notification::send: уведомление отправлено роли «" << recipientRole_ << "».\n";
}

void Notification::markAsRead() {
    isRead_ = true;
    std::cout << "Notification::markAsRead: уведомление прочитано.\n";
}

void Notification::printInfo() const {
    std::cout << "Notification::printInfo: уведомление #" << id_ << ", "
              << type_ << " — " << message_ << ".\n";
}

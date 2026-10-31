#pragma once

#include "storage.h"

#include <memory>
#include <string>
#include <vector>

enum class DeliveryNoteStatus { Created, Expected, Received, Completed, Discrepancy };
enum class DeliveryStatus { Expected, Arrived, Accepted, Discrepancy };
enum class OrderStatus { Created, Picking, Completed, Shipped };
enum class StocktakeStatus { Created, InProgress, Completed };
enum class ShipmentStatus { Created, Preparing, Ready, Shipped };

// Дата в формате ДД.ММ.ГГГГ (сегодня + daysFromToday).
std::string currentDate(int daysFromToday = 0);
// Дата ДД.ММ.ГГГГ -> число ГГГГММДД, чтобы даты можно было сравнивать.
int dateKey(const std::string& date);

class Product {
public:
    Product(int id, std::string name, std::string sku, double unitVolume);

    void printInfo() const;
    int getId() const;
    const std::string& getName() const;
    const std::string& getSku() const;
    double getUnitVolume() const;

private:
    int id_;
    std::string name_;
    std::string sku_;
    double unitVolume_;
};

class Notification {
public:
    Notification(int id, std::string message, std::string type, std::string recipientRole);

    void send() const;
    void markAsRead();
    void printInfo() const;
    bool isRead() const;

private:
    int id_;
    std::string message_;
    std::string date_;
    std::string type_;
    std::string recipientRole_;
    bool isRead_ = false;
};

class DeliveryNoteItem {
public:
    DeliveryNoteItem(int id, Product* product, int expectedQuantity);

    void setActualQuantity(int quantity);
    void setStorageCell(StorageCell* cell, double reservedVolume);
    int calculateDifference() const;
    bool isQuantityCorrect() const;
    void reject();
    void printInfo() const;

    Product* getProduct() const;
    int getExpectedQuantity() const;
    int getActualQuantity() const;
    StorageCell* getStorageCell() const;
    double getReservedVolume() const;
    bool isRejected() const;

private:
    int id_;
    int expectedQuantity_;
    int actualQuantity_ = 0;
    double reservedVolume_ = 0.0;
    bool rejected_ = false;
    Product* product_;
    StorageCell* storageCell_ = nullptr;
};

class Delivery;

class DeliveryNote {
public:
    DeliveryNote(int id, std::string number, std::string type, std::string dueDate);

    DeliveryNoteItem* addItem(std::unique_ptr<DeliveryNoteItem> item);
    bool checkQuantity() const;
    void setStatus(DeliveryNoteStatus status);
    void setDelivery(Delivery* delivery);
    std::unique_ptr<Notification> createNotification(int id, const std::string& message,
                                                     const std::string& role) const;
    void printInfo() const;

    static const char* statusText(DeliveryNoteStatus status);

    const std::vector<std::unique_ptr<DeliveryNoteItem>>& getItems() const;
    const std::string& getNumber() const;
    const std::string& getDueDate() const;
    DeliveryNoteStatus getStatus() const;
    Delivery* getDelivery() const;

private:
    int id_;
    std::string number_;
    std::string date_;
    std::string dueDate_;
    std::string type_;
    DeliveryNoteStatus status_ = DeliveryNoteStatus::Created;
    std::vector<std::unique_ptr<DeliveryNoteItem>> items_;
    Delivery* delivery_ = nullptr;
};

class Delivery {
public:
    Delivery(int id, std::shared_ptr<DeliveryNote> deliveryNote);

    void arrive();
    void accept();
    void acceptWithDiscrepancy();
    void printInfo() const;

    const std::shared_ptr<DeliveryNote>& getDeliveryNote() const;
    DeliveryStatus getStatus() const;

private:
    int id_;
    std::string arrivalDate_;
    DeliveryStatus status_ = DeliveryStatus::Expected;
    std::shared_ptr<DeliveryNote> deliveryNote_;
};

class OrderItem {
public:
    OrderItem(int id, Product* product, int quantity);

    void pick(int quantity);
    bool isComplete() const;
    void addBatch(const std::shared_ptr<GoodsBatch>& batch);
    void setPickedBatch(const std::shared_ptr<GoodsBatch>& batch);
    void printInfo() const;

    Product* getProduct() const;
    int getQuantity() const;
    int getPickedQuantity() const;
    const std::shared_ptr<GoodsBatch>& getPickedBatch() const;

private:
    int id_;
    int quantity_;
    int pickedQuantity_ = 0;
    Product* product_;
    std::vector<std::shared_ptr<GoodsBatch>> batches_;   // партии, из которых отобран товар
    std::shared_ptr<GoodsBatch> pickedBatch_;            // собранный товар в зоне отгрузки
};

class WarehouseOrder {
public:
    explicit WarehouseOrder(int id);

    OrderItem* addItem(std::unique_ptr<OrderItem> item);
    void startPicking();
    bool checkCompleteness() const;
    void complete();
    void ship();
    void printInfo() const;

    static const char* statusText(OrderStatus status);

    const std::vector<std::unique_ptr<OrderItem>>& getItems() const;
    int getId() const;
    OrderStatus getStatus() const;

private:
    int id_;
    OrderStatus status_ = OrderStatus::Created;
    std::string createdAt_;
    std::vector<std::unique_ptr<OrderItem>> items_;
};

class StocktakeItem {
public:
    StocktakeItem(int id, Product* product, StorageCell* cell, int expectedQuantity);

    void setActualQuantity(int quantity);
    int calculateDifference();
    bool isCorrect() const;
    void markForRecheck();
    bool isChecked() const;
    std::unique_ptr<Notification> createNotification(int id) const;
    void printInfo() const;

    int getDifference() const;
    int getActualQuantity() const;
    const std::string& getStatus() const;

private:
    int id_;
    int expectedQuantity_;
    int actualQuantity_ = 0;
    int difference_ = 0;
    std::string status_ = "не проверено";
    Product* product_;
    StorageCell* storageCell_;
};

class StocktakeSession {
public:
    explicit StocktakeSession(int id);

    void start();
    void finish();
    StocktakeItem* addItem(std::unique_ptr<StocktakeItem> item);
    bool isComplete() const;
    void printInfo() const;

private:
    int id_;
    std::string date_;
    StocktakeStatus status_ = StocktakeStatus::Created;
    std::vector<std::unique_ptr<StocktakeItem>> items_;
};

class ShipmentItem {
public:
    ShipmentItem(int id, Product* product, OrderItem* orderItem,
                 int expectedQuantity, int actualQuantity);

    int calculateDifference() const;
    bool isCorrect() const;
    std::unique_ptr<Notification> createNotification(int id) const;
    void printInfo() const;

    OrderItem* getOrderItem() const;

private:
    int id_;
    int expectedQuantity_;
    int actualQuantity_;
    Product* product_;
    OrderItem* orderItem_;
};

class Shipment {
public:
    Shipment(int id, WarehouseOrder* warehouseOrder);

    ShipmentItem* addItem(std::unique_ptr<ShipmentItem> item);
    bool checkReadiness();
    bool dispatch();
    void printInfo() const;

    const std::vector<std::unique_ptr<ShipmentItem>>& getItems() const;

private:
    int id_;
    std::string date_;
    ShipmentStatus status_ = ShipmentStatus::Created;
    WarehouseOrder* warehouseOrder_;
    std::vector<std::unique_ptr<ShipmentItem>> items_;
};

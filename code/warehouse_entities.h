#pragma once

#include "storage.h"

#include <memory>
#include <string>
#include <vector>

enum class BatchStatus { Received, Stored, Reserved, Depleted };
enum class DeliveryNoteStatus { Created, Expected, Received, Completed, Discrepancy };
enum class DeliveryStatus { Expected, Arrived, Accepted, Discrepancy };
enum class OrderStatus { Created, Picking, Completed, Shipped };
enum class StocktakeStatus { Created, InProgress, Completed };
enum class ShipmentStatus { Created, Preparing, Ready, Shipped };

class Product {
public:
    Product(int id, std::string name, std::string sku, double unitVolume);

    void printInfo() const;
    int getId() const;
    const std::string& getName() const;
    double getUnitVolume() const;

private:
    int id_;
    std::string name_;
    std::string sku_;
    double unitVolume_;
};

class DeliveryNoteItem {
public:
    DeliveryNoteItem(int id, Product* product, int expectedQuantity);

    void setActualQuantity(int quantity);
    void setStorageCell(StorageCell* cell, double reservedVolume);
    int calculateDifference() const;
    bool isQuantityCorrect() const;
    void printInfo() const;

    Product* getProduct() const;
    int getExpectedQuantity() const;
    int getActualQuantity() const;
    StorageCell* getStorageCell() const;
    double getReservedVolume() const;

private:
    int id_;
    int expectedQuantity_;
    int actualQuantity_ = 0;
    double reservedVolume_ = 0.0;
    Product* product_;
    StorageCell* storageCell_ = nullptr;
};

class Delivery;

class DeliveryNote {
public:
    DeliveryNote(int id, std::string number, std::string date, std::string dueDate);

    DeliveryNoteItem* addItem(std::unique_ptr<DeliveryNoteItem> item);
    bool checkQuantity() const;
    void setStatus(DeliveryNoteStatus status);
    void printInfo() const;

    const std::vector<std::unique_ptr<DeliveryNoteItem>>& getItems() const;
    DeliveryNoteStatus getStatus() const;

private:
    int id_;
    std::string number_;
    std::string date_;
    std::string dueDate_;
    DeliveryNoteStatus status_ = DeliveryNoteStatus::Created;
    std::vector<std::unique_ptr<DeliveryNoteItem>> items_;
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
    DeliveryStatus status_ = DeliveryStatus::Expected;
    std::shared_ptr<DeliveryNote> deliveryNote_;
};

class OrderItem {
public:
    OrderItem(int id, Product* product, int quantity);

    void pick(int quantity);
    bool isComplete() const;
    void addBatch(const std::shared_ptr<GoodsBatch>& batch);
    void printInfo() const;

    Product* getProduct() const;
    int getQuantity() const;
    int getPickedQuantity() const;

private:
    int id_;
    int quantity_;
    int pickedQuantity_ = 0;
    Product* product_;
    std::vector<std::shared_ptr<GoodsBatch>> batches_;
};

class WarehouseOrder {
public:
    explicit WarehouseOrder(int id);

    OrderItem* addItem(std::unique_ptr<OrderItem> item);
    bool checkCompleteness() const;
    void complete();
    void printInfo() const;

    const std::vector<std::unique_ptr<OrderItem>>& getItems() const;
    OrderStatus getStatus() const;

private:
    int id_;
    OrderStatus status_ = OrderStatus::Created;
    std::vector<std::unique_ptr<OrderItem>> items_;
};

class StocktakeItem {
public:
    StocktakeItem(int id, Product* product, StorageCell* cell, int expectedQuantity);

    void setActualQuantity(int quantity);
    int calculateDifference();
    bool isCorrect() const;
    void printInfo() const;

private:
    int id_;
    int expectedQuantity_;
    int actualQuantity_ = 0;
    int difference_ = 0;
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
    StocktakeStatus status_ = StocktakeStatus::Created;
    std::vector<std::unique_ptr<StocktakeItem>> items_;
};

class ShipmentItem {
public:
    ShipmentItem(int id, Product* product, OrderItem* orderItem,
                 int expectedQuantity, int actualQuantity);

    int calculateDifference() const;
    bool isCorrect() const;
    void printInfo() const;

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
    bool checkReadiness() const;
    void dispatch();
    void printInfo() const;

private:
    int id_;
    ShipmentStatus status_ = ShipmentStatus::Created;
    WarehouseOrder* warehouseOrder_;
    std::vector<std::unique_ptr<ShipmentItem>> items_;
};

class Notification {
public:
    Notification(int id, std::string message, std::string type, std::string recipientRole);

    void send() const;
    void markAsRead();
    void printInfo() const;

private:
    int id_;
    std::string message_;
    std::string type_;
    std::string recipientRole_;
    bool isRead_ = false;
};

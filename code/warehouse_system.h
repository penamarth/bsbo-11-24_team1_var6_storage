#pragma once

#include "warehouse_entities.h"

#include <memory>
#include <string>
#include <vector>

class WarehouseSystem {
public:
    WarehouseSystem();

    void runMenu();

    // Методы из диаграммы классов.
    void registerDelivery();
    void receiveDelivery();
    void placeGoods();
    void createOrder();
    void pickOrder(WarehouseOrder* order);
    void startStocktake();
    void createShipment();
    void checkDeliveryDates();
    void cancelDelivery();

    void showWarehouseStructure() const;
    void showNotifications();

private:
    // Пункты меню, в которых несколько действий.
    void deliveryMenu();
    void orderMenu();
    void cancelOrder(WarehouseOrder* order);

    // Вывод на экран.
    void printMenu() const;
    void printStatusBar() const;
    void printScenarioTitle(const std::string& title) const;
    void printClassChain(const std::vector<std::string>& chain) const;
    void printMethodsHeader() const;
    void printProducts() const;

    // Ввод с клавиатуры: Enter без числа подставляет значение по умолчанию.
    bool readLine(std::string& line);
    int readMenuChoice();
    int readInt(const std::string& prompt, int minValue, int maxValue, int defaultValue);
    Product* chooseProduct(int defaultNumber);
    void waitForEnter();

    void addNotification(std::unique_ptr<Notification> notification);
    void addNotification(const std::string& message, const std::string& type, const std::string& role);
    std::vector<std::shared_ptr<DeliveryNote>> getExpectedNotes() const;
    std::shared_ptr<DeliveryNote> chooseExpectedNote();
    int countFreeUnits(double unitVolume) const;
    std::vector<WarehouseOrder*> getOrdersWithStatus(OrderStatus status) const;
    int countUnreadNotifications() const;

    std::unique_ptr<Warehouse> warehouse_;
    std::vector<std::unique_ptr<BufferZone>> bufferZones_;
    BufferZone* receivingZone_ = nullptr;
    BufferZone* shippingZone_ = nullptr;

    std::vector<std::unique_ptr<Product>> products_;
    std::vector<std::shared_ptr<DeliveryNote>> deliveryNotes_;
    std::vector<std::shared_ptr<Delivery>> deliveries_;
    std::vector<std::unique_ptr<WarehouseOrder>> orders_;
    std::vector<std::unique_ptr<StocktakeSession>> stocktakes_;
    std::vector<std::unique_ptr<Shipment>> shipments_;
    std::vector<std::unique_ptr<Notification>> notifications_;

    std::string menuMessage_;    // сообщение под меню (например, об ошибке ввода)
    bool inputClosed_ = false;   // ввод закончился (Ctrl+D / конец файла)
    int nextId_ = 100;
    int deliveryCounter_ = 0;
    int orderCounter_ = 0;
    int shipmentCounter_ = 0;
    int stocktakeCounter_ = 0;
};

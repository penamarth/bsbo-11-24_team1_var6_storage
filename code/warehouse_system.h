#pragma once

#include "warehouse_entities.h"

#include <memory>
#include <string>
#include <vector>

class WarehouseSystem {
public:
    WarehouseSystem();

    void runMenu();
    void showWarehouseStructure() const;
    void runReceivingScenario();
    void runPlacementScenario();
    void runOrderScenario();
    void runStocktakeScenario();
    void runShipmentScenario();
    void showNotifications() const;

private:
    void printClassChain(const std::string& chain) const;
    void printMenu() const;
    int readMenuChoice() const;
    void pauseAfterScenario() const;
    Product* getDemoProduct(std::size_t index) const;

    std::unique_ptr<Warehouse> warehouse_;
    std::vector<std::unique_ptr<Product>> products_;
    std::vector<std::shared_ptr<Delivery>> deliveries_;
    std::vector<std::unique_ptr<WarehouseOrder>> orders_;
    std::vector<std::unique_ptr<StocktakeSession>> stocktakes_;
    std::vector<std::unique_ptr<Shipment>> shipments_;
    std::vector<std::unique_ptr<Notification>> notifications_;

    std::shared_ptr<GoodsBatch> demoBatch_;
    StorageCell* demoCell_ = nullptr;
    WarehouseOrder* demoOrder_ = nullptr;
    int nextId_ = 100;
};

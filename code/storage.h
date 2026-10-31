#pragma once

#include <memory>
#include <string>
#include <vector>

class StorageLocation;
class StorageCell;
class Product;

// Статус партии товара.
enum class BatchStatus { Received, Stored, Reserved, Depleted };

class GoodsBatch {
public:
    GoodsBatch(int id, double volume, std::string productName, int quantity = 0, Product* product = nullptr);

    void receive();
    bool reserveQuantity(int quantity);
    void decreaseQuantity(int quantity, bool fromReserve = false);
    void changeQuantity(int newQuantity);
    int getAvailableQuantity() const;
    std::shared_ptr<GoodsBatch> split(int newId, int partQuantity);
    double getUnitVolume() const;
    void updateStatus();
    void printInfo(const std::string& indent = "") const;

    static const char* statusText(BatchStatus status);

    int id;
    double volume;
    std::string productName;
    int quantity = 0;
    int reservedQuantity = 0;
    Product* product = nullptr;
    StorageLocation* location = nullptr;
    BatchStatus status = BatchStatus::Received;

private:
    void updateVolume();

    double unitVolume_ = 0.0;
};

// Абстрактный базовый класс мест хранения. От него наследуются только ячейка и буферная зона.
class StorageLocation {
public:
    StorageLocation(int id, double capacity);
    virtual ~StorageLocation() = default;

    virtual bool hasSpace(double volume) const = 0;
    virtual double getAvailableVolume() const = 0;
    virtual bool placeBatch(const std::shared_ptr<GoodsBatch>& batch) = 0;
    virtual bool removeBatch(GoodsBatch* batch) = 0;
    virtual void inventory() const = 0;
    virtual std::string getTitle() const = 0;

    // Геттеры ничего не печатают — они нужны для строки состояния меню.
    int getId() const;
    double getCapacity() const;
    double getCurrentVolume() const;   // занятый объём = сумма объёмов партий
    const std::vector<std::shared_ptr<GoodsBatch>>& getBatches() const;

protected:
    static bool isValidVolume(double volume);
    bool addBatch(const std::shared_ptr<GoodsBatch>& batch);
    bool eraseBatch(GoodsBatch* batch);

    int id_;
    double capacity_;
    std::vector<std::shared_ptr<GoodsBatch>> batches_;
};

class StorageCell final : public StorageLocation {
public:
    StorageCell(int id, std::string address, double capacity);

    bool hasSpace(double volume) const override;
    double getAvailableVolume() const override;
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch) override;
    bool removeBatch(GoodsBatch* batch) override;
    void inventory() const override;
    std::string getTitle() const override;

    bool reserveSpace(double volume);
    void releaseSpace(double volume);

    const std::string& getAddress() const;
    double getReservedVolume() const;

private:
    double freeVolume() const;

    std::string address_;
    double reservedVolume_ = 0.0;
};

class BufferZone final : public StorageLocation {
public:
    BufferZone(int id, std::string name, double capacity);

    bool hasSpace(double volume) const override;
    double getAvailableVolume() const override;
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch) override;
    bool removeBatch(GoodsBatch* batch) override;
    void inventory() const override;
    std::string getTitle() const override;

    const std::string& getName() const;

private:
    std::string name_;
};

// Shelf НЕ наследуется от StorageLocation. Это составная часть шкафа.
class Shelf final {
public:
    Shelf(int id, std::string number);

    void addCell(std::unique_ptr<StorageCell> cell);
    bool hasSpace(double volume) const;
    StorageCell* findFreeCell(double volume) const;
    double getAvailableVolume() const;
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch);
    bool removeBatch(GoodsBatch* batch);
    void inventory() const;

    int getId() const;
    const std::string& getNumber() const;
    const std::vector<std::unique_ptr<StorageCell>>& getCells() const;

private:
    int id_;
    std::string number_;
    std::vector<std::unique_ptr<StorageCell>> cells_;
};

// StorageCabinet НЕ наследуется от StorageLocation. Это составная часть склада.
class StorageCabinet final {
public:
    StorageCabinet(int id, std::string number);

    void addShelf(std::unique_ptr<Shelf> shelf);
    bool hasSpace(double volume) const;
    StorageCell* findFreeCell(double volume) const;
    double getAvailableVolume() const;
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch);
    bool removeBatch(GoodsBatch* batch);
    void inventory() const;

    int getId() const;
    const std::string& getNumber() const;
    const std::vector<std::unique_ptr<Shelf>>& getShelves() const;

private:
    int id_;
    std::string number_;
    std::vector<std::unique_ptr<Shelf>> shelves_;
};

// Warehouse: шкафы входят в склад (композиция),
// буферные зоны склад только использует (агрегация) — ими владеет WarehouseSystem.
class Warehouse final {
public:
    Warehouse(int id, std::string name, std::string address);

    void addCabinet(std::unique_ptr<StorageCabinet> cabinet);
    void addBufferZone(BufferZone* bufferZone);
    StorageCell* findFreeCell(double volume) const;
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch);
    bool removeBatch(GoodsBatch* batch);
    std::vector<std::shared_ptr<GoodsBatch>> findBatches(const std::string& productName) const;
    double getAvailableVolume() const;
    void inventory() const;

    // Геттеры без вывода в консоль.
    std::vector<StorageCell*> getAllCells() const;
    int getId() const;
    const std::string& getName() const;
    const std::string& getAddress() const;
    const std::vector<std::unique_ptr<StorageCabinet>>& getCabinets() const;
    const std::vector<BufferZone*>& getBufferZones() const;

private:
    int id_;
    std::string name_;
    std::string address_;
    std::vector<std::unique_ptr<StorageCabinet>> cabinets_;
    std::vector<BufferZone*> bufferZones_;
};

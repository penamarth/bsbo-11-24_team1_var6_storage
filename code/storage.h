#pragma once

#include <memory>
#include <string>
#include <vector>

class StorageLocation;
class Product;

// Статус партии товара.
enum class BatchStatus { Received, Stored, InBuffer, Reserved, Depleted, Shipped };

class GoodsBatch {
public:
    GoodsBatch(int id, double volume, std::string productName, int quantity = 0, Product* product = nullptr);

    void receive();
    bool reserveQuantity(int quantity);
    void decreaseQuantity(int quantity, bool fromReserve = false);
    void changeQuantity(int newQuantity);
    int getAvailableQuantity() const;
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

// Абстрактный общий класс только для непосредственных мест хранения.
class StorageLocation {
public:
    virtual ~StorageLocation() = default;

    virtual bool hasSpace(double volume) const = 0;
    virtual double getAvailableVolume() const = 0;
    virtual bool placeBatch(const std::shared_ptr<GoodsBatch>& batch) = 0;
    virtual bool removeBatch(GoodsBatch* batch) = 0;
    virtual void inventory() const = 0;
    virtual std::string getTitle() const = 0;

protected:
    static bool isValidVolume(double volume);
    static double sumVolume(const std::vector<std::shared_ptr<GoodsBatch>>& batches);
};

// Ячейка и буферная зона — единственные классы-наследники StorageLocation.
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

    // Геттеры ничего не печатают — они нужны для строки состояния меню.
    const std::vector<std::shared_ptr<GoodsBatch>>& getBatches() const;
    int getId() const;
    const std::string& getAddress() const;
    double getCapacity() const;
    double getUsedVolume() const;
    double getReservedVolume() const;

private:
    double freeVolume() const;

    int id_;
    std::string address_;
    double capacity_;
    double reservedVolume_ = 0.0;
    std::vector<std::shared_ptr<GoodsBatch>> batches_;
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

    const std::vector<std::shared_ptr<GoodsBatch>>& getBatches() const;
    int getId() const;
    const std::string& getName() const;
    double getCapacity() const;
    double getUsedVolume() const;

private:
    int id_;
    std::string name_;
    double capacity_;
    std::vector<std::shared_ptr<GoodsBatch>> batches_;
};

// Shelf НЕ наследуется от StorageLocation. Это составная часть шкафа.
class Shelf final {
public:
    Shelf(int id, std::string number);

    void addCell(std::unique_ptr<StorageCell> cell);
    bool hasSpace(double volume) const;
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

// Warehouse — верхний объект композиции: содержит шкафы и буферные зоны.
class Warehouse final {
public:
    Warehouse(int id, std::string name, std::string address);

    void addCabinet(std::unique_ptr<StorageCabinet> cabinet);
    BufferZone* addBufferZone(std::unique_ptr<BufferZone> bufferZone);
    bool placeBatch(const std::shared_ptr<GoodsBatch>& batch);
    StorageCell* findFreeCell(double volume) const;
    std::vector<std::shared_ptr<GoodsBatch>> findBatches(const std::string& productName) const;
    double getAvailableVolume() const;
    void inventory() const;

    // Геттеры без вывода в консоль.
    std::vector<StorageCell*> getAllCells() const;
    int getId() const;
    const std::string& getName() const;
    const std::string& getAddress() const;
    const std::vector<std::unique_ptr<StorageCabinet>>& getCabinets() const;
    const std::vector<std::unique_ptr<BufferZone>>& getBufferZones() const;

private:
    int id_;
    std::string name_;
    std::string address_;
    std::vector<std::unique_ptr<StorageCabinet>> cabinets_;
    std::vector<std::unique_ptr<BufferZone>> bufferZones_;
};

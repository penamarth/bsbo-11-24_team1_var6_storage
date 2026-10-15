#include "storage.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <utility>

namespace {

bool removeFromBatches(std::vector<std::shared_ptr<GoodsBatch>>& batches,
                       GoodsBatch* batch,
                       double& currentVolume) {
    if (batch == nullptr) {
        return false;
    }

    const auto it = std::find_if(
        batches.begin(), batches.end(),
        [batch](const std::shared_ptr<GoodsBatch>& item) { return item.get() == batch; });

    if (it == batches.end()) {
        return false;
    }

    currentVolume -= (*it)->volume;
    if (currentVolume < 0.0) {
        currentVolume = 0.0;
    }
    batch->location = nullptr;
    batches.erase(it);
    return true;
}

void printVolume(double value) {
    std::cout << std::fixed << std::setprecision(2) << value;
}

} // namespace

GoodsBatch::GoodsBatch(int id, double volume, std::string productName, int quantity, Product* product)
    : id(id), volume(volume), productName(std::move(productName)), quantity(quantity), product(product) {
    std::cout << "GoodsBatch::GoodsBatch: партия #" << id << " создана.\n";
}

void GoodsBatch::receive() {
    std::cout << "GoodsBatch::receive: партия #" << id << " принята.\n";
}

void GoodsBatch::reserveQuantity(int value) {
    reservedQuantity += value;
    std::cout << "GoodsBatch::reserveQuantity: зарезервировано " << value
              << " шт. партии #" << id << ".\n";
}

void GoodsBatch::decreaseQuantity(int value) {
    quantity = std::max(0, quantity - value);
    reservedQuantity = std::max(0, reservedQuantity - value);
    std::cout << "GoodsBatch::decreaseQuantity: количество партии #" << id
              << " уменьшено на " << value << " шт.\n";
}

int GoodsBatch::getAvailableQuantity() const {
    const int result = quantity - reservedQuantity;
    std::cout << "GoodsBatch::getAvailableQuantity: доступно " << result
              << " шт. партии #" << id << ".\n";
    return result;
}

bool StorageLocation::isValidVolume(double volume) {
    return volume > 0.0;
}

// StorageCell

StorageCell::StorageCell(int id, std::string address, double capacity)
    : id_(id), address_(std::move(address)), capacity_(capacity) {
    std::cout << "StorageCell::StorageCell: ячейка " << address_ << " создана.\n";
}

bool StorageCell::hasSpace(double volume) const {
    const bool result = isValidVolume(volume) && getAvailableVolume() >= volume - 0.001;
    std::cout << "StorageCell::hasSpace: ячейка " << address_
              << ", места достаточно — " << std::boolalpha << result << ".\n";
    return result;
}

double StorageCell::getAvailableVolume() const {
    const double freeVolume = capacity_ - currentVolume_ - reservedVolume_;
    const double result = freeVolume > 0.0 ? freeVolume : 0.0;
    std::cout << "StorageCell::getAvailableVolume: в ячейке " << address_ << " свободно ";
    printVolume(result);
    std::cout << " м³.\n";
    return result;
}

bool StorageCell::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch || !isValidVolume(batch->volume)) {
        std::cout << "StorageCell::placeBatch: некорректная партия.\n";
        return false;
    }

    // Резерв партии уже означает, что для неё место найдено.
    if (capacity_ - currentVolume_ < batch->volume - 0.001) {
        std::cout << "StorageCell::placeBatch: в ячейке " << address_ << " недостаточно места.\n";
        return false;
    }

    if (batch->location != nullptr && batch->location != this) {
        batch->location->removeBatch(batch.get());
    }

    if (reservedVolume_ >= batch->volume) {
        reservedVolume_ -= batch->volume;
    }

    currentVolume_ += batch->volume;
    batch->location = this;
    batches_.push_back(batch);
    std::cout << "StorageCell::placeBatch: партия #" << batch->id
              << " размещена в ячейке " << address_ << ".\n";
    return true;
}

void StorageCell::removeBatch(GoodsBatch* batch) {
    if (removeFromBatches(batches_, batch, currentVolume_)) {
        std::cout << "StorageCell::removeBatch: партия #" << batch->id
                  << " удалена из ячейки " << address_ << ".\n";
    } else {
        std::cout << "StorageCell::removeBatch: партия не найдена в ячейке " << address_ << ".\n";
    }
}

void StorageCell::inventory() const {
    std::cout << "StorageCell::inventory: " << address_ << " | вместимость ";
    printVolume(capacity_);
    std::cout << " м³ | занято ";
    printVolume(currentVolume_);
    std::cout << " м³ | резерв ";
    printVolume(reservedVolume_);
    std::cout << " м³ | партий " << batches_.size() << ".\n";
}

void StorageCell::reserveSpace(double volume) {
    if (!isValidVolume(volume) || !hasSpace(volume)) {
        std::cout << "StorageCell::reserveSpace: место в ячейке " << address_
                  << " зарезервировать нельзя.\n";
        return;
    }
    reservedVolume_ += volume;
    std::cout << "StorageCell::reserveSpace: в ячейке " << address_ << " зарезервировано ";
    printVolume(volume);
    std::cout << " м³.\n";
}

void StorageCell::releaseSpace(double volume) {
    reservedVolume_ = std::max(0.0, reservedVolume_ - volume);
    std::cout << "StorageCell::releaseSpace: резерв ячейки " << address_ << " уменьшен на ";
    printVolume(volume);
    std::cout << " м³.\n";
}

const std::vector<std::shared_ptr<GoodsBatch>>& StorageCell::getBatches() const {
    std::cout << "StorageCell::getBatches: получен список партий ячейки " << address_ << ".\n";
    return batches_;
}

int StorageCell::getId() const { return id_; }
const std::string& StorageCell::getAddress() const { return address_; }
double StorageCell::getReservedVolume() const { return reservedVolume_; }

// BufferZone

BufferZone::BufferZone(int id, std::string name, double capacity)
    : id_(id), name_(std::move(name)), capacity_(capacity) {
    std::cout << "BufferZone::BufferZone: зона «" << name_ << "» создана.\n";
}

bool BufferZone::hasSpace(double volume) const {
    const bool result = isValidVolume(volume) && getAvailableVolume() >= volume - 0.001;
    std::cout << "BufferZone::hasSpace: зона «" << name_
              << "» места достаточно — " << std::boolalpha << result << ".\n";
    return result;
}

double BufferZone::getAvailableVolume() const {
    const double freeVolume = capacity_ - currentVolume_;
    const double result = freeVolume > 0.0 ? freeVolume : 0.0;
    std::cout << "BufferZone::getAvailableVolume: в зоне «" << name_ << "» свободно ";
    printVolume(result);
    std::cout << " м³.\n";
    return result;
}

bool BufferZone::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch || !hasSpace(batch->volume)) {
        std::cout << "BufferZone::placeBatch: партию разместить нельзя.\n";
        return false;
    }

    if (batch->location != nullptr && batch->location != this) {
        batch->location->removeBatch(batch.get());
    }

    currentVolume_ += batch->volume;
    batch->location = this;
    batches_.push_back(batch);
    std::cout << "BufferZone::placeBatch: партия #" << batch->id
              << " размещена в зоне «" << name_ << "».\n";
    return true;
}

void BufferZone::removeBatch(GoodsBatch* batch) {
    if (removeFromBatches(batches_, batch, currentVolume_)) {
        std::cout << "BufferZone::removeBatch: партия #" << batch->id
                  << " удалена из зоны «" << name_ << "».\n";
    } else {
        std::cout << "BufferZone::removeBatch: партия не найдена в зоне «" << name_ << "».\n";
    }
}

void BufferZone::inventory() const {
    std::cout << "BufferZone::inventory: «" << name_ << "» | вместимость ";
    printVolume(capacity_);
    std::cout << " м³ | занято ";
    printVolume(currentVolume_);
    std::cout << " м³ | партий " << batches_.size() << ".\n";
}

int BufferZone::getId() const { return id_; }
const std::string& BufferZone::getName() const { return name_; }

// Shelf

Shelf::Shelf(int id, std::string number) : id_(id), number_(std::move(number)) {
    std::cout << "Shelf::Shelf: полка " << number_ << " создана.\n";
}

void Shelf::addCell(std::unique_ptr<StorageCell> cell) {
    if (!cell) {
        std::cout << "Shelf::addCell: пустую ячейку добавить нельзя.\n";
        return;
    }
    cells_.push_back(std::move(cell));
    std::cout << "Shelf::addCell: ячейка добавлена на полку " << number_ << ".\n";
}

bool Shelf::hasSpace(double volume) const {
    const bool result = std::any_of(cells_.begin(), cells_.end(),
        [volume](const std::unique_ptr<StorageCell>& cell) { return cell->hasSpace(volume); });
    std::cout << "Shelf::hasSpace: на полке " << number_ << " места достаточно — "
              << std::boolalpha << result << ".\n";
    return result;
}

double Shelf::getAvailableVolume() const {
    double total = 0.0;
    for (const auto& cell : cells_) total += cell->getAvailableVolume();
    std::cout << "Shelf::getAvailableVolume: на полке " << number_ << " свободно ";
    printVolume(total);
    std::cout << " м³.\n";
    return total;
}

bool Shelf::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch) {
        std::cout << "Shelf::placeBatch: пустую партию разместить нельзя.\n";
        return false;
    }
    for (const auto& cell : cells_) {
        if (cell->placeBatch(batch)) {
            std::cout << "Shelf::placeBatch: партия размещена на полке " << number_ << ".\n";
            return true;
        }
    }
    std::cout << "Shelf::placeBatch: на полке " << number_ << " нет места.\n";
    return false;
}

void Shelf::removeBatch(GoodsBatch* batch) {
    for (const auto& cell : cells_) cell->removeBatch(batch);
    std::cout << "Shelf::removeBatch: выполнен поиск партии на полке " << number_ << ".\n";
}

void Shelf::inventory() const {
    std::cout << "Shelf::inventory: полка " << number_ << ".\n";
    for (const auto& cell : cells_) cell->inventory();
}

int Shelf::getId() const { return id_; }
const std::string& Shelf::getNumber() const { return number_; }
const std::vector<std::unique_ptr<StorageCell>>& Shelf::getCells() const { return cells_; }

// StorageCabinet

StorageCabinet::StorageCabinet(int id, std::string number)
    : id_(id), number_(std::move(number)) {
    std::cout << "StorageCabinet::StorageCabinet: шкаф " << number_ << " создан.\n";
}

void StorageCabinet::addShelf(std::unique_ptr<Shelf> shelf) {
    if (!shelf) {
        std::cout << "StorageCabinet::addShelf: пустую полку добавить нельзя.\n";
        return;
    }
    shelves_.push_back(std::move(shelf));
    std::cout << "StorageCabinet::addShelf: полка добавлена в шкаф " << number_ << ".\n";
}

bool StorageCabinet::hasSpace(double volume) const {
    const bool result = std::any_of(shelves_.begin(), shelves_.end(),
        [volume](const std::unique_ptr<Shelf>& shelf) { return shelf->hasSpace(volume); });
    std::cout << "StorageCabinet::hasSpace: в шкафу " << number_ << " места достаточно — "
              << std::boolalpha << result << ".\n";
    return result;
}

double StorageCabinet::getAvailableVolume() const {
    double total = 0.0;
    for (const auto& shelf : shelves_) total += shelf->getAvailableVolume();
    std::cout << "StorageCabinet::getAvailableVolume: в шкафу " << number_ << " свободно ";
    printVolume(total);
    std::cout << " м³.\n";
    return total;
}

bool StorageCabinet::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch) {
        std::cout << "StorageCabinet::placeBatch: пустую партию разместить нельзя.\n";
        return false;
    }
    for (const auto& shelf : shelves_) {
        if (shelf->placeBatch(batch)) {
            std::cout << "StorageCabinet::placeBatch: партия размещена в шкафу " << number_ << ".\n";
            return true;
        }
    }
    std::cout << "StorageCabinet::placeBatch: в шкафу " << number_ << " нет места.\n";
    return false;
}

void StorageCabinet::removeBatch(GoodsBatch* batch) {
    for (const auto& shelf : shelves_) shelf->removeBatch(batch);
    std::cout << "StorageCabinet::removeBatch: выполнен поиск партии в шкафу " << number_ << ".\n";
}

void StorageCabinet::inventory() const {
    std::cout << "StorageCabinet::inventory: шкаф " << number_ << ".\n";
    for (const auto& shelf : shelves_) shelf->inventory();
}

int StorageCabinet::getId() const { return id_; }
const std::string& StorageCabinet::getNumber() const { return number_; }
const std::vector<std::unique_ptr<Shelf>>& StorageCabinet::getShelves() const { return shelves_; }

// Warehouse

Warehouse::Warehouse(int id, std::string name, std::string address)
    : id_(id), name_(std::move(name)), address_(std::move(address)) {
    std::cout << "Warehouse::Warehouse: склад «" << name_ << "» создан.\n";
}

void Warehouse::addCabinet(std::unique_ptr<StorageCabinet> cabinet) {
    if (!cabinet) {
        std::cout << "Warehouse::addCabinet: пустой шкаф добавить нельзя.\n";
        return;
    }
    cabinets_.push_back(std::move(cabinet));
    std::cout << "Warehouse::addCabinet: шкаф добавлен на склад «" << name_ << "».\n";
}

void Warehouse::addBufferZone(std::unique_ptr<BufferZone> bufferZone) {
    if (!bufferZone) {
        std::cout << "Warehouse::addBufferZone: пустую зону добавить нельзя.\n";
        return;
    }
    bufferZones_.push_back(std::move(bufferZone));
    std::cout << "Warehouse::addBufferZone: буферная зона добавлена на склад.\n";
}

StorageCell* Warehouse::findFreeCell(double volume) const {
    for (const auto& cabinet : cabinets_) {
        for (const auto& shelf : cabinet->getShelves()) {
            for (const auto& cell : shelf->getCells()) {
                if (cell->hasSpace(volume)) {
                    std::cout << "Warehouse::findFreeCell: найдена ячейка " << cell->getAddress() << ".\n";
                    return cell.get();
                }
            }
        }
    }
    std::cout << "Warehouse::findFreeCell: свободная ячейка не найдена.\n";
    return nullptr;
}

bool Warehouse::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    for (const auto& cabinet : cabinets_) {
        if (cabinet->placeBatch(batch)) {
            std::cout << "Warehouse::placeBatch: партия размещена на складе.\n";
            return true;
        }
    }
    std::cout << "Warehouse::placeBatch: на складе нет свободного места.\n";
    return false;
}

double Warehouse::getAvailableVolume() const {
    double total = 0.0;
    for (const auto& cabinet : cabinets_) total += cabinet->getAvailableVolume();
    std::cout << "Warehouse::getAvailableVolume: на складе свободно ";
    printVolume(total);
    std::cout << " м³.\n";
    return total;
}

void Warehouse::inventory() const {
    std::cout << "Warehouse::inventory: склад «" << name_ << "», адрес: " << address_ << ".\n";
    for (const auto& cabinet : cabinets_) cabinet->inventory();
    for (const auto& zone : bufferZones_) zone->inventory();
}

int Warehouse::getId() const { return id_; }
const std::string& Warehouse::getName() const { return name_; }
const std::string& Warehouse::getAddress() const { return address_; }
const std::vector<std::unique_ptr<StorageCabinet>>& Warehouse::getCabinets() const { return cabinets_; }
const std::vector<std::unique_ptr<BufferZone>>& Warehouse::getBufferZones() const { return bufferZones_; }

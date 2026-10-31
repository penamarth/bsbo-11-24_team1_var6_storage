#include "storage.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <utility>

namespace {

const double kEpsilon = 0.0001;

void printVolume(double value) {
    std::cout << std::fixed << std::setprecision(2) << value;
}

} // namespace

// GoodsBatch

GoodsBatch::GoodsBatch(int batchId, double batchVolume, std::string name, int batchQuantity, Product* batchProduct)
    : id(batchId), volume(batchVolume), productName(std::move(name)), quantity(batchQuantity), product(batchProduct) {
    unitVolume_ = quantity > 0 ? volume / quantity : 0.0;
    std::cout << "GoodsBatch::GoodsBatch: партия #" << id << " («" << productName
              << "», " << quantity << " шт.) создана.\n";
}

void GoodsBatch::receive() {
    updateStatus();
    std::cout << "GoodsBatch::receive: партия #" << id << " принята, статус — "
              << statusText(status) << ".\n";
}

bool GoodsBatch::reserveQuantity(int value) {
    if (value <= 0 || value > quantity - reservedQuantity) {
        std::cout << "GoodsBatch::reserveQuantity: нельзя зарезервировать " << value
                  << " шт. партии #" << id << ".\n";
        return false;
    }
    reservedQuantity += value;
    updateStatus();
    std::cout << "GoodsBatch::reserveQuantity: зарезервировано " << value
              << " шт. партии #" << id << ".\n";
    return true;
}

void GoodsBatch::decreaseQuantity(int value, bool fromReserve) {
    if (value <= 0) {
        std::cout << "GoodsBatch::decreaseQuantity: количество должно быть больше нуля.\n";
        return;
    }
    value = std::min(value, quantity);
    quantity -= value;
    if (fromReserve) {
        reservedQuantity = std::max(0, reservedQuantity - value);
    }
    reservedQuantity = std::min(reservedQuantity, quantity);
    updateVolume();
    updateStatus();
    std::cout << "GoodsBatch::decreaseQuantity: количество партии #" << id
              << " уменьшено на " << value << " шт., осталось " << quantity << " шт.\n";
}

void GoodsBatch::changeQuantity(int newQuantity) {
    const int oldQuantity = quantity;
    quantity = std::max(0, newQuantity);
    reservedQuantity = std::min(reservedQuantity, quantity);
    updateVolume();
    updateStatus();
    std::cout << "GoodsBatch::changeQuantity: количество партии #" << id << " изменено с "
              << oldQuantity << " на " << quantity << " шт.\n";
}

int GoodsBatch::getAvailableQuantity() const {
    const int result = quantity - reservedQuantity;
    std::cout << "GoodsBatch::getAvailableQuantity: доступно " << result
              << " шт. партии #" << id << ".\n";
    return result;
}

std::shared_ptr<GoodsBatch> GoodsBatch::split(int newId, int partQuantity) {
    if (partQuantity <= 0 || partQuantity >= quantity - reservedQuantity) {
        std::cout << "GoodsBatch::split: партию #" << id << " нельзя разделить на " << partQuantity << " шт.\n";
        return nullptr;
    }
    quantity -= partQuantity;
    updateVolume();
    std::cout << "GoodsBatch::split: от партии #" << id << " отделено " << partQuantity
              << " шт., в ней осталось " << quantity << " шт.\n";
    return std::make_shared<GoodsBatch>(newId, unitVolume_ * partQuantity, productName, partQuantity, product);
}

double GoodsBatch::getUnitVolume() const { return unitVolume_; }

void GoodsBatch::updateStatus() {
    if (quantity <= 0) {
        status = BatchStatus::Depleted;
    } else if (reservedQuantity > 0) {
        status = BatchStatus::Reserved;
    } else if (dynamic_cast<StorageCell*>(location) != nullptr) {
        status = BatchStatus::Stored;
    } else {
        status = BatchStatus::Received;   // ещё не размещена в ячейке
    }
}

void GoodsBatch::printInfo(const std::string& indent) const {
    std::cout << indent << "GoodsBatch::printInfo: партия #" << id << " «" << productName << "» — "
              << quantity << " шт. (резерв " << reservedQuantity << "), ";
    printVolume(volume);
    std::cout << " м³, статус: " << statusText(status) << ".\n";
}

const char* GoodsBatch::statusText(BatchStatus value) {
    switch (value) {
        case BatchStatus::Received: return "принята";
        case BatchStatus::Stored: return "размещена";
        case BatchStatus::Reserved: return "зарезервирована";
        case BatchStatus::Depleted: return "израсходована";
    }
    return "неизвестно";
}

void GoodsBatch::updateVolume() {
    volume = unitVolume_ * quantity;
}

// StorageLocation

StorageLocation::StorageLocation(int id, double capacity) : id_(id), capacity_(capacity) {}

bool StorageLocation::isValidVolume(double volume) {
    return volume > 0.0;
}

bool StorageLocation::addBatch(const std::shared_ptr<GoodsBatch>& batch) {
    // Копия указателя: ссылка batch может указывать на элемент списка старого места,
    // который удалится при переносе.
    const std::shared_ptr<GoodsBatch> moved = batch;
    if (moved->location == this) {
        return false;
    }
    if (moved->location != nullptr) {
        moved->location->removeBatch(moved.get());
    }
    moved->location = this;
    batches_.push_back(moved);
    moved->updateStatus();
    return true;
}

bool StorageLocation::eraseBatch(GoodsBatch* batch) {
    if (batch == nullptr) {
        return false;
    }
    const auto it = std::find_if(
        batches_.begin(), batches_.end(),
        [batch](const std::shared_ptr<GoodsBatch>& item) { return item.get() == batch; });
    if (it == batches_.end()) {
        return false;
    }
    batch->location = nullptr;
    batches_.erase(it);
    return true;
}

int StorageLocation::getId() const { return id_; }
double StorageLocation::getCapacity() const { return capacity_; }
const std::vector<std::shared_ptr<GoodsBatch>>& StorageLocation::getBatches() const { return batches_; }

double StorageLocation::getCurrentVolume() const {
    double total = 0.0;
    for (const auto& batch : batches_) total += batch->volume;
    return total;
}

// StorageCell

StorageCell::StorageCell(int id, std::string address, double capacity)
    : StorageLocation(id, capacity), address_(std::move(address)) {
    std::cout << "StorageCell::StorageCell: ячейка " << address_ << " создана.\n";
}

double StorageCell::freeVolume() const {
    const double result = capacity_ - getCurrentVolume() - reservedVolume_;
    return result > 0.0 ? result : 0.0;
}

bool StorageCell::hasSpace(double volume) const {
    const bool result = isValidVolume(volume) && freeVolume() + kEpsilon >= volume;
    std::cout << "StorageCell::hasSpace: ячейка " << address_ << ", свободно ";
    printVolume(freeVolume());
    std::cout << " м³, нужно ";
    printVolume(volume);
    std::cout << " м³ — " << (result ? "подходит" : "не подходит") << ".\n";
    return result;
}

double StorageCell::getAvailableVolume() const {
    const double result = freeVolume();
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
    if (batch->location == this) {
        std::cout << "StorageCell::placeBatch: партия #" << batch->id << " уже в ячейке " << address_ << ".\n";
        return true;
    }
    if (freeVolume() + kEpsilon < batch->volume) {
        std::cout << "StorageCell::placeBatch: в ячейке " << address_ << " недостаточно места.\n";
        return false;
    }
    const int batchId = batch->id;
    addBatch(batch);
    std::cout << "StorageCell::placeBatch: партия #" << batchId << " размещена в ячейке " << address_ << ".\n";
    return true;
}

bool StorageCell::removeBatch(GoodsBatch* batch) {
    if (eraseBatch(batch)) {
        std::cout << "StorageCell::removeBatch: партия #" << batch->id
                  << " убрана из ячейки " << address_ << ".\n";
        return true;
    }
    std::cout << "StorageCell::removeBatch: партия не найдена в ячейке " << address_ << ".\n";
    return false;
}

void StorageCell::inventory() const {
    std::cout << "      StorageCell::inventory: ячейка " << address_ << " | занято ";
    printVolume(getCurrentVolume());
    std::cout << " из ";
    printVolume(capacity_);
    std::cout << " м³ | резерв ";
    printVolume(reservedVolume_);
    std::cout << " м³ | партий " << batches_.size() << ".\n";
    for (const auto& batch : batches_) batch->printInfo("        ");
}

std::string StorageCell::getTitle() const {
    return "ячейка " + address_;
}

bool StorageCell::reserveSpace(double volume) {
    if (!hasSpace(volume)) {
        std::cout << "StorageCell::reserveSpace: место в ячейке " << address_
                  << " зарезервировать нельзя.\n";
        return false;
    }
    reservedVolume_ += volume;
    std::cout << "StorageCell::reserveSpace: в ячейке " << address_ << " зарезервировано ";
    printVolume(volume);
    std::cout << " м³.\n";
    return true;
}

void StorageCell::releaseSpace(double volume) {
    reservedVolume_ = std::max(0.0, reservedVolume_ - volume);
    std::cout << "StorageCell::releaseSpace: резерв ячейки " << address_ << " уменьшен на ";
    printVolume(volume);
    std::cout << " м³.\n";
}

const std::string& StorageCell::getAddress() const { return address_; }
double StorageCell::getReservedVolume() const { return reservedVolume_; }

// BufferZone

BufferZone::BufferZone(int id, std::string name, double capacity)
    : StorageLocation(id, capacity), name_(std::move(name)) {
    std::cout << "BufferZone::BufferZone: зона «" << name_ << "» создана.\n";
}

bool BufferZone::hasSpace(double volume) const {
    const bool result = isValidVolume(volume) && capacity_ - getCurrentVolume() + kEpsilon >= volume;
    std::cout << "BufferZone::hasSpace: зона «" << name_ << "» — "
              << (result ? "места достаточно" : "места не хватает") << ".\n";
    return result;
}

double BufferZone::getAvailableVolume() const {
    const double freeVolume = capacity_ - getCurrentVolume();
    const double result = freeVolume > 0.0 ? freeVolume : 0.0;
    std::cout << "BufferZone::getAvailableVolume: в зоне «" << name_ << "» свободно ";
    printVolume(result);
    std::cout << " м³.\n";
    return result;
}

bool BufferZone::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch) {
        std::cout << "BufferZone::placeBatch: пустую партию разместить нельзя.\n";
        return false;
    }
    if (batch->location == this) {
        return true;
    }
    if (!hasSpace(batch->volume)) {
        std::cout << "BufferZone::placeBatch: партию разместить нельзя.\n";
        return false;
    }
    const int batchId = batch->id;
    addBatch(batch);
    std::cout << "BufferZone::placeBatch: партия #" << batchId << " размещена в зоне «" << name_ << "».\n";
    return true;
}

bool BufferZone::removeBatch(GoodsBatch* batch) {
    if (eraseBatch(batch)) {
        std::cout << "BufferZone::removeBatch: партия #" << batch->id
                  << " убрана из зоны «" << name_ << "».\n";
        return true;
    }
    std::cout << "BufferZone::removeBatch: партия не найдена в зоне «" << name_ << "».\n";
    return false;
}

void BufferZone::inventory() const {
    std::cout << "  BufferZone::inventory: зона «" << name_ << "» | занято ";
    printVolume(getCurrentVolume());
    std::cout << " из ";
    printVolume(capacity_);
    std::cout << " м³ | партий " << batches_.size() << ".\n";
    for (const auto& batch : batches_) batch->printInfo("    ");
}

std::string BufferZone::getTitle() const {
    return "зона «" + name_ + "»";
}

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
    std::cout << "Shelf::addCell: ячейка " << cell->getAddress() << " добавлена на полку " << number_ << ".\n";
    cells_.push_back(std::move(cell));
}

bool Shelf::hasSpace(double volume) const {
    const bool result = std::any_of(cells_.begin(), cells_.end(),
        [volume](const std::unique_ptr<StorageCell>& cell) { return cell->hasSpace(volume); });
    std::cout << "Shelf::hasSpace: на полке " << number_ << " место "
              << (result ? "есть" : "не найдено") << ".\n";
    return result;
}

StorageCell* Shelf::findFreeCell(double volume) const {
    for (const auto& cell : cells_) {
        if (cell->hasSpace(volume)) {
            std::cout << "Shelf::findFreeCell: на полке " << number_ << " подходит ячейка "
                      << cell->getAddress() << ".\n";
            return cell.get();
        }
    }
    std::cout << "Shelf::findFreeCell: на полке " << number_ << " места нет.\n";
    return nullptr;
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
    StorageCell* cell = findFreeCell(batch->volume);
    if (cell != nullptr && cell->placeBatch(batch)) {
        std::cout << "Shelf::placeBatch: партия размещена на полке " << number_ << ".\n";
        return true;
    }
    std::cout << "Shelf::placeBatch: на полке " << number_ << " нет места.\n";
    return false;
}

bool Shelf::removeBatch(GoodsBatch* batch) {
    if (batch == nullptr) {
        return false;
    }
    // Удаляем только из той ячейки, где партия действительно лежит.
    for (const auto& cell : cells_) {
        if (batch->location == cell.get() && cell->removeBatch(batch)) {
            std::cout << "Shelf::removeBatch: партия убрана с полки " << number_ << ".\n";
            return true;
        }
    }
    return false;
}

void Shelf::inventory() const {
    std::cout << "    Shelf::inventory: полка " << number_ << ".\n";
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
    std::cout << "StorageCabinet::addShelf: полка " << shelf->getNumber() << " добавлена в шкаф " << number_ << ".\n";
    shelves_.push_back(std::move(shelf));
}

bool StorageCabinet::hasSpace(double volume) const {
    const bool result = std::any_of(shelves_.begin(), shelves_.end(),
        [volume](const std::unique_ptr<Shelf>& shelf) { return shelf->hasSpace(volume); });
    std::cout << "StorageCabinet::hasSpace: в шкафу " << number_ << " место "
              << (result ? "есть" : "не найдено") << ".\n";
    return result;
}

StorageCell* StorageCabinet::findFreeCell(double volume) const {
    for (const auto& shelf : shelves_) {
        StorageCell* cell = shelf->findFreeCell(volume);
        if (cell != nullptr) {
            std::cout << "StorageCabinet::findFreeCell: место найдено в шкафу " << number_ << ".\n";
            return cell;
        }
    }
    std::cout << "StorageCabinet::findFreeCell: в шкафу " << number_ << " места нет.\n";
    return nullptr;
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

bool StorageCabinet::removeBatch(GoodsBatch* batch) {
    for (const auto& shelf : shelves_) {
        if (shelf->removeBatch(batch)) {
            std::cout << "StorageCabinet::removeBatch: партия убрана из шкафа " << number_ << ".\n";
            return true;
        }
    }
    return false;
}

void StorageCabinet::inventory() const {
    std::cout << "  StorageCabinet::inventory: шкаф " << number_ << ".\n";
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
    std::cout << "Warehouse::addCabinet: шкаф " << cabinet->getNumber() << " добавлен на склад «" << name_ << "».\n";
    cabinets_.push_back(std::move(cabinet));
}

void Warehouse::addBufferZone(BufferZone* bufferZone) {
    if (bufferZone == nullptr) {
        std::cout << "Warehouse::addBufferZone: пустую зону добавить нельзя.\n";
        return;
    }
    bufferZones_.push_back(bufferZone);
    std::cout << "Warehouse::addBufferZone: буферная зона «" << bufferZone->getName() << "» добавлена на склад.\n";
}

StorageCell* Warehouse::findFreeCell(double volume) const {
    for (const auto& cabinet : cabinets_) {
        StorageCell* cell = cabinet->findFreeCell(volume);
        if (cell != nullptr) {
            std::cout << "Warehouse::findFreeCell: найдена свободная ячейка " << cell->getAddress() << ".\n";
            return cell;
        }
    }
    std::cout << "Warehouse::findFreeCell: свободная ячейка не найдена.\n";
    return nullptr;
}

bool Warehouse::placeBatch(const std::shared_ptr<GoodsBatch>& batch) {
    if (!batch) {
        std::cout << "Warehouse::placeBatch: пустую партию разместить нельзя.\n";
        return false;
    }
    for (const auto& cabinet : cabinets_) {
        if (cabinet->placeBatch(batch)) {
            std::cout << "Warehouse::placeBatch: партия размещена на складе.\n";
            return true;
        }
    }
    std::cout << "Warehouse::placeBatch: на складе нет свободного места.\n";
    return false;
}

bool Warehouse::removeBatch(GoodsBatch* batch) {
    for (const auto& cabinet : cabinets_) {
        if (cabinet->removeBatch(batch)) {
            std::cout << "Warehouse::removeBatch: партия #" << batch->id << " убрана со склада.\n";
            return true;
        }
    }
    std::cout << "Warehouse::removeBatch: партия не найдена в ячейках склада.\n";
    return false;
}

std::vector<std::shared_ptr<GoodsBatch>> Warehouse::findBatches(const std::string& productName) const {
    std::vector<std::shared_ptr<GoodsBatch>> result;
    for (StorageCell* cell : getAllCells()) {
        for (const auto& batch : cell->getBatches()) {
            if (batch->productName == productName && batch->quantity > 0) {
                result.push_back(batch);
            }
        }
    }

    if (result.empty()) {
        std::cout << "Warehouse::findBatches: товар «" << productName << "» в ячейках не найден.\n";
        return result;
    }
    std::cout << "Warehouse::findBatches: товар «" << productName << "» хранится:";
    for (const auto& batch : result) {
        std::cout << " " << batch->location->getTitle() << " (партия #" << batch->id << ", "
                  << batch->quantity << " шт.)";
    }
    std::cout << ".\n";
    return result;
}

double Warehouse::getAvailableVolume() const {
    double total = 0.0;
    for (const auto& cabinet : cabinets_) total += cabinet->getAvailableVolume();
    std::cout << "Warehouse::getAvailableVolume: в ячейках склада свободно ";
    printVolume(total);
    std::cout << " м³.\n";
    return total;
}

void Warehouse::inventory() const {
    std::cout << "Warehouse::inventory: склад «" << name_ << "», адрес: " << address_ << ".\n";
    for (const auto& cabinet : cabinets_) cabinet->inventory();
    for (BufferZone* zone : bufferZones_) zone->inventory();
}

std::vector<StorageCell*> Warehouse::getAllCells() const {
    std::vector<StorageCell*> result;
    for (const auto& cabinet : cabinets_) {
        for (const auto& shelf : cabinet->getShelves()) {
            for (const auto& cell : shelf->getCells()) result.push_back(cell.get());
        }
    }
    return result;
}

int Warehouse::getId() const { return id_; }
const std::string& Warehouse::getName() const { return name_; }
const std::string& Warehouse::getAddress() const { return address_; }
const std::vector<std::unique_ptr<StorageCabinet>>& Warehouse::getCabinets() const { return cabinets_; }
const std::vector<BufferZone*>& Warehouse::getBufferZones() const { return bufferZones_; }

# Информационная система склада на C++

В этой папке реализованы классы физической структуры склада, предметные классы
информационной системы и демонстрационное консольное меню.

## Связи классов

- `Warehouse` содержит шкафы `StorageCabinet` и буферные зоны `BufferZone`;
- `StorageCabinet` содержит полки `Shelf`;
- `Shelf` содержит ячейки `StorageCell`;
- `StorageCell` и `BufferZone` наследуются от абстрактного `StorageLocation`;
- `GoodsBatch` существует отдельно и хранит указатель на текущее место хранения.

Дополнительные предметные классы:

- `Product` — товар;
- `DeliveryNote`, `DeliveryNoteItem`, `Delivery` — приемка поставки;
- `WarehouseOrder`, `OrderItem` — заказ и его позиции;
- `StocktakeSession`, `StocktakeItem` — инвентаризация;
- `Shipment`, `ShipmentItem` — отгрузка;
- `Notification` — уведомление пользователя.

`Warehouse`, `StorageCabinet` и `Shelf` не наследуются от `StorageLocation`. Они включают другие объекты через `std::unique_ptr`, что соответствует композиции в структуре склада.

## Файлы

- `storage.h` — объявления классов;
- `storage.cpp` — реализация методов;
- `warehouse_entities.h/.cpp` — предметные классы приемки, заказов, инвентаризации и отгрузки;
- `warehouse_system.h/.cpp` — меню и сценарии работы системы;
- `main.cpp` — запуск меню.

Все основные методы печатают сообщение в консоль через `std::cout`, поэтому последовательность работы можно показать при защите.

## Меню

После запуска пользователь вводит номер пункта от `1` до `8` и нажимает `Enter`.
Пункт `8` завершает работу. В начале каждого сценария выводится цепочка вызовов
классов, например:

```text
WarehouseSystem -> DeliveryNote -> DeliveryNoteItem -> Warehouse -> StorageCell -> GoodsBatch
```

Пункты меню демонстрируют структуру склада, приемку, размещение, сборку заказа,
инвентаризацию, отгрузку и просмотр уведомлений.

## Компиляция

```bash
g++ -std=c++17 -Wall -Wextra -pedantic \
    main.cpp storage.cpp warehouse_entities.cpp warehouse_system.cpp \
    -o warehouse_demo
./warehouse_demo
```

/* SPDX-License-Identifier: Apache-2.0 */
#include "inventory.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>

static constexpr auto INVENTORY_BUS_NAME =
    "xyz.openbmc_project.Inventory.Manager";

// https://github.com/openbmc/phosphor-dbus-interfaces/blob/08baf48ad5f15774d393fbbf4e9479a0ef3e82d0/yaml/xyz/openbmc_project/Inventory/Manager.interface.yaml
static constexpr auto INVENTORY_MANAGER_IFACE =
    "xyz.openbmc_project.Inventory.Manager";
static constexpr auto INVENTORY_MANAGER_OBJECT =
    "/xyz/openbmc_project/inventory";

// https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties
static constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";

using namespace inventory;

void InventoryManager::updateObject(const std::string& path,
                                    const ObjectType& updates)
{
    auto call =
        dbus.new_method_call(INVENTORY_BUS_NAME, INVENTORY_MANAGER_OBJECT,
                             INVENTORY_MANAGER_IFACE, "Notify");

    std::map<sdbusplus::message::object_path, ObjectType> inventoryUpdate = {
        {path, updates},
    };

    call.append(inventoryUpdate);
    dbus.call(call);
}

void InventoryManager::markPresent(const std::string& path)
{
    std::string absolute = std::string("/xyz/openbmc_project/inventory") + path;

    auto call = dbus.new_method_call(INVENTORY_BUS_NAME, absolute.c_str(),
                                     DBUS_PROPERTY_IFACE, "Set");

    call.append(INVENTORY_ITEM_IFACE, "Present", std::variant<bool>(true));
    dbus.call(call);
}

void InventoryManager::markAbsent(const std::string& path)
{
    std::string absolute = std::string("/xyz/openbmc_project/inventory") + path;

    auto call = dbus.new_method_call(INVENTORY_BUS_NAME, absolute.c_str(),
                                     DBUS_PROPERTY_IFACE, "Set");

    call.append(INVENTORY_ITEM_IFACE, "Present", std::variant<bool>(false));
    dbus.call(call);
}

/* inventory::accumulate */

void inventory::accumulate(std::map<std::string, ObjectType>& store,
                           const std::string& path, const ObjectType& updates)
{
    if (store.contains(path))
    {
        auto& object = store[path];

        for (const auto& [ikey, ival] : updates)
        {
            if (object.contains(ikey))
            {
                auto& interface = object[ikey];

                for (const auto& [pkey, pval] : ival)
                {
                    interface[pkey] = pval;
                }
            }
            else
            {
                object[ikey] = ival;
            }
        }
    }
    else
    {
        store[path] = updates;
    }
}

/* PublishWhenPresentInventoryDecorator */

PublishWhenPresentInventoryDecorator::PublishWhenPresentInventoryDecorator(
    Inventory* inventory) :
    inventory(inventory)
{}

void PublishWhenPresentInventoryDecorator::updateObject(
    const std::string& path, const ObjectType& updates)
{
    inventory::accumulate(objectCache, path, updates);

    if (presentCache.contains(path) && presentCache[path])
    {
        inventory->updateObject(path, objectCache[path]);
        inventory->markPresent(path);
    }
}

void PublishWhenPresentInventoryDecorator::markPresent(const std::string& path)
{
    bool alreadyPresent = presentCache.contains(path) && presentCache[path];

    presentCache.insert_or_assign(path, true);

    if (!alreadyPresent && objectCache.contains(path))
    {
        inventory->updateObject(path, objectCache[path]);
        inventory->markPresent(path);
    }
}

void PublishWhenPresentInventoryDecorator::markAbsent(const std::string& path)
{
    bool wasPresent = !presentCache.contains(path) || presentCache[path];

    presentCache.insert_or_assign(path, false);

    if (wasPresent)
    {
        inventory->markAbsent(path);
    }
}
#include "DeviceStore.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <dto/DTOs.hpp>

namespace deviceStore
{

namespace
{

// A fresh mapper per call: a cached one would keep oatpp objects alive beyond
// oatpp::base::Environment::destroy() and be reported as a leak.
std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper> mapper()
{
    return oatpp::parser::json::mapping::ObjectMapper::createShared();
}

std::string asString(const oatpp::String &value)
{
    return value ? std::string(value->c_str()) : std::string();
}

oatpp::Object<KnownDeviceDTO> toDto(const KnownDevice &device)
{
    auto dto = KnownDeviceDTO::createShared();
    dto->deviceId = device.deviceId.c_str();
    dto->friendlyName = device.friendlyName.c_str();
    dto->deviceType = device.deviceType.c_str();
    dto->locationUrl = device.locationUrl.c_str();
    dto->controlUrl = device.controlUrl.c_str();
    dto->serviceType = device.serviceType.c_str();
    dto->serviceId = device.serviceId.c_str();
    dto->manufacturer = device.manufacturer.c_str();
    dto->modelName = device.modelName.c_str();
    dto->ipAddr = device.ipAddr.c_str();
    dto->port = device.port;
    dto->lastSeen = device.lastSeen;
    return dto;
}

KnownDevice fromDto(const oatpp::Object<KnownDeviceDTO> &dto)
{
    KnownDevice device;
    if (!dto)
        return device;

    device.deviceId = asString(dto->deviceId);
    device.friendlyName = asString(dto->friendlyName);
    device.deviceType = asString(dto->deviceType);
    device.locationUrl = asString(dto->locationUrl);
    device.controlUrl = asString(dto->controlUrl);
    device.serviceType = asString(dto->serviceType);
    device.serviceId = asString(dto->serviceId);
    device.manufacturer = asString(dto->manufacturer);
    device.modelName = asString(dto->modelName);
    device.ipAddr = asString(dto->ipAddr);
    device.port = dto->port ? *dto->port : 0;
    device.lastSeen = dto->lastSeen ? *dto->lastSeen : 0;
    return device;
}

} // namespace

std::string defaultStorePath()
{
    const char *path = std::getenv("DEVICE_STORE");
    if (path != nullptr && path[0] != '\0')
        return std::string(path);
    return std::string("devices.json");
}

KnownDevice mergeDevice(const KnownDevice &known, const KnownDevice &discovered)
{
    KnownDevice merged = known;

    const auto keepLatest = [](std::string &target, const std::string &value) {
        if (!value.empty())
            target = value;
    };

    keepLatest(merged.deviceId, discovered.deviceId);
    keepLatest(merged.friendlyName, discovered.friendlyName);
    keepLatest(merged.deviceType, discovered.deviceType);
    keepLatest(merged.locationUrl, discovered.locationUrl);
    keepLatest(merged.controlUrl, discovered.controlUrl);
    keepLatest(merged.serviceType, discovered.serviceType);
    keepLatest(merged.serviceId, discovered.serviceId);
    keepLatest(merged.manufacturer, discovered.manufacturer);
    keepLatest(merged.modelName, discovered.modelName);
    keepLatest(merged.ipAddr, discovered.ipAddr);
    if (discovered.port != 0)
        merged.port = discovered.port;
    if (discovered.lastSeen != 0)
        merged.lastSeen = discovered.lastSeen;

    return merged;
}

std::string serializeDevices(const std::vector<KnownDevice> &devices)
{
    auto dto = StoredDevicesDTO::createShared();
    for (const auto &device : devices)
        dto->devices->push_back(toDto(device));
    return std::string(mapper()->writeToString(dto)->c_str());
}

std::vector<KnownDevice> deserializeDevices(const std::string &json)
{
    std::vector<KnownDevice> devices;
    if (json.empty())
        return devices;

    try
    {
        auto dto = mapper()->readFromString<oatpp::Object<StoredDevicesDTO>>(json.c_str());
        if (!dto || !dto->devices)
            return devices;
        for (const auto &deviceDto : *dto->devices)
        {
            auto device = fromDto(deviceDto);
            if (!device.deviceId.empty())
                devices.push_back(std::move(device));
        }
    }
    catch (const std::exception &)
    {
        // A corrupted store must not take the server down; start from scratch.
        devices.clear();
    }

    return devices;
}

DeviceStore::DeviceStore(std::string path) : m_path(std::move(path))
{
    load();
}

bool DeviceStore::load()
{
    std::ifstream file(m_path);
    if (!file.is_open())
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices = deserializeDevices(buffer.str());
    return true;
}

bool DeviceStore::save() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return saveLocked();
}

bool DeviceStore::saveLocked() const
{
    std::ofstream file(m_path, std::ios::trunc);
    if (!file.is_open())
        return false;
    file << serializeDevices(m_devices);
    return file.good();
}

std::vector<KnownDevice> DeviceStore::list() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

bool DeviceStore::upsert(const KnownDevice &device)
{
    if (device.deviceId.empty())
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const KnownDevice &known) {
        return known.deviceId == device.deviceId;
    });

    if (it == m_devices.end())
        m_devices.push_back(device);
    else
        *it = mergeDevice(*it, device);

    saveLocked();
    return true;
}

bool DeviceStore::remove(const std::string &deviceId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = std::remove_if(m_devices.begin(), m_devices.end(), [&](const KnownDevice &known) {
        return known.deviceId == deviceId;
    });
    if (it == m_devices.end())
        return false;

    m_devices.erase(it, m_devices.end());
    saveLocked();
    return true;
}

} // namespace deviceStore

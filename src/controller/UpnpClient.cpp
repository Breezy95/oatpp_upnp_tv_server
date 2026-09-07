#include "UpnpClient.hpp"

#include <algorithm>
#include <regex>
#include <stdexcept>

namespace {

std::pair<std::string, uint16_t> parseLocationHostAndPort(const std::string &location)
{
    std::smatch match;
    const std::regex locationRe(R"(https?://([^/:]+)(?::([0-9]+))?)");
    if (!std::regex_search(location, match, locationRe))
        return {"", 0};

    uint16_t port = 0;
    if (match[2].matched)
        port = static_cast<uint16_t>(std::stoul(match[2].str()));

    return {match[1].str(), port};
}

} // namespace

UpnpClient::UpnpClient(uint16_t port, bool initialize)
{
    if (!initialize)
        return;

    const int initResult = UpnpInit2(nullptr, port);
    if (initResult != UPNP_E_SUCCESS)
        throw std::runtime_error("Failed to initialize UPnP");

    const int registerResult = UpnpRegisterClient(callback, this, &m_handle);
    if (registerResult != UPNP_E_SUCCESS)
    {
        UpnpFinish();
        throw std::runtime_error("Failed to register UPnP client");
    }
}

UpnpClient::~UpnpClient()
{
    if (m_handle != -1)
        UpnpUnRegisterClient(m_handle);
    UpnpFinish();
}

std::unordered_map<std::string, deviceInfo> UpnpClient::devices() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

void UpnpClient::updateXml(const std::string &location, const std::string &xml)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &entry : m_devices)
    {
        if (entry.second.locationUrl == location)
        {
            entry.second.xmlString = xml;
            return;
        }
    }
}

int UpnpClient::search(int maximumWait, const std::string &searchTarget)
{
    if (m_handle == -1)
        return UPNP_E_INIT;
    return UpnpSearchAsync(m_handle, maximumWait, searchTarget.c_str(), this);
}

deviceInfo UpnpClient::parseDiscoveredDevice(const std::string &deviceId,
                                            const std::string &location,
                                            const char *deviceType,
                                            const char *serviceType)
{
    deviceInfo info;
    if (deviceId.empty() || location.empty())
        return info;

    info.deviceId = deviceId;
    info.locationUrl = location;
    if (deviceType)
        info.deviceType = deviceType;
    if (serviceType)
    {
        info.serviceType = serviceType;
        info.serviceTypes.push_back(serviceType);
    }

    const auto [host, port] = parseLocationHostAndPort(location);
    if (!host.empty())
    {
        info.ipAddr = host;
        info.port = port;
    }

    return info;
}

int UpnpClient::callback(Upnp_EventType eventType, const void *event, void *cookie)
{
    if (!cookie || !event)
        return 0;

    static_cast<UpnpClient *>(cookie)->handleDiscoveryEvent(
        eventType, static_cast<const UpnpDiscovery *>(event));
    return 0;
}

void UpnpClient::handleDiscoveryEvent(Upnp_EventType eventType, const UpnpDiscovery *event)
{
    if (eventType == UPNP_DISCOVERY_SEARCH_TIMEOUT || event == nullptr)
        return;

    const char *id = UpnpDiscovery_get_DeviceID_cstr(event);
    const char *location = UpnpDiscovery_get_Location_cstr(event);
    if (!id || !location)
        return;

    const std::string key(id);
    if (eventType == UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.erase(key);
        return;
    }

    if (eventType != UPNP_DISCOVERY_SEARCH_RESULT &&
        eventType != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE)
        return;

    if (UpnpDiscovery_get_ErrCode(event) != UPNP_E_SUCCESS)
        return;

    auto info = parseDiscoveredDevice(
        id,
        location,
        UpnpDiscovery_get_DeviceType_cstr(event),
        UpnpDiscovery_get_ServiceType_cstr(event));

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(key);
    if (it != m_devices.end())
    {
        if (!info.serviceType.empty())
        {
            auto &serviceTypes = it->second.serviceTypes;
            if (std::find(serviceTypes.begin(), serviceTypes.end(), info.serviceType) == serviceTypes.end())
                serviceTypes.push_back(info.serviceType);
            it->second.serviceType = info.serviceType;
        }
        if (!info.locationUrl.empty())
            it->second.locationUrl = info.locationUrl;
        if (!info.deviceType.empty())
            it->second.deviceType = info.deviceType;
        if (!info.ipAddr.empty())
            it->second.ipAddr = info.ipAddr;
        if (info.port != 0)
            it->second.port = info.port;
        return;
    }

    m_devices[key] = std::move(info);
}

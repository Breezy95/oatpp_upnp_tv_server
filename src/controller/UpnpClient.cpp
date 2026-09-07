#include "UpnpClient.hpp"

#include <regex>
#include <stdexcept>

UpnpClient::UpnpClient(uint16_t port)
{
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
    return UpnpSearchAsync(m_handle, maximumWait, searchTarget.c_str(), this);
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
    if (eventType == UPNP_DISCOVERY_SEARCH_TIMEOUT)
    {
        return;
    }

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

    deviceInfo info;
    info.deviceId = id;
    info.locationUrl = location;
    if (const char *value = UpnpDiscovery_get_DeviceType_cstr(event))
        info.deviceType = value;
    if (const char *value = UpnpDiscovery_get_ServiceType_cstr(event))
        info.serviceType = value;

    std::smatch match;
    if (std::regex_search(info.locationUrl, match,
                          std::regex(R"(https?://([^/:]+)(?::([0-9]+))?)")))
    {
        info.ipAddr = match[1].str();
        if (match[2].matched)
            info.port = static_cast<uint16_t>(std::stoul(match[2].str()));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices[key] = std::move(info);
}

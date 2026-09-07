#ifndef UPnPClient_hpp
#define UPnPClient_hpp

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <upnp/upnp.h>
#include <upnp/UpnpDiscovery.h>
#include <vector>

struct deviceInfo
{
    std::string deviceId;
    std::string deviceType;
    std::string friendlyName;
    std::string manufacturer;
    std::string modelName;
    std::string serviceType;
    std::string serviceId;
    std::string ipAddr;
    uint16_t port = 0;
    std::string locationUrl;
    std::vector<std::string> scpd_urls;
    std::string xmlString;
};

class UpnpClient
{
public:
    explicit UpnpClient(uint16_t port = 0, bool initialize = true);
    ~UpnpClient();

    UpnpClient(const UpnpClient &) = delete;
    UpnpClient &operator=(const UpnpClient &) = delete;

    UpnpClient_Handle handle() const noexcept { return m_handle; }
    std::unordered_map<std::string, deviceInfo> devices() const;
    void updateXml(const std::string &location, const std::string &xml);
    int search(int maximumWait, const std::string &searchTarget);
    static int callback(Upnp_EventType eventType, const void *event, void *cookie);

    static deviceInfo parseDiscoveredDevice(const std::string &deviceId,
                                           const std::string &location,
                                           const char *deviceType,
                                           const char *serviceType);

private:
    void handleDiscoveryEvent(Upnp_EventType eventType, const UpnpDiscovery *event);

    UpnpClient_Handle m_handle{-1};
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, deviceInfo> m_devices;
};

#endif

#include "upnp.h"
#include "UpnpDiscovery.h"
#include "upnpDevice.hpp"
#include <format>
#include <iostream>
#include <regex>
int upnpController::upnpCallback(Upnp_EventType EventType, const void *Event, void *Cookie)
{
    auto *controller = static_cast<upnpController *>(Cookie);
    OATPP_LOGW("UPNP CALLBACK", "CALLBACK TRIGGERED\t EVENT TYPE:%d", EventType);
    switch (EventType)
    {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery *d_event = static_cast<const UpnpDiscovery *>(Event);

        if (UpnpDiscovery_get_ErrCode(d_event) != UPNP_E_SUCCESS)
        {
            OATPP_LOGW("UPNP CALLBACK", "Discovery error: %d", UpnpDiscovery_get_ErrCode(d_event));
            break;
        }

        const char *id = UpnpDiscovery_get_DeviceID_cstr(d_event);
        const char *st = UpnpDiscovery_get_ServiceType_cstr(d_event);
        const char *location = UpnpDiscovery_get_Location_cstr(d_event);
        const char *sv = UpnpDiscovery_get_ServiceVer_cstr(d_event);
        const char *dt = UpnpDiscovery_get_DeviceType_cstr(d_event);


        if (!id || !location || !st)
        {
            OATPP_LOGW("UPNP CALLBACK", "Incomplete discovery data");
            break;
        }

        std::string locationStr(location);
        std::string ipAddr;
        uint16_t port = 0;

        // Extract IP and port from LOCATION URL using regex
        std::regex locationRegex(R"(http:\/\/([0-9.]+):([0-9]+))");
        std::smatch match;
        if (std::regex_search(locationStr, match, locationRegex))
        {
            ipAddr = match[1].str();
            port = static_cast<uint16_t>(std::stoi(match[2].str()));
        }
        else
        {
            OATPP_LOGW("UPNP CALLBACK", "Failed to parse IP/port from LOCATION: %s", location);
        }

        deviceInfo info;
        info.deviceId = id;
        info.deviceType = dt;
        info.locationUrl = locationStr;
        info.ipAddr = ipAddr;
        info.port = port;
        info.serviceType = st;
        
        //parse xml
        info.friendlyName = ""; // optional, can be populated from device XML
        info.manufacturer = "";
        info.modelName = "";
        
        info.serviceId = "";

        controller->deviceList[info.deviceType] = info;

        OATPP_LOGI("UPNP CALLBACK", "Device found: USN=%s, ST=%s, LOCATION=%s, IP=%s:%d",
                   id, st, location, ipAddr.c_str(), port);

        break;
    }

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        OATPP_LOGI("UPNP CALLBACK", "Discovery search timeout");
        break;

    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        OATPP_LOGI("UPNP CALLBACK", "Advertisement alive received");
        break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        OATPP_LOGI("UPNP CALLBACK", "Advertisement byebye received");
        break;

    default:
        OATPP_LOGD("UPNP CALLBACK", "Unhandled event type: %d", EventType);
        break;
    }

    return 0;
}
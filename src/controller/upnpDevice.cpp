#include "upnp.h"
#include "UpnpDiscovery.h"
#include "upnpDevice.hpp"
#include <format>
#include <iostream>
#include <regex>

int upnpController::upnpCallback(Upnp_EventType EventType, const void *Event, void *Cookie)
{
    auto *controller = static_cast<upnpController *>(Cookie);
    const UpnpDiscovery *d_event = static_cast<const UpnpDiscovery *>(Event);
    switch (EventType)
    {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {

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

        OATPP_LOGI("UPNP CALLBACK", "Discovery fields: DeviceID=%s, ServiceType=%s, LOCATION=%s, ServiceVer=%s, DeviceType=%s",
                   id ? id : "(null)", st ? st : "(null)", location ? location : "(null)", sv ? sv : "(null)", dt ? dt : "(null)");
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

        // parse xml
        info.friendlyName = ""; // optional, can be populated from device XML
        info.manufacturer = "";
        info.modelName = "";
        info.serviceId = "";

        OATPP_LOGI("UPNP CALLBACK", "Device found: USN=%s, ST=%s, LOCATION=%s, IP=%s:%d", id, st, location, ipAddr.c_str(), port);

        break;
    }

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        OATPP_LOGI("UPNP CALLBACK", "Discovery search timeout");
        break;
    }

    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        const char *id = UpnpDiscovery_get_DeviceID_cstr(d_event);
        const char *st = UpnpDiscovery_get_ServiceType_cstr(d_event);
        const char *location = UpnpDiscovery_get_Location_cstr(d_event);
        const char *sv = UpnpDiscovery_get_ServiceVer_cstr(d_event);
        const char *dt = UpnpDiscovery_get_DeviceType_cstr(d_event);
        OATPP_LOGI("UPNP CALLBACK", "Advertisement Alive fields: DeviceID=%s, ServiceType=%s, LOCATION=%s, ServiceVer=%s, DeviceType=%s",
                   id ? id : "(null)", st ? st : "(null)", location ? location : "(null)", sv ? sv : "(null)", dt ? dt : "(null)");

        break;
    }

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        OATPP_LOGI("UPNP CALLBACK", "Advertisement byebye received");
        const char *location = UpnpDiscovery_get_Location_cstr(d_event);
        controller->deviceList.erase(std::string(location));
        break;
    }

    default:
    {
        OATPP_LOGD("UPNP CALLBACK", "Unhandled event type: %d", EventType);
        break;
    }

        return 0;
    }
}

// GPTS TAKE
// Recursively parses device/serviceList for SCPD URLs and adds them to the vector.
// If a service points to another SCPD (via SCPDURL), downloads and parses that as well.
void parseDeviceForSCPDs(IXML_Document *deviceMainXML, std::vector<std::string> &scpd_urls, const std::string &baseUrl = "")
{
    if (!deviceMainXML)
        return;

    // Find all <service> elements in the document
    IXML_NodeList *serviceList = ixmlDocument_getElementsByTagName(deviceMainXML, "service");
    if (!serviceList)
        return;

    for (unsigned long i = 0; i < ixmlNodeList_length(serviceList); ++i)
    {
        IXML_Node *serviceNode = ixmlNodeList_item(serviceList, i);
        if (!serviceNode)
            continue;

        // Find <SCPDURL> child
        IXML_NodeList *scpdUrlList = ixmlElement_getElementsByTagName((IXML_Element *)serviceNode, "SCPDURL");
        if (scpdUrlList && ixmlNodeList_length(scpdUrlList) > 0)
        {
            IXML_Node *scpdUrlNode = ixmlNodeList_item(scpdUrlList, 0);
            if (scpdUrlNode)
            {
                const char *urlText = ixmlNode_getNodeValue(ixmlNode_getFirstChild(scpdUrlNode));
                if (urlText)
                {
                    std::string url(urlText);
                    // If baseUrl is provided and url is relative, prepend baseUrl
                    if (!baseUrl.empty() && url.find("http://") != 0 && url.find("https://") != 0)
                    {
                        std::string fullUrl = baseUrl;
                        if (fullUrl.back() != '/' && url.front() != '/')
                            fullUrl += '/';
                        fullUrl += url;
                        url = fullUrl;
                    }
                    scpd_urls.push_back(url);

                    // Download and parse the SCPD if it's another XML
                    IXML_Document *scpdDoc = nullptr;
                    int ret = UpnpDownloadXmlDoc(url.c_str(), &scpdDoc);
                    if (ret == UPNP_E_SUCCESS && scpdDoc)
                    {
                        // Convert SCPD document to string
                        // Recursively parse for nested SCPDs, seen in the devicemainxml
                        std::string scpdXml = ixmlDocumenttoString(scpdDoc);

                        IXML_Element *scpdElem = ixmlDocument_createElement(deviceMainXML, "SCPDDocument");
                        if (scpdElem)
                        {

                            IXML_Node *textNode = ixmlDocument_createTextNode(deviceMainXML, scpdXml.c_str());
                            if (textNode)
                            {
                                ixmlNode_appendChild((IXML_Node *)scpdElem, textNode);
                            }

                            ixmlNode_appendChild(serviceNode, (IXML_Node *)scpdElem);
                        }

                        // Recursively parse the downloaded SCPD for further SCPD references
                        parseDeviceForSCPDs(scpdDoc, scpd_urls, baseUrl);
                        ixmlDocument_free(scpdDoc);
                    }
                }
            }
            ixmlNodeList_free(scpdUrlList);
        }
    }
    ixmlNodeList_free(serviceList);
}

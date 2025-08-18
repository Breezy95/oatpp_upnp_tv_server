#ifndef upnpDevice
#include <dto/DTOs.hpp>
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include <string>
#include <upnp/upnp.h>
#include <iostream>
#include "vector"
#include <DeviceDescriptorComponent.hpp>
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/incoming/Request.hpp"
#include "ixmlfuncs.hpp"

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
    uint16_t port;
    std::string locationUrl;
};

class upnpController : public oatpp::web::server::api::ApiController
{
private:
    OATPP_COMPONENT(std::shared_ptr<DeviceDescriptorComponent::DeviceDescriptor>, m_desc);

public:
    static std::shared_ptr<upnpController> createShared(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
    {
        return std::shared_ptr<upnpController>(std::make_shared<upnpController>(objectMapper));
    }

    upnpController(const std::shared_ptr<ObjectMapper> &objectMapper) : oatpp::web::server::api::ApiController(objectMapper)
    {
        UpnpInit2(nullptr, 0);
        // this->Hnd =
        int ret = UpnpRegisterClient(upnpController::upnpCallback, this, &this->Hnd);
        if (ret != UPNP_E_SUCCESS)
        {
            OATPP_LOGI("upnpdevice", " client register failure, err code:  %d", ret)
        }
        OATPP_LOGI("upnpdevice", " client register success, err code:  %d", ret)
    }

    ~upnpController()
    {
        UpnpFinish();
    };
    static int upnpCallback(Upnp_EventType EventType, const void *Event, void *Cookie);
    UpnpClient_Handle Hnd;
    std::unordered_map<std::string, deviceInfo> deviceList;
    // int setTvControlUrl(std::string url);
public:
#include OATPP_CODEGEN_BEGIN(ApiController)
    ENDPOINT("GET", "/upnp/hello", upnp_hello_world)
    {
        auto dto = MessageDto::createShared();
        dto->statusCode = 200;
        dto->message = "Hello World";
        return createDtoResponse(Status::CODE_200, dto);
    }

    ENDPOINT("GET", "/upnp/devices", getDeviceList)
    {
        auto dto = UpnpDeviceListResponse::createShared();
        for (auto &dev : deviceList)
        {
            auto deviceDTO = DeviceInfoDTO::createShared();
            deviceDTO->modelName = dev.second.modelName;
            deviceDTO->locationUrl = dev.second.locationUrl;
            deviceDTO->deviceId = dev.second.deviceId;
            deviceDTO->deviceType = dev.second.deviceType;
            deviceDTO->friendlyName = dev.second.friendlyName;
            deviceDTO->manufacturer = dev.second.manufacturer;
            deviceDTO->serviceType = dev.second.serviceType;
            deviceDTO->serviceId = dev.second.serviceId;
            deviceDTO->ipAddr = dev.second.ipAddr;
            deviceDTO->port = dev.second.port;
            dto->devices->push_back(deviceDTO);
        }

        return createDtoResponse(Status::CODE_200, dto);
    }

    ENDPOINT("POST", "/upnp/getProtocolInfo", getProtocolInfo)
    {
        IXML_Document *body = createGetProtocolInfoDocument();
        return createDtoResponse(Status::CODE_200, String("Success"));
    }
    /*
        ENDPOINT("HEAD", "/upnp/getResource/{resourcePath}"){

        }

        // TV will retrieve
        ENDPOINT("GET", "/upnp/getResource/{resourcePath}", getResourceURI){

        }
    
    // Sends command to tv to set resource link
    ENDPOINT("POST", "/upnp/setResource/{resourceUri}", setResourceURI)
    {
        // ixmlDocument_createDocumentEx
    }

    ENDPOINT("POST", "/upnp/loadVideo", playVideo, BODY_DTO(Object<EnqueueVidRequestDTO>, req))
    {
        auto resource = req->mediaResourceUrl;
        auto deviceAddress = req->deviceAddress;
        // int ret =UpnpSendActionAsync(Hnd, req->deviceAddress->c_str(),);
    }
*/
    ENDPOINT("POST", "/upnp/search", upnpSearch, BODY_DTO(Object<UpnpSearchRequest>, req))
    {
        auto retDTO = MessageDto::createShared();
        String st = req->searchType;
        int timeout = (int)req->mx;
        OATPP_LOGI("upnpdevice", " starting search\tST: %s, timeout: %d ", st->c_str(), timeout);

        int ret = UpnpSearchAsync(Hnd, timeout, st->c_str(), this);
        if (ret != UPNP_E_SUCCESS)
        {
            auto err = UpnpErrorDTO::createShared();
            err->message = "Error in upnp search async";
            OATPP_LOGI("upnpdevice", " search failure, err code:  %d", ret)
            err->upnpErrorCode = ret;
            return createDtoResponse(Status::CODE_422, err);
        }

        return createDtoResponse(Status::CODE_200, retDTO);
    }

    ENDPOINT("M-SEARCH", "*", star)
    {
        OATPP_LOGD("SsdpController", "'M-SEARCH *' Received");
        auto rsp = createResponse(Status::CODE_200, oatpp::String(""));
        rsp->putHeader("CACHE-CONTROL", "max-age=100");
        rsp->putHeader("EXT", "");
        rsp->putHeader("LOCATION", "http://" + m_desc->ipPort + "/description.xml");
        rsp->putHeader("SERVER", "Ubuntu WSL, UPnP/1.0, IpBridge/1.17.0");
        rsp->putHeader("ST", "urn:schemas-upnp-org:device:basic:1");
        rsp->putHeader("USN", "uuid:" + m_desc->uuid + "::upnp:rootdevice");
        return rsp;
    }
};

#endif
#ifndef upnpDevice
#include <dto/DTOs.hpp>
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include <string>
#include <upnp/upnp.h>
#include "upnptools.h"
#include <iostream>
#include "vector"
#include <DeviceDescriptorComponent.hpp>
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/protocol/http/incoming/Request.hpp"
#include "ixmlfuncs.hpp"
#include "UpnpClient.hpp"
#include <regex>

#include <iostream>
#include <fstream>

extern "C" {
    #include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include <libavutil/dict.h>
#include "gupnp-av-1.0/libgupnp-av/gupnp-av.h"
#include "gupnp-dlna-2.0/libgupnp-dlna/gupnp-dlna.h"
}

enum class AudioKind
{
    LPCM,
    MP3,
    UNKNOWN
};
struct DlnaHeaders
{
    std::string contentType;
    std::string contentFeatures;
};
//In the future create a template
DlnaHeaders pickAudioDlnaHeaders(AVCodecID codec,
                                 int sampleRate,
                                 int channels)
{
    DlnaHeaders h;

    if (codec == AV_CODEC_ID_PCM_S16LE)
    {
        // WAV / LPCM
        h.contentType =
            "audio/L16;rate=" + std::to_string(sampleRate) +
            ";channels=" + std::to_string(channels);

        h.contentFeatures =
            "DLNA.ORG_PN=LPCM;"
            "DLNA.ORG_OP=01;"
            "DLNA.ORG_FLAGS=01700000000000000000000000000000";
    }
    else if (codec == AV_CODEC_ID_MP3)
    {
        h.contentType = "audio/mpeg";

        h.contentFeatures =
            "DLNA.ORG_PN=MP3;"
            "DLNA.ORG_OP=01;"
            "DLNA.ORG_FLAGS=01700000000000000000000000000000";
    }
    else
    {
        // Fallback (works on Samsung)
        h.contentType = "audio/mpeg";
        h.contentFeatures =
            "DLNA.ORG_PN=MP3;"
            "DLNA.ORG_OP=01;"
            "DLNA.ORG_FLAGS=01700000000000000000000000000000";
    }

    return h;
}

void parseDeviceForSCPDs(IXML_Document *deviceMainXML,
                         std::vector<std::string> &scpd_urls,
                         const std::string &baseUrl);

class upnpController : public oatpp::web::server::api::ApiController
{
private:
    OATPP_COMPONENT(std::shared_ptr<DeviceDescriptorComponent::DeviceDescriptor>, m_desc);
    std::shared_ptr<UpnpClient> m_upnp;

public:
    static std::shared_ptr<upnpController> createShared(
        const std::shared_ptr<ObjectMapper> &objectMapper,
        const std::shared_ptr<UpnpClient> &upnp)
    {
        return std::make_shared<upnpController>(objectMapper, upnp);
    }

    upnpController(const std::shared_ptr<ObjectMapper> &objectMapper,
                   const std::shared_ptr<UpnpClient> &upnp)
        : oatpp::web::server::api::ApiController(objectMapper), m_upnp(upnp) {}

public:
#include OATPP_CODEGEN_BEGIN(ApiController)
    ENDPOINT("GET", "/upnp/hello", upnp_hello_world)
    {
        auto dto = MessageDto::createShared();
        dto->statusCode = 200;
        dto->message = "Hello World";
        return createDtoResponse(Status::CODE_200, dto);
    }

    ENDPOINT("GET", "/upnp/getVolume", getTVVolume, BODY_DTO(Object<ActionDTO>, actionDTO))
    {
        auto actionDoc = createGetVolumeDocument();

        IXML_Document *resp;
        OATPP_LOGI("XML", "action doc \n %s\n", ixmlDocumenttoString(actionDoc))
        OATPP_LOGI("UPNP", "controlURL=%s, serviceType=%s, serviceId=%s",
                   actionDTO->actionUrl ? actionDTO->actionUrl->c_str() : "(null)",
                   actionDTO->serviceType ? actionDTO->serviceType->c_str() : "(null)",
                   actionDTO->serviceId ? actionDTO->serviceId->c_str() : "(null)");

        int ret = UpnpSendAction(m_upnp->handle(), actionDTO->actionUrl->c_str(), actionDTO->serviceType->c_str(), nullptr, actionDoc, &resp);
        if (ret != 0)
        {
            auto error = UpnpErrorDTO::createShared();
            error->statusCode = 422;
            error->upnpErrorCode = ret;
            if (ret > 0)
                error->message = ixmlDocumenttoString(resp);
            return createDtoResponse(Status::CODE_422, error);
        }
        return createResponse(Status::CODE_200, ixmlDocumenttoString(resp));
    }

    ENDPOINT("GET", "/upnp/getMainXML", getDeviceMainXML, BODY_DTO(Object<MessageDto>, message))
    {
        String msg = message->message;
        IXML_Document *doc = nullptr;
        int ret = UpnpDownloadXmlDoc(msg->c_str(), &doc);
        if (ret != UPNP_E_SUCCESS || doc == nullptr)
        {
            auto err = MessageDto::createShared();
            err->statusCode = ret;
            err->message = "Failed to download XML document";
            return createDtoResponse(Status::CODE_422, err);
        }

        std::string baseUrl = std::string(msg->c_str());
        auto pos = baseUrl.rfind('/');
        if (pos != std::string::npos)
            baseUrl = baseUrl.substr(0, pos + 1);

        std::vector<std::string> scpd_urls;
        parseDeviceForSCPDs(doc, scpd_urls, baseUrl);
        std::string xmlStr = ixmlDocumenttoString(doc);
        ixmlDocument_free(doc);

        std::string s = msg->c_str();
        std::regex re(R"(https?://([^/:]+)(?::([0-9]+))?)");
        std::smatch m;
        std::string host, port;
        if (std::regex_search(s, m, re))
        {
            host = m[1];                           // "192.168.0.100"
            port = m[2].matched ? m[2].str() : ""; // "52235" or empty
        }

        m_upnp->updateXml(msg->c_str(), xmlStr);
        return createResponse(Status::CODE_200, xmlStr);
    }

    // try with a simple volume change
    ENDPOINT("POST", "/upnp/sendAction", sendAction, BODY_DTO(Object<ActionDTO>, actionDTO))
    {
        int arglen = actionDTO->actionArgsList->size();
        std::vector<actionArg> args;
        for (int i = 0; i < arglen; i++)
        {
            args.push_back(actionArg{
                actionDTO->actionArgsList[i].get()->name,
                actionDTO->actionArgsList[i].get()->direction,
                actionDTO->actionArgsList[i].get()->relatedStateVariable,
                actionDTO->actionArgsList[i].get()->value,
            });
        }

        auto actionDoc = createActionDocument(actionDTO->actionName, actionDTO->serviceId, args);
        IXML_Document *resp;
        OATPP_LOGI("createAction", "action document:\n %s\n", ixmlDocumenttoString(actionDoc));
        int ret = UpnpSendAction(m_upnp->handle(), actionDTO->actionUrl->c_str(), actionDTO->serviceType->c_str(), nullptr, actionDoc, &resp);
        if (ret != 0)
        {
            auto error = UpnpErrorDTO::createShared();
            error->upnpErrorCode = ret;
            OATPP_LOGW("send action doc", "RECEIVED: \n%s", ixmlDocumenttoString(resp))
            error->message = +UpnpGetErrorMessage(ret);
            return createDtoResponse(Status::CODE_422, error);
        }
        return createResponse(Status::CODE_200, ixmlDocumenttoString(resp));
    }

    ENDPOINT("GET", "/upnp/devices", getDeviceList)
    {
        auto deviceList = m_upnp->devices();
        if (deviceList.empty())
            return createResponse(Status::CODE_200, "Empty device list");
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

    ENDPOINT("POST", "/upnp/getProtocolInfo", getProtocolInfo, BODY_DTO(Object<ActionDTO>, req))
    {
        String devicePath = req->actionUrl;
        IXML_Document *action = createGetProtocolInfoDocument();
        IXML_Document *resp = nullptr;
        int ret = UpnpSendAction(m_upnp->handle(), devicePath->c_str(), req->serviceType->c_str(), nullptr, action, &resp);

        if (ret != UPNP_E_SUCCESS)
        {
            auto upnpError = UpnpErrorDTO::createShared();
            upnpError->upnpErrorCode = ret;
            upnpError->message = ixmlDocumenttoString(resp);
            return createDtoResponse(Status::CODE_401, upnpError);
        }
        return createResponse(Status::CODE_200, ixmlDocumenttoString(resp));
    }

    ENDPOINT("HEAD", "/media/video/{resourcePath}", resourcePathVideoMetadata, PATH(String, resourcePath))
    {

        auto outResp = serveVideoResourceURI(resourcePath);
        auto headers = outResp->getHeaders();
        for (auto &pair : headers.getAll())
        {
            OATPP_LOGI("REQUEST_HEADERS", "%s: %s", pair.first.toString(), pair.second.toString());
        }
        return outResp;
    }

    ENDPOINT("GET", "/media/video/{resourcePath}", serveVideoResourceURI, PATH(String, resourcePath))
    {

        std::string filePath = std::string("media/video/") + resourcePath;

        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return createResponse(Status::CODE_404, "File not found");
        }

        file.seekg(0, std::ios::beg);
        std::string fileContent((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        file.close();

        auto response = ResponseFactory::createResponse(Status::CODE_200, fileContent);
        response->putHeader("Content-Type", "audio/wav");
        response->putHeader("ContentFeatures.DLNA.ORG", "DLNA.ORG_PN=LPCM;DLNA.ORG_OP=01;DLNA.ORG_FLAGS=01700000000000000000000000000000");
        response->putHeader("Scid.DLNA.ORG", "839080694");
        response->putHeader("TransferMode.DLNA.ORG", "Streaming");
        // response->putHeader("Content-Length", "463500");
        response->putHeader("Connection", "Keep-Alive");
        return response;
    }

    ENDPOINT("HEAD", "/media/audio/{resourcePath}", resourcePathAudioMetadata, PATH(String, resourcePath))
    {

        auto outResp = serveResourceURI(resourcePath);
        auto headers = outResp->getHeaders();
        for (auto &pair : headers.getAll())
        {
            OATPP_LOGI("REQUEST_HEADERS", "%s: %s", pair.first.toString(), pair.second.toString());
        }
        return outResp;
    }

    // TV will retrieve
    ENDPOINT("GET", "/media/audio/{resourcePath}", serveResourceURI, PATH(String, resourcePath))
    {
        std::string filePath = std::string("media/audio/") + resourcePath;
        AVFormatContext *fmt = nullptr;
        avformat_open_input(&fmt, filePath.c_str(), nullptr, nullptr);
        avformat_find_stream_info(fmt, nullptr);
        // std::cout <<
        AVStream *audio = nullptr;
        for (unsigned i = 0; i < fmt->nb_streams; i++)
        {
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audio = fmt->streams[i];
                break;
            }
        }
        if (!audio)
        {
            avformat_close_input(&fmt);
            throw std::runtime_error("No audio stream");
        }

        AVCodecParameters *cp = audio->codecpar;

        int sampleRate = cp->sample_rate;
        int channels = cp->ch_layout.nb_channels;
        avformat_close_input(&fmt);

        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return createResponse(Status::CODE_404, "File not found");
        }

        file.seekg(0, std::ios::beg);
        std::string fileContent((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

        file.close();

        auto response = ResponseFactory::createResponse(Status::CODE_200, fileContent);
        auto headers = pickAudioDlnaHeaders(codecId, sampleRate, channels);
        response->putHeader("Content-Type", h.contentType.c_str());
        response->putHeader("ContentFeatures.DLNA.ORG", h.contentFeatures.c_str());
        response->putHeader("Scid.DLNA.ORG", "839080694");
        response->putHeader("TransferMode.DLNA.ORG", "Streaming");
        // response->putHeader("Content-Length", "463500");
        response->putHeader("Connection", "Keep-Alive");
        return response;
    }

    // Sends command to tv to set resource link
    ENDPOINT("POST", "/upnp/setResource", setResourceURI, BODY_DTO(Object<SetResourceUriDTO>, actionDTO))
    {

        std::map<std::string, std::string> args;
        for (int i = 0; i < actionDTO->actionArgsList->size(); i++)
        {
            auto elem = actionDTO->actionArgsList[i];
            IXML_Document *text;

            if (elem->name->compare("CurrentURIMetaData") == 0 && actionDTO->serverGen)
            {
                std::map<std::string, std::string> fileInfo;
                for (auto begin = actionDTO->fileInfo->begin(); begin != actionDTO->fileInfo->end(); begin++)
                {
                    fileInfo[begin->first] = begin->second;
                }
                if (actionDTO->isAudio)
                {
                    std::string metaString = generateDidlLite(fileInfo["filepath"], fileInfo["fileURI"]);
                    args.insert({elem->name, metaString.c_str()});
                }
                else
                {
                    text = createMetadataDocument();
                    args.insert({elem->name, ixmlDocumenttoString(text)});
                    ixmlDocument_free(text);
                }
            }

            else
                args.insert({elem->name, elem->value});
        }

        // non generic action doc
        auto actionDoc = createSetAVTransportURIDoc(args);
        // auto a = createMetadataDocument();
        // auto b = createMetadataArgs("media/audio/BisexualLight.mp3");
        // OATPP_LOGI("DIFFERENCE", "VERSION 1: %s\n\n\n VERSION 2: %s\n\n\n", ixmlDocumenttoString(a), ixmlDocumenttoString(b));
        // ixmlDocument_free(a);
        // ixmlDocument_free(b);
        IXML_Document *resp;
        OATPP_LOGI("SET RESOURCE", "SETRESOURCE DOC: \n %s", ixmlDocumenttoString(actionDoc))
        int ret = UpnpSendAction(m_upnp->handle(), actionDTO->actionUrl->c_str(), actionDTO->serviceType->c_str(), nullptr, actionDoc, &resp);
        if (ret != UPNP_E_SUCCESS)
        {
            auto upnpError = UpnpErrorDTO::createShared();
            upnpError->upnpErrorCode = ret;
            {
                std::string msg = std::string(UpnpGetErrorMessage(ret)) + (resp ? ixmlDocumenttoString(resp) : "");
                upnpError->message = msg.c_str();
            }
            ixmlDocument_free(resp);
            return createDtoResponse(Status::CODE_400, upnpError);
        }
        return createResponse(Status::CODE_200, ixmlDocumenttoString(resp));
    }

    ENDPOINT("POST", "/upnp/loadVideo", playVideo, BODY_DTO(Object<EnqueueVidRequestDTO>, req))
    {
        auto resource = req->mediaResourceUrl;
        auto deviceAddress = req->deviceAddress;
        // int ret =UpnpSendActionAsync(Hnd, req->deviceAddress->c_str(),);
        return createDtoResponse(Status::CODE_200, String("Placeholder"));
    }

    ENDPOINT("POST", "/upnp/search", upnpSearch, BODY_DTO(Object<UpnpSearchRequest>, req))
    {
        auto retDTO = MessageDto::createShared();
        String st = req->searchType;
        int timeout = (int)req->mx;
        OATPP_LOGI("upnpdevice", " starting search\tST: %s, timeout: %d ", st->c_str(), timeout);

        int ret = m_upnp->search(timeout, st->c_str());
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

    // ENDPOINT()

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

#endif upnpDevice
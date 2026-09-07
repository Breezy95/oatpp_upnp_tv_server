#ifndef UPNP_DEVICE_HPP
#define UPNP_DEVICE_HPP

#include <dto/DTOs.hpp>
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include <string>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
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

inline std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string fileStem(const std::string &filePath)
{
    const auto slashPos = filePath.find_last_of("/\\");
    std::string name = slashPos == std::string::npos ? filePath : filePath.substr(slashPos + 1);
    const auto dotPos = name.find_last_of('.');
    return dotPos == std::string::npos ? name : name.substr(0, dotPos);
}

inline AudioKind detectAudioKind(const std::string &filePath)
{
    const auto normalized = toLower(filePath);
    if (normalized.find(".mp3") != std::string::npos ||
        normalized.find(".m4a") != std::string::npos ||
        normalized.find(".aac") != std::string::npos)
        return AudioKind::MP3;
    if (normalized.find(".wav") != std::string::npos ||
        normalized.find(".pcm") != std::string::npos ||
        normalized.find(".flac") != std::string::npos)
        return AudioKind::LPCM;
    return AudioKind::UNKNOWN;
}

inline DlnaHeaders pickAudioDlnaHeaders(AudioKind codec,
                                       int sampleRate,
                                       int channels)
{
    DlnaHeaders h;

    if (codec == AudioKind::LPCM)
    {
        h.contentType = "audio/L16;rate=" + std::to_string(sampleRate) +
                        ";channels=" + std::to_string(channels);
        h.contentFeatures =
            "DLNA.ORG_PN=LPCM;"
            "DLNA.ORG_FLAGS=ED100000000000000000000000000000";
    }
    else
    {
        h.contentType = "audio/mpeg";
        h.contentFeatures =
            "DLNA.ORG_PN=MP3;"
            "DLNA.ORG_FLAGS=ED100000000000000000000000000000";
    }

    return h;
}

void parseDeviceForSCPDs(IXML_Document *deviceMainXML,
                         std::vector<std::string> &scpd_urls,
                         const std::string &baseUrl);

inline std::string xmlEscape(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (char ch : value)
    {
        switch (ch)
        {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += ch; break;
        }
    }
    return result;
}

inline std::string buildContentDirectoryDIDL(const std::string &objectId,
                                           const std::string &title,
                                           const std::string &streamUrl,
                                           const std::string &mediaType)
{
    const std::string normalizedType = toLower(mediaType);
    const bool isAudio = normalizedType == "audio" || normalizedType == "mp3" || normalizedType == "wav" || normalizedType == "pcm";
    const std::string itemClass = isAudio ? "object.item.audioItem" : "object.item.videoItem";
    const std::string protocolInfo = isAudio
        ? "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_FLAGS=ED100000000000000000000000000000"
        : "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_FLAGS=ED100000000000000000000000000000";
    const std::string resolvedObjectId = objectId.empty() ? "0" : objectId;

    std::ostringstream oss;
    oss << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
        << "<item id=\"" << xmlEscape(resolvedObjectId) << "\" parentID=\"0\" restricted=\"false\">"
        << "<dc:title>" << xmlEscape(title.empty() ? (isAudio ? "Audio stream" : "Video stream") : title) << "</dc:title>"
        << "<res protocolInfo=\"" << protocolInfo << "\" "
        << (isAudio ? "sampleFrequency=\"44100\" nrAudioChannels=\"2\" bitrate=\"320000\"" : "resolution=\"1280x720\" bitrate=\"4500000\"")
        << ">" << xmlEscape(streamUrl) << "</res>"
        << "<upnp:class>" << itemClass << "</upnp:class>"
        << "</item></DIDL-Lite>";
    return oss.str();
}

inline std::string buildContentDirectoryBrowseResponse(const std::string &objectId,
                                                    const std::string &title,
                                                    const std::string &streamUrl,
                                                    const std::string &mediaType)
{
    const std::string result = buildContentDirectoryDIDL(objectId, title, streamUrl, mediaType);
    std::ostringstream oss;
    oss << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        << "<s:Body><u:BrowseResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
        << "<Result>" << xmlEscape(result) << "</Result>"
        << "<NumberReturned>1</NumberReturned>"
        << "<TotalMatches>1</TotalMatches>"
        << "<UpdateID>0</UpdateID>"
        << "</u:BrowseResponse></s:Body></s:Envelope>";
    return oss.str();
}

inline std::string buildContentDirectoryActionResponse(const std::string &actionName,
                                                    const std::string &payload)
{
    std::ostringstream oss;
    oss << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        << "<s:Body><u:" << actionName << "Response xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
        << payload
        << "</u:" << actionName << "Response></s:Body></s:Envelope>";
    return oss.str();
}

inline std::string contentDirectoryProfileXml()
{
    return R"(<?xml version="1.0" encoding="utf-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <actionList>
    <action>
      <name>Browse</name>
      <argumentList>
        <argument><name>ObjectID</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_ObjectID</relatedStateVariable></argument>
        <argument><name>BrowseFlag</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_BrowseFlag</relatedStateVariable></argument>
        <argument><name>Filter</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Filter</relatedStateVariable></argument>
        <argument><name>StartingIndex</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Index</relatedStateVariable></argument>
        <argument><name>RequestedCount</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable></argument>
        <argument><name>SortCriteria</name><direction>in</direction><relatedStateVariable>A_ARG_TYPE_SortCriteria</relatedStateVariable></argument>
        <argument><name>Result</name><direction>out</direction><relatedStateVariable>A_ARG_TYPE_Result</relatedStateVariable></argument>
        <argument><name>NumberReturned</name><direction>out</direction><relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable></argument>
        <argument><name>TotalMatches</name><direction>out</direction><relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable></argument>
        <argument><name>UpdateID</name><direction>out</direction><relatedStateVariable>A_ARG_TYPE_UpdateID</relatedStateVariable></argument>
      </argumentList>
    </action>
    <action>
      <name>GetSystemUpdateID</name>
      <argumentList><argument><name>Id</name><direction>out</direction><relatedStateVariable>SystemUpdateID</relatedStateVariable></argument></argumentList>
    </action>
  </actionList>
  <serviceStateTable>
    <stateVariable sendEvents="yes"><name>SystemUpdateID</name><dataType>ui4</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>SearchCapabilities</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>SortCapabilities</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_ObjectID</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_BrowseFlag</name><dataType>string</dataType><allowedValueList><allowedValue>BrowseMetadata</allowedValue><allowedValue>BrowseDirectChildren</allowedValue></allowedValueList></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_Filter</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_Index</name><dataType>ui4</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_Count</name><dataType>ui4</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_SortCriteria</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_Result</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="no"><name>A_ARG_TYPE_UpdateID</name><dataType>ui4</dataType></stateVariable>
  </serviceStateTable>
</scpd>)";
}

inline std::string descriptionProfileXml()
{
    return R"(<?xml version="1.0" encoding="utf-8"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device>
    <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
    <friendlyName>Oat++ UPnP TV Server</friendlyName>
    <manufacturer>Oat++ UPnP TV Server</manufacturer>
    <manufacturerURL>https://github.com/Breezy95/oatpp_upnp_tv_server</manufacturerURL>
    <modelDescription>UPnP/DLNA media server</modelDescription>
    <modelName>OatppUpnpTvServer</modelName>
    <modelNumber>1</modelNumber>
    <serialNumber>1000000471337</serialNumber>
    <UDN>uuid:2f402f80-da50-11e1-9b23-be5a0a70cafe</UDN>
    <serviceList>
      <service>
        <serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>
        <serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>
        <SCPDURL>/profiles/contentdirectory.xml</SCPDURL>
        <controlURL>/upnp/services/content-directory/control</controlURL>
        <eventSubURL>/upnp/services/content-directory/events</eventSubURL>
      </service>
      <service>
        <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>
        <serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>
        <SCPDURL>/profiles/connectionmanager.xml</SCPDURL>
        <controlURL>/upnp/services/connection-manager/control</controlURL>
        <eventSubURL>/upnp/services/connection-manager/events</eventSubURL>
      </service>
    </serviceList>
  </device>
</root>)";
}

inline std::string connectionManagerProfileXml()
{
    return R"(<?xml version="1.0" encoding="utf-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <actionList>
    <action><name>GetProtocolInfo</name><argumentList>
      <argument><name>Source</name><direction>out</direction><relatedStateVariable>SourceProtocolInfo</relatedStateVariable></argument>
      <argument><name>Sink</name><direction>out</direction><relatedStateVariable>SinkProtocolInfo</relatedStateVariable></argument>
    </argumentList></action>
  </actionList>
  <serviceStateTable>
    <stateVariable sendEvents="yes"><name>SourceProtocolInfo</name><dataType>string</dataType></stateVariable>
    <stateVariable sendEvents="yes"><name>SinkProtocolInfo</name><dataType>string</dataType><defaultValue>http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_FLAGS=ED100000000000000000000000000000,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_FLAGS=ED100000000000000000000000000000</defaultValue></stateVariable>
  </serviceStateTable>
</scpd>)";
}

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

    ENDPOINT("GET", "/description.xml", descriptionXml)
    {
        return createResponse(Status::CODE_200, descriptionProfileXml());
    }

    ENDPOINT("GET", "/profiles/{profileName}", profileXml, PATH(String, profileName))
    {
        const std::string name = profileName->c_str();
        if (name == "description.xml")
            return createResponse(Status::CODE_200, descriptionProfileXml());
        if (name == "contentdirectory.xml")
            return createResponse(Status::CODE_200, contentDirectoryProfileXml());
        if (name == "connectionmanager.xml")
            return createResponse(Status::CODE_200, connectionManagerProfileXml());
        return createResponse(Status::CODE_404, "Profile not found");
    }

    ENDPOINT("GET", "/stream/live/{streamName}", liveStream, PATH(String, streamName))
    {
        const std::string resourceName = streamName->c_str();
        const std::string host = m_desc && m_desc->ipPort ? std::string(m_desc->ipPort->c_str()) : std::string("127.0.0.1:8000");
        const std::string streamUrl = std::string("http://") + host + "/media/video/" + resourceName + ".mp4";
        std::ostringstream playlist;
        playlist << "#EXTM3U\n#EXT-X-VERSION:3\n#EXTINF:0,"
                 << (resourceName.empty() ? "desktop" : resourceName)
                 << "\n"
                 << streamUrl
                 << "\n";
        auto response = createResponse(Status::CODE_200, playlist.str());
        response->putHeader("Content-Type", "application/vnd.apple.mpegurl");
        response->putHeader("Cache-Control", "no-cache");
        return response;
    }

    ENDPOINT("POST", "/upnp/services/content-directory/control", contentDirectoryControl, BODY_STRING(String, body))
    {
        const std::string soapBody = body;
        const std::string actionName = soapBody.find("GetSystemUpdateID") != std::string::npos ? "GetSystemUpdateID"
            : soapBody.find("GetSearchCapabilities") != std::string::npos ? "GetSearchCapabilities"
            : soapBody.find("GetSortCapabilities") != std::string::npos ? "GetSortCapabilities"
            : "Browse";

        std::string responseBody;
        const std::string host = m_desc && m_desc->ipPort ? std::string(m_desc->ipPort->c_str()) : std::string("127.0.0.1:8000");
        if (actionName == "Browse")
        {
            std::string title = "Desktop Stream";
            std::string mediaType = "video";
            std::string streamUrl = std::string("http://") + host + "/stream/live/desktop.m3u8";
            responseBody = buildContentDirectoryBrowseResponse("0", title, streamUrl, mediaType);
        }
        else if (actionName == "GetSystemUpdateID")
        {
            responseBody = buildContentDirectoryActionResponse(actionName, "<Id>0</Id>");
        }
        else if (actionName == "GetSearchCapabilities")
        {
            responseBody = buildContentDirectoryActionResponse(actionName, "<SearchCaps></SearchCaps>");
        }
        else if (actionName == "GetSortCapabilities")
        {
            responseBody = buildContentDirectoryActionResponse(actionName, "<SortCaps></SortCaps>");
        }

        return createResponse(Status::CODE_200, responseBody);
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
        response->putHeader("ContentFeatures.DLNA.ORG", "DLNA.ORG_PN=LPCM;DLNA.ORG_FLAGS=ED100000000000000000000000000000");
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
        const auto audioKind = detectAudioKind(filePath);
        const int sampleRate = 44100;
        const int channels = 2;

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
        const auto headers = pickAudioDlnaHeaders(audioKind, sampleRate, channels);
        response->putHeader("Content-Type", headers.contentType.c_str());
        response->putHeader("ContentFeatures.DLNA.ORG", headers.contentFeatures.c_str());
        response->putHeader("Scid.DLNA.ORG", "839080694");
        response->putHeader("TransferMode.DLNA.ORG", "Streaming");
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

    ENDPOINT("POST", "/upnp/sendMedia", sendMediaToTv, BODY_DTO(Object<SendMediaRequestDTO>, req))
    {
        if (!m_upnp || m_upnp->handle() == -1)
        {
            auto err = UpnpErrorDTO::createShared();
            err->statusCode = 500;
            err->message = "UPnP client is not initialized";
            return createDtoResponse(Status::CODE_500, err);
        }

        const auto asString = [](const oatpp::String &value, const char *fallback) {
            return value ? std::string(value->c_str()) : std::string(fallback);
        };

        const std::string sourceType = asString(req->sourceType, "file");
        const std::string mediaType = asString(req->mediaType, "video");
        const std::string streamUrl = asString(req->streamUrl, "");
        const std::string mimeType = asString(req->mimeType, "");
        const std::string title = asString(req->title, "");
        std::string mediaUrl = asString(req->mediaUrl, "");
        std::string filePath = asString(req->filePath, "");

        const bool isExplicitStream = !streamUrl.empty() || sourceType == "stream" || sourceType == "screen" || sourceType == "desktop" || sourceType == "vlc" || sourceType == "live";
        const bool isAudio = isExplicitStream
            ? (mediaType == "audio" || mediaType == "mp3" || mediaType == "wav" || sourceType == "audio")
            : (mediaType == "audio" || mediaType == "mp3" || mediaType == "wav");

        if (isExplicitStream && mediaUrl.empty())
            mediaUrl = streamUrl;

        if (mediaUrl.empty() && !filePath.empty())
        {
            std::string cleanPath = filePath;
            if (cleanPath.rfind("/", 0) != 0 && cleanPath.rfind("http", 0) != 0)
                cleanPath = std::string("/") + cleanPath;
            mediaUrl = std::string(SERVER_ADDRESS) + "/media/" + (isAudio ? "audio" : "video") + cleanPath;
        }
        if (mediaUrl.empty())
        {
            auto err = UpnpErrorDTO::createShared();
            err->statusCode = 400;
            err->message = "mediaUrl, streamUrl, or filePath is required";
            return createDtoResponse(Status::CODE_400, err);
        }

        const std::string actionUrl = asString(req->actionUrl, "http://192.168.0.100:52235/upnp/control/AVTransport1");
        const std::string serviceId = asString(req->serviceId, "urn:upnp-org:serviceId:AVTransport");
        const std::string serviceType = asString(req->serviceType, "urn:schemas-upnp-org:service:AVTransport:1");
        const std::string instanceId = asString(req->instanceId, "0");

        const std::string itemTitle = !title.empty() ? title
            : filePath.empty() ? (isAudio ? "Audio stream" : "Video stream")
            : fileStem(filePath);
        const std::string metadata = isExplicitStream
            ? generateStreamDidlLite(itemTitle, mediaUrl, isAudio ? "audio" : "video", mimeType)
            : (isAudio ? generateDidlLite(filePath.empty() ? mediaUrl : filePath, mediaUrl)
                       : generateVideoDidlLite(filePath.empty() ? mediaUrl : filePath, mediaUrl));

        std::vector<actionArg> setArgs = {
            {"InstanceID", "in", "A_ARG_TYPE_InstanceID", instanceId},
            {"CurrentURI", "in", "AVTransportURI", mediaUrl},
            {"CurrentURIMetaData", "in", "AVTransportURIMetaData", metadata}
        };

        IXML_Document *setDoc = createActionDocument("SetAVTransportURI", serviceId, setArgs);
        IXML_Document *setResp = nullptr;
        int ret = UpnpSendAction(m_upnp->handle(), actionUrl.c_str(), serviceType.c_str(), nullptr, setDoc, &setResp);
        ixmlDocument_free(setDoc);
        if (ret != UPNP_E_SUCCESS)
        {
            auto err = UpnpErrorDTO::createShared();
            err->upnpErrorCode = ret;
            err->message = setResp ? ixmlDocumenttoString(setResp) : "SetAVTransportURI failed";
            ixmlDocument_free(setResp);
            return createDtoResponse(Status::CODE_422, err);
        }
        ixmlDocument_free(setResp);

        std::vector<actionArg> playArgs = {
            {"InstanceID", "in", "A_ARG_TYPE_InstanceID", instanceId},
            {"Speed", "in", "TransportPlaySpeed", "1"}
        };
        IXML_Document *playDoc = createActionDocument("Play", serviceId, playArgs);
        IXML_Document *playResp = nullptr;
        ret = UpnpSendAction(m_upnp->handle(), actionUrl.c_str(), serviceType.c_str(), nullptr, playDoc, &playResp);
        ixmlDocument_free(playDoc);
        if (ret != UPNP_E_SUCCESS)
        {
            auto err = UpnpErrorDTO::createShared();
            err->upnpErrorCode = ret;
            err->message = playResp ? ixmlDocumenttoString(playResp) : "Play failed";
            ixmlDocument_free(playResp);
            return createDtoResponse(Status::CODE_422, err);
        }
        ixmlDocument_free(playResp);

        auto response = MessageDto::createShared();
        response->statusCode = 200;
        response->message = isAudio ? "Audio stream queued on TV" : "Video stream queued on TV";
        return createDtoResponse(Status::CODE_200, response);
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

#endif // UPNP_DEVICE_HPP
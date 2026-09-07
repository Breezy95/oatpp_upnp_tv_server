#include "ixmlfuncs.hpp"
#include <upnp/ixml.h>
#include <string>
#include <iostream>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

std::string xmlEscape(const std::string &value)
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

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string fileStem(const std::string &filePath)
{
    const auto slashPos = filePath.find_last_of("/\\");
    std::string name = slashPos == std::string::npos ? filePath : filePath.substr(slashPos + 1);
    const auto dotPos = name.find_last_of('.');
    return dotPos == std::string::npos ? name : name.substr(0, dotPos);
}

std::string toDlnaMime(const std::string &filePath)
{
    const auto lowered = toLower(filePath);
    if (lowered.find(".wav") != std::string::npos || lowered.find(".pcm") != std::string::npos)
        return "audio/L16";
    if (lowered.find(".mp3") != std::string::npos || lowered.find(".m4a") != std::string::npos || lowered.find(".aac") != std::string::npos)
        return "audio/mpeg";
    return "audio/mpeg";
}

} // namespace

IXML_Document *createBaseDocument()
{
    IXML_Document *doc = ixmlDocument_createDocument();
    if (!doc)
        return nullptr;

    // Create Envelope
    IXML_Element *envelope =
        ixmlDocument_createElementNS(
            doc,
            "http://schemas.xmlsoap.org/soap/envelope/",
            "s:Envelope");
    if (!envelope)
    {
        ixmlDocument_free(doc);
        return nullptr;
    }

    ixmlElement_setAttribute(envelope, "xmlns:s", "http://schemas.xmlsoap.org/soap/envelope/");
    ixmlElement_setAttribute(envelope, "s:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/");

    ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)envelope);

    // Create Body
    IXML_Element *body =
        ixmlDocument_createElementNS(
            doc,
            "http://schemas.xmlsoap.org/soap/envelope/",
            "s:Body");
    if (!body)
    {
        ixmlDocument_free(doc);
        return nullptr;
    }

    ixmlNode_appendChild((IXML_Node *)envelope, (IXML_Node *)body);

    return doc;
}

IXML_Document *createGetProtocolInfoDocument()
{
    IXML_Document *baseDoc = ixmlDocument_createDocument();
    if (!baseDoc)
    {
        return nullptr;
    }
    /*
        IXML_Element *envelope = ixmlDocument_createElementNS(
            baseDoc,
            "http://schemas.xmlsoap.org/soap/envelope/",
            "SOAP-ENV:Envelope");

        if (!envelope)
        {
            ixmlDocument_free(baseDoc);
            return nullptr;
        }

        ixmlElement_setAttribute(envelope, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/");
        ixmlElement_setAttribute(
            envelope,
            "SOAP-ENV:encodingStyle",
            "http://schemas.xmlsoap.org/soap/encoding/");

        // Create Body element
        IXML_Element *body = ixmlDocument_createElementNS(
            baseDoc,
            "http://schemas.xmlsoap.org/soap/envelope/",
            "SOAP-ENV:Body");
        if (!body)
        {
            ixmlDocument_free(baseDoc);
            return nullptr;
        }

        ixmlNode_appendChild((IXML_Node *)baseDoc, (IXML_Node *)envelope);
        ixmlNode_appendChild((IXML_Node *)envelope, (IXML_Node *)body);

        IXML_Element *action = ixmlDocument_createElementNS(
            baseDoc,
            "urn:schemas-upnp-org:service:ConnectionManager:1", // service type
            "m:GetProtocolInfo");
        if (!action)
        {
            ixmlDocument_free(baseDoc);
            return nullptr;
        }

        ixmlNode_appendChild(&body->n, &action->n);
        */

    return baseDoc;
}

IXML_Document *createGetVolumeDocument(unsigned int instanceId, std::string channel)
{
    IXML_Document *doc = ixmlDocument_createDocument();
    if (!doc)
        return nullptr;

    // Create <u:GetVolume> element with correct namespace
    IXML_Element *action =
        ixmlDocument_createElementNS(
            doc,
            "urn:upnp-org:serviceId:RenderingControl",
            "u:GetVolume");
    ixmlElement_setAttribute(action, "xmlns:u", "urn:upnp-org:serviceId:RenderingControl");

    // InstanceID
    IXML_Element *instElem = ixmlDocument_createElement(doc, "InstanceID");
    if (instElem)
    {
        const char *idStr = "0000"; // Samsung expects 0000
        IXML_Node *instText = ixmlDocument_createTextNode(doc, idStr);
        ixmlNode_appendChild((IXML_Node *)instElem, instText);
        ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)instElem);
    }

    // Channel
    IXML_Element *channelElem = ixmlDocument_createElement(doc, "Channel");
    if (channelElem)
    {
        IXML_Node *channelText = ixmlDocument_createTextNode(doc, "Master");
        ixmlNode_appendChild((IXML_Node *)channelElem, channelText);
        ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)channelElem);
        ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)action);
    }

    return doc;
}

IXML_Document *createSetVolumeDocument(unsigned int desiredVolume, unsigned int instanceId, const char *channel)
{
    IXML_Document *doc = createBaseDocument();
    if (!doc)
        return nullptr;

    // Find the Body element
    IXML_NodeList *bodyList = ixmlDocument_getElementsByTagName(doc, "Body");
    if (!bodyList || ixmlNodeList_length(bodyList) == 0)
    {
        if (bodyList)
            ixmlNodeList_free(bodyList);
        ixmlDocument_free(doc);
        return nullptr;
    }

    IXML_Node *bodyNode = ixmlNodeList_item(bodyList, 0);
    ixmlNodeList_free(bodyList);

    // Create the SetVolume action element in the RenderingControl namespace
    IXML_Element *action = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:service:RenderingControl:1", "m:SetVolume");
    if (!action)
    {
        ixmlDocument_free(doc);
        return nullptr;
    }

    // InstanceID
    IXML_Element *instElem = ixmlDocument_createElement(doc, "InstanceID");
    if (instElem)
    {
        std::string instStr = std::to_string(instanceId);
        IXML_Node *instText = ixmlDocument_createTextNode(doc, instStr.c_str());
        if (instText)
            ixmlNode_appendChild((IXML_Node *)instElem, instText);
        ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)instElem);
    }

    // Channel
    IXML_Element *chanElem = ixmlDocument_createElement(doc, "Channel");
    if (chanElem)
    {
        IXML_Node *chanText = ixmlDocument_createTextNode(doc, channel);
        if (chanText)
            ixmlNode_appendChild((IXML_Node *)chanElem, chanText);
        ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)chanElem);
    }

    // DesiredVolume
    IXML_Element *volElem = ixmlDocument_createElement(doc, "DesiredVolume");
    if (volElem)
    {
        std::string volStr = std::to_string(desiredVolume);
        IXML_Node *volText = ixmlDocument_createTextNode(doc, volStr.c_str());
        if (volText)
            ixmlNode_appendChild((IXML_Node *)volElem, volText);
        ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)volElem);
    }

    // Append the action to the body
    ixmlNode_appendChild(bodyNode, (IXML_Node *)action);

    return doc;
}

struct MediaMetadata
{
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int durationSec = 0;
    int bitrate = 0;
};

MediaMetadata getMetadataFromFile(const std::string &filepath)
{
    MediaMetadata meta;
    meta.title = fileStem(filepath);
    if (meta.title.empty())
        meta.title = "Unknown Title";

    const auto lowered = toLower(filepath);
    if (lowered.find(".mp3") != std::string::npos)
    {
        meta.genre = "Audio";
        meta.bitrate = 320;
    }
    else if (lowered.find(".wav") != std::string::npos || lowered.find(".pcm") != std::string::npos)
    {
        meta.genre = "Audio";
        meta.bitrate = 1411;
    }
    else
    {
        meta.genre = "Media";
    }
    return meta;
}

IXML_Document *createDIDLFromFFmpeg(
    const std::string &fileUri,
    const std::string &localFilePath)
{
    MediaMetadata meta = getMetadataFromFile(localFilePath);

    IXML_Document *doc = ixmlDocument_createDocument();

    IXML_Element *didl = ixmlDocument_createElementNS(
        doc,
        "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/",
        "DIDL-Lite");
    ixmlElement_setAttribute(didl, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    ixmlElement_setAttribute(didl, "xmlns:upnp", "urn:schemas-upnp-org:metadata-1-0/upnp/");
    ixmlNode_appendChild(&doc->n, &didl->n);

    IXML_Element *item = ixmlDocument_createElement(doc, "item");
    ixmlElement_setAttribute(item, "id", "0");
    ixmlElement_setAttribute(item, "parentID", "-1");
    ixmlElement_setAttribute(item, "restricted", "false");
    ixmlNode_appendChild(&didl->n, &item->n);

    auto addText = [&](const char *name, const std::string &value)
    {
        if (value.empty())
            return;
        IXML_Element *e = ixmlDocument_createElement(doc, name);
        ixmlNode_appendChild(&e->n,
                             ixmlDocument_createTextNode(doc, value.c_str()));
        ixmlNode_appendChild(&item->n, &e->n);
    };

    addText("upnp:class", "object.item.audioItem.musicTrack");
    addText("dc:title", meta.title.empty() ? "Unknown Title" : meta.title);
    addText("dc:creator", meta.artist.empty() ? "Unknown Artist" : meta.artist);
    addText("upnp:album", meta.album);
    addText("dc:genre", meta.genre);

    // <res>
    IXML_Element *res = ixmlDocument_createElement(doc, "res");

    std::string protocolInfo =
        "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;"
        "DLNA.ORG_FLAGS=ED100000000000000000000000000000";

    ixmlElement_setAttribute(res, "protocolInfo", protocolInfo.c_str());

    if (meta.bitrate > 0)
        ixmlElement_setAttribute(res, "bitrate",
                                 std::to_string(meta.bitrate * 1000).c_str());

    if (meta.durationSec > 0)
    {
        char dur[32];
        snprintf(dur, sizeof(dur), "%d:%02d:%02d",
                 meta.durationSec / 3600,
                 (meta.durationSec / 60) % 60,
                 meta.durationSec % 60);
        ixmlElement_setAttribute(res, "duration", dur);
    }

    ixmlNode_appendChild(&res->n,
                         ixmlDocument_createTextNode(doc, fileUri.c_str()));
    ixmlNode_appendChild(&item->n, &res->n);

    return doc;
}

IXML_Document *createMetadataArgs(std::string fp)
{
    IXML_Document *doc = ixmlDocument_createDocument();
    IXML_Element *didlLite = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/", "DIDL-Lite");
    ixmlElement_setAttribute(didlLite, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    ixmlElement_setAttribute(didlLite, "xmlns:upnp", "urn:schemas-upnp-org:metadata-1-0/upnp/");
    ixmlNode_appendChild(&(doc->n), &(didlLite->n));

    IXML_Element *item = ixmlDocument_createElement(doc, "item");
    ixmlElement_setAttribute(item, "id", "0");
    ixmlElement_setAttribute(item, "parentID", "0");
    ixmlElement_setAttribute(item, "restricted", "false");
    ixmlNode_appendChild(&(didlLite->n), &(item->n));

    IXML_Element *title = ixmlDocument_createElement(doc, "dc:title");
    IXML_Node *titleText = ixmlDocument_createTextNode(doc, "BisexualLight");
    ixmlNode_appendChild(&title->n, titleText);
    ixmlNode_appendChild(&item->n, &title->n);

    IXML_Element *creator = ixmlDocument_createElement(doc, "dc:creator");
    IXML_Node *creatorText = ixmlDocument_createTextNode(doc, "Example Artist");
    ixmlNode_appendChild(&creator->n, creatorText);
    ixmlNode_appendChild(&item->n, &creator->n);

    IXML_Element *res = ixmlDocument_createElement(doc, "res");
    ixmlElement_setAttribute(res, "protocolInfo", "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_FLAGS=ED100000000000000000000000000000");
    IXML_Node *resText = ixmlDocument_createTextNode(doc, "http://192.168.0.212:8000/media/audio/flashbang-jumpscare-loud.mp3");
    ixmlNode_appendChild(&res->n, resText);
    ixmlNode_appendChild(&item->n, &res->n);

    IXML_Element *upnpClass = ixmlDocument_createElement(doc, "upnp:class");
    IXML_Node *classText = ixmlDocument_createTextNode(doc, "object.item.audioItem");
    ixmlNode_appendChild(&upnpClass->n, classText);
    ixmlNode_appendChild(&item->n, &upnpClass->n);
    return doc;
}

//pulled from a earlier piece of code that has a bunch of 
IXML_Document* createMetadataDocument() {
    IXML_Document* doc = ixmlDocument_createDocument();
    IXML_Element* didlLite = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/", "DIDL-Lite");
    ixmlElement_setAttribute(didlLite, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    ixmlElement_setAttribute(didlLite, "xmlns:upnp", "urn:schemas-upnp-org:metadata-1-0/upnp/");
    ixmlNode_appendChild(&(doc->n), &(didlLite->n));

    IXML_Element* item = ixmlDocument_createElement(doc, "item");
    ixmlElement_setAttribute(item, "id", "0");
    ixmlElement_setAttribute(item, "parentID", "0");
    ixmlElement_setAttribute(item, "restricted", "False");
    ixmlNode_appendChild(&(didlLite->n), &(item->n));

    IXML_Element* title = ixmlDocument_createElement(doc, "dc:title");
    IXML_Node* titleText = ixmlDocument_createTextNode(doc, "BiLight");
    ixmlNode_appendChild(&title->n, titleText);
    ixmlNode_appendChild(&item->n, &title->n);

    IXML_Element* creator = ixmlDocument_createElement(doc, "dc:creator");
    IXML_Node* creatorText = ixmlDocument_createTextNode(doc, "Example Artist");
    ixmlNode_appendChild(&creator->n, creatorText);
    ixmlNode_appendChild(&item->n, &creator->n);

/*
<res nrAudioChannels="2" duration="1:48:55.701" bitrate="327040"
microsoft:codec="{34363248-0000-0010-8000-00AA00389B71}"
protocolInfo="http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_OP=10;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=01700000000000000000000000000000"
sampleFrequency="44100" resolution="640x360">
"http://192.168.0.212:10246/MDEServer/89A83FB7-F5E2-4BE6-B5AC-0676CCB2C3F5/1000.mp4?formatID=00000041-A9AF-4584-84E2-55BFEF0A7D7E,instance=1"
   </res>
   */

  /*
 res bitrate="327040" resolution="640x360"
            protocolInfo="http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=01700000000000000000000000000000"
            sampleFrequency="44100" nrAudioChannels="2"
            microsoft:codec="{34363248-0000-0010-8000-00AA00389B71}">
            http://192.168.0.212:10246/MDEServer/9BC98514-6D84-47B9-9E7B-A1E70D6F7E65/1000.mp4?formatID=00000041-A9AF-4584-84E2-55BFEF0A7D7E</res> 
  */
    IXML_Element* res = ixmlDocument_createElement(doc, "res");
    ixmlElement_setAttribute(res, "protocolInfo", 
    "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_FLAGS=ED100000000000000000000000000000");
    ixmlElement_setAttribute(res, "sampleFrequency", "44100");
    //ixmlElement_setAttribute(res, "duration", "1:48:55.701");
    ixmlElement_setAttribute(res, "bitrate", "327040");
    ixmlElement_setAttribute(res, "microsoft:codec", "{34363248-0000-0010-8000-00AA00389B71}");
    ixmlElement_setAttribute(res, "resolution","640x360");
    IXML_Node* resText = ixmlDocument_createTextNode(doc, "http://192.168.0.212:8000/media/audio/flashbang-jumpscare-loud.mp3");
    ixmlNode_appendChild(&res->n, resText);
    ixmlNode_appendChild(&item->n, &res->n);


    
    IXML_Element* upnpClass = ixmlDocument_createElement(doc, "upnp:class");
    IXML_Node* classText = ixmlDocument_createTextNode(doc, "object.item.audioItem");
    ixmlNode_appendChild(&upnpClass->n, classText);
    ixmlNode_appendChild(&item->n, &upnpClass->n);
    return doc;
}

// action, args, serviceid,
IXML_Document *createActionDocument(std::string actionName, std::string serviceId, std::vector<actionArg> args)
{
    IXML_Document *doc = ixmlDocument_createDocument();
    if (!doc)
        return nullptr;

    IXML_Element *action =
        ixmlDocument_createElementNS(
            doc,
            serviceId.c_str(),
            ("u:" + actionName).c_str());

    ixmlElement_setAttribute(action, "xmlns:u", serviceId.c_str());

    for (int i = 0; i < args.size(); i++)
    {

        if (args[i].name.compare("CurrentURIMetaData") == 0)
        {
            IXML_Element *argElement = ixmlDocument_createElement(doc, args.at(i).name.c_str());

            auto metadatadoc = createMetadataArgs(args[i].val);
            IXML_Node *metadata = ixmlDocument_createTextNode(doc, ixmlPrintDocument(metadatadoc));
            // need an escaped xml string

            ixmlNode_appendChild(&argElement->n, metadata);
            ixmlNode_appendChild(&action->n, &argElement->n);
        }
        else
        {
            IXML_Element *argNode = ixmlDocument_createElement(doc, args.at(i).name.c_str());
            IXML_Node *value = ixmlDocument_createTextNode(doc, args.at(i).val.c_str());
            ixmlNode_appendChild((IXML_Node *)argNode, value);
            ixmlNode_appendChild((IXML_Node *)action, (IXML_Node *)argNode);
        }
    }

    ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)action);

    return doc;
}

IXML_Document* createMetadataAudioDocument(std::map<std::string,std::string> &fileInfo){
        IXML_Document* doc = ixmlDocument_createDocument();
    IXML_Element* didlLite = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/", "DIDL-Lite");
    ixmlElement_setAttribute(didlLite, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    ixmlElement_setAttribute(didlLite, "xmlns:upnp", "urn:schemas-upnp-org:metadata-1-0/upnp/");
    ixmlNode_appendChild(&(doc->n), &(didlLite->n));

    IXML_Element* item = ixmlDocument_createElement(doc, "item");
    ixmlElement_setAttribute(item, "id", "0");
    ixmlElement_setAttribute(item, "parentID", "0");
    ixmlElement_setAttribute(item, "restricted", "False");
    ixmlNode_appendChild(&(didlLite->n), &(item->n));

    IXML_Element* title = ixmlDocument_createElement(doc, "dc:title");
    IXML_Node* titleText = ixmlDocument_createTextNode(doc, fileInfo["title"].c_str());
    ixmlNode_appendChild(&title->n, titleText);
    ixmlNode_appendChild(&item->n, &title->n);

    IXML_Element* creator = ixmlDocument_createElement(doc, "dc:creator");
    IXML_Node* creatorText = ixmlDocument_createTextNode(doc, "Example Artist");
    ixmlNode_appendChild(&creator->n, creatorText);
    ixmlNode_appendChild(&item->n, &creator->n);

    IXML_Element* res = ixmlDocument_createElement(doc, "res");
    ixmlElement_setAttribute(res, "protocolInfo", 
    "http-get:*:audio/wav:DLNA.ORG_PN=LPCM;DLNA.ORG_FLAGS=ED100000000000000000000000000000");
    //ixmlElement_setAttribute(res, "sampleFrequency", "44100");
    //ixmlElement_setAttribute(res, "duration", "1:48:55.701");
    //ixmlElement_setAttribute(res, "bitrate", "327040");
    //ixmlElement_setAttribute(res, "microsoft:codec", "{34363248-0000-0010-8000-00AA00389B71}");
    //ixmlElement_setAttribute(res, "resolution","640x360");
    IXML_Node* resText = ixmlDocument_createTextNode(doc, (std::string(SERVER_ADDRESS) + fileInfo["filename"]).c_str());
    ixmlNode_appendChild(&res->n, resText);
    ixmlNode_appendChild(&item->n, &res->n);


    
    IXML_Element* upnpClass = ixmlDocument_createElement(doc, "upnp:class");
    IXML_Node* classText = ixmlDocument_createTextNode(doc, "object.item.audioItem");
    ixmlNode_appendChild(&upnpClass->n, classText);
    ixmlNode_appendChild(&item->n, &upnpClass->n);
    return doc;
}
IXML_Document * createSetAVTransportURIDoc(std::map<std::string, std::string> &args)
{
    IXML_Document *doc = ixmlDocument_createDocument();

    IXML_Element *action = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:service:AVTransport:1", "u:SetAVTransportURI");
    int ret = ixmlElement_setAttribute(action, "xmlns:u", "urn:upnp-org:serviceId:AVTransport");
    if (ret != 0)
    {
        std::cout << "operation failed:" << ret << std::endl;
    }
    ixmlNode_appendChild(&(doc->n), &(action->n));

    for (const auto &arg : args)
    {
        IXML_Element *argElement = ixmlDocument_createElement(doc, arg.first.c_str());
        IXML_Node *argTextNode = ixmlDocument_createTextNode(doc, arg.second.c_str());
        ixmlNode_appendChild(&argElement->n, argTextNode);
        ixmlNode_appendChild(&action->n, &argElement->n);
    }

    return doc;
}


std::string generateDidlLite(
    const std::string& filePath,
    const std::string& streamUrl)
{
    const std::string title = fileStem(filePath);
    const std::string mime = toDlnaMime(filePath);
    const std::string pn = mime == "audio/L16" ? "LPCM" : "MP3";
    const std::string protocolInfo =
        "http-get:*:" + mime +
        ":DLNA.ORG_PN=" + pn + ";DLNA.ORG_FLAGS=ED100000000000000000000000000000";

    std::ostringstream didl;
    didl
        << "<DIDL-Lite "
        << "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
        << "<item id=\"0\" parentID=\"0\" restricted=\"false\">"
        << "<dc:title>" << xmlEscape(title) << "</dc:title>"
        << "<res protocolInfo=\"" << protocolInfo << "\" "
        << "sampleFrequency=\"44100\" "
        << "nrAudioChannels=\"2\" "
        << "bitrate=\"320000\">"
        << xmlEscape(streamUrl)
        << "</res>"
        << "<upnp:class>object.item.audioItem</upnp:class>"
        << "</item>"
        << "</DIDL-Lite>";

    return didl.str();
}

std::string generateVideoDidlLite(
    const std::string& filePath,
    const std::string& streamUrl)
{
    const std::string title = fileStem(filePath);
    const std::string lowered = toLower(filePath);
    const std::string mime = lowered.find(".mp4") != std::string::npos || lowered.find(".mkv") != std::string::npos || lowered.find(".avi") != std::string::npos
                                ? "video/mp4"
                                : "video/mpeg";
    const std::string protocolInfo =
        "http-get:*:" + mime +
        ":DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC;DLNA.ORG_FLAGS=ED100000000000000000000000000000";

    std::ostringstream didl;
    didl
        << "<DIDL-Lite "
        << "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
        << "<item id=\"0\" parentID=\"0\" restricted=\"false\">"
        << "<dc:title>" << xmlEscape(title) << "</dc:title>"
        << "<res protocolInfo=\"" << protocolInfo << "\" "
        << "resolution=\"1280x720\" "
        << "bitrate=\"4500000\">"
        << xmlEscape(streamUrl)
        << "</res>"
        << "<upnp:class>object.item.videoItem</upnp:class>"
        << "</item>"
        << "</DIDL-Lite>";

    return didl.str();
}

std::string generateStreamDidlLite(
    const std::string& title,
    const std::string& streamUrl,
    const std::string& mediaType,
    const std::string& mimeType)
{
    const std::string lowerType = toLower(mediaType);
    const bool isAudio = lowerType == "audio" || lowerType == "mp3" || lowerType == "wav" || lowerType == "pcm";
    const std::string resolvedTitle = title.empty() ? (isAudio ? "Audio stream" : "Video stream") : title;
    const std::string resolvedMime = mimeType.empty()
        ? (isAudio ? "audio/mpeg" : "video/mp4")
        : mimeType;
    const std::string dlnaPn = isAudio ? (resolvedMime == "audio/L16" ? "LPCM" : "MP3") : "AVC_MP4_BL_L3L_SD_AAC";
    const std::string protocolInfo = isAudio
        ? "http-get:*:" + resolvedMime + ":DLNA.ORG_PN=" + dlnaPn + ";DLNA.ORG_FLAGS=ED100000000000000000000000000000"
        : "http-get:*:" + resolvedMime + ":DLNA.ORG_PN=" + dlnaPn + ";DLNA.ORG_FLAGS=ED100000000000000000000000000000";

    std::ostringstream didl;
    didl
        << "<DIDL-Lite "
        << "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
        << "<item id=\"0\" parentID=\"0\" restricted=\"false\">"
        << "<dc:title>" << xmlEscape(resolvedTitle) << "</dc:title>"
        << "<res protocolInfo=\"" << protocolInfo << "\" "
        << (isAudio ? "sampleFrequency=\"44100\" nrAudioChannels=\"2\" bitrate=\"320000\"" : "resolution=\"1280x720\" bitrate=\"4500000\"")
        << ">"
        << xmlEscape(streamUrl)
        << "</res>"
        << "<upnp:class>object.item." << (isAudio ? "audioItem" : "videoItem") << "</upnp:class>"
        << "</item>"
        << "</DIDL-Lite>";

    return didl.str();
}

namespace {

std::string firstChildText(IXML_Element *parent, const char *tag)
{
    if (!parent)
        return std::string();

    IXML_NodeList *nodes = ixmlElement_getElementsByTagName(parent, tag);
    std::string value;
    if (nodes && ixmlNodeList_length(nodes) > 0)
    {
        IXML_Node *node = ixmlNodeList_item(nodes, 0);
        if (node)
        {
            const char *text = ixmlNode_getNodeValue(ixmlNode_getFirstChild(node));
            if (text)
                value = text;
        }
    }
    if (nodes)
        ixmlNodeList_free(nodes);
    return value;
}

} // namespace

std::string resolveUrl(const std::string &baseUrl, const std::string &url)
{
    if (url.empty())
        return url;
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0)
        return url;
    if (baseUrl.empty())
        return url;

    std::string base = baseUrl;
    if (url.front() == '/')
    {
        // Absolute path: keep only scheme and authority of the base URL.
        const auto schemeEnd = base.find("://");
        const auto pathStart = schemeEnd == std::string::npos
                                   ? base.find('/')
                                   : base.find('/', schemeEnd + 3);
        if (pathStart != std::string::npos)
            base = base.substr(0, pathStart);
        return base + url;
    }

    if (base.back() != '/')
        base += '/';
    return base + url;
}

deviceDescription parseDeviceDescription(const std::string &xml, const std::string &baseUrl)
{
    deviceDescription description;
    if (xml.empty())
        return description;

    IXML_Document *doc = nullptr;
    if (ixmlParseBufferEx(xml.c_str(), &doc) != IXML_SUCCESS || !doc)
        return description;

    IXML_NodeList *devices = ixmlDocument_getElementsByTagName(doc, "device");
    if (devices && ixmlNodeList_length(devices) > 0)
    {
        auto *device = (IXML_Element *)ixmlNodeList_item(devices, 0);
        description.friendlyName = firstChildText(device, "friendlyName");
        description.manufacturer = firstChildText(device, "manufacturer");
        description.modelName = firstChildText(device, "modelName");
        description.udn = firstChildText(device, "UDN");
    }
    if (devices)
        ixmlNodeList_free(devices);

    IXML_NodeList *services = ixmlDocument_getElementsByTagName(doc, "service");
    if (services)
    {
        for (unsigned long i = 0; i < ixmlNodeList_length(services); ++i)
        {
            auto *service = (IXML_Element *)ixmlNodeList_item(services, i);
            const std::string serviceType = firstChildText(service, "serviceType");
            if (serviceType.find("AVTransport") == std::string::npos)
                continue;

            description.serviceType = serviceType;
            description.serviceId = firstChildText(service, "serviceId");
            description.controlUrl = resolveUrl(baseUrl, firstChildText(service, "controlURL"));
            break;
        }
        ixmlNodeList_free(services);
    }

    ixmlDocument_free(doc);
    return description;
}

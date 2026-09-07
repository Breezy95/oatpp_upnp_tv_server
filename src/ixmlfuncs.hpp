#ifndef IXML_FUNCS_HPP
#define IXML_FUNCS_HPP

#include <string>
#include <vector>
#include <upnp/ixml.h>
#include <upnp/upnptools.h>
#include <map>


struct actionArg {
std::string name;
std::string direction;
std::string relatedStateVariable;
std::string val;
};

// Fields extracted from a UPnP device description document.
struct deviceDescription {
std::string friendlyName;
std::string manufacturer;
std::string modelName;
std::string udn;
// Control URL and service type of the AVTransport service, when advertised.
std::string controlUrl;
std::string serviceType;
std::string serviceId;
};

// Parses a device description XML document. Relative URLs found in the document
// are resolved against `baseUrl` (the directory part of the description URL).
deviceDescription parseDeviceDescription(const std::string &xml, const std::string &baseUrl = "");

// Resolves a possibly relative URL against a base URL.
std::string resolveUrl(const std::string &baseUrl, const std::string &url);

IXML_Document* createBaseDocument();
IXML_Document* createGetProtocolInfoDocument();
// Create a SOAP action document for the RenderingControl:SetVolume action.
// - `desiredVolume`: absolute volume value to set (0-100)
// - `instanceId`: typically 0
// - `channel`: typically "Master"
IXML_Document* createSetVolumeDocument(unsigned int desiredVolume, unsigned int instanceId = 0, const char* channel = "Master");

IXML_Document* createGetVolumeDocument(unsigned int instanceId = 0, std::string channel = "Master");
IXML_Document *createActionDocument(std::string action, std::string serviceId, std::vector<actionArg> args);
IXML_Document *createMetadataForFile(std::string fp);
IXML_Document *createSetAVTransportURIDoc(std::map<std::string, std::string> &args);
IXML_Document* createMetadataDocument();
IXML_Document* createMetadataAudioDocument(std::map<std::string, std::string> &args);
IXML_Document *createMetadataArgs(std::string fp);
std::string generateDidlLite(const std::string& filePath,const std::string& streamUrl);
std::string generateVideoDidlLite(const std::string& filePath,const std::string& streamUrl);
std::string generateStreamDidlLite(const std::string& title,
                                  const std::string& streamUrl,
                                  const std::string& mediaType,
                                  const std::string& mimeType = "");

#endif // IXML_FUNCS_HPP
#ifndef IXML_FUNCS_HPP
#define IXML_FUNCS_HPP

#include <string>
#include <vector>
#include <upnpdebug.h>
#include <upnptools.h>
#include "ixml.h"
#include <map>


struct actionArg {
std::string name;
std::string direction;
std::string relatedStateVariable;
std::string val;
};

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

#endif // IXML_FUNCS_HPP
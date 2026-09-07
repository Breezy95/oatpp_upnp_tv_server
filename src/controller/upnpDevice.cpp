#include "upnpDevice.hpp"
#include <iostream>

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

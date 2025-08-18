#include "ixmlfuncs.hpp"

IXML_Document* createBaseDocument(){
    IXML_Document* baseDoc = ixmlDocument_createDocument();
    if (!baseDoc){
        return nullptr;
    }

     IXML_Element* envelope = ixmlDocument_createElementNS(
        baseDoc,
        "http://schemas.xmlsoap.org/soap/envelope/",
        "SOAP-ENV:Envelope"
    );

    if (!envelope) {
        ixmlDocument_free(baseDoc);
        return nullptr;
    }

     ixmlElement_setAttribute(
        envelope,
        "SOAP-ENV:encodingStyle",
        "http://schemas.xmlsoap.org/soap/encoding/"
    );

    // Create Body element
    IXML_Element* body = ixmlDocument_createElementNS(
        baseDoc,
        "http://schemas.xmlsoap.org/soap/envelope/",
        "SOAP-ENV:Body"
    );
    if (!body) {
        ixmlDocument_free(baseDoc);
        return nullptr;
    }

    ixmlNode_appendChild((IXML_Node*)baseDoc, (IXML_Node*)envelope);
    ixmlNode_appendChild((IXML_Node*)envelope, (IXML_Node*)body);
    return baseDoc;

}

IXML_Document* createGetProtocolInfoDocument(){

    IXML_Document* doc = createBaseDocument();
    IXML_NodeList* elems = ixmlDocument_getElementsByTagNameNS(doc,
        "http://schemas.xmlsoap.org/soap/envelope/",
        "SOAP-ENV:Body");
    
    
    IXML_Node* curr = ixmlNodeList_item(elems,0);


    IXML_Element* getProtocolInfo = ixmlDocument_createElementNS(
        doc,
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "m:GetProtocolInfo"
    );
    if (!getProtocolInfo) {
        ixmlDocument_free(doc);
        return nullptr;
    }

    
    //ixmlNode_appendChild((IXML_Node*)body, (IXML_Node*)getProtocolInfo);

    return doc;

}


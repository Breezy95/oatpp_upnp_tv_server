
#ifndef MyApiTestClient_hpp
#define MyApiTestClient_hpp

#include "oatpp/web/client/ApiClient.hpp"
#include "oatpp/core/macro/codegen.hpp"

/* Begin Api Client code generation */
#include OATPP_CODEGEN_BEGIN(ApiClient)

/**
 * Test API client.
 * Use this client to call application APIs.
 */
class MyApiTestClient : public oatpp::web::client::ApiClient {

  API_CLIENT_INIT(MyApiTestClient)

  API_CALL("GET", "/upnp/hello", getHelloWorld)
  API_CALL("GET", "/upnp/devices", getDeviceList)
  API_CALL("POST", "/upnp/search", searchDevices, BODY_DTO(Object<UpnpSearchRequest>, body))
  API_CALL("POST", "/upnp/sendMedia", sendMedia, BODY_DTO(Object<SendMediaRequestDTO>, body))
  API_CALL("GET", "/api/media/video", getVideoLibrary)
  API_CALL("GET", "/", getBrowsePage)

};

/* End Api Client code generation */
#include OATPP_CODEGEN_END(ApiClient)

#endif // MyApiTestClient_hpp
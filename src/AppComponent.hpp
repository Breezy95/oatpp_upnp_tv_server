
#ifndef AppComponent_hpp
#define AppComponent_hpp


#include "Utils.hpp"

#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp-ssdp/SimpleSsdpUdpStreamProvider.hpp"
#include "oatpp-ssdp/SsdpStreamHandler.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include <DeviceDescriptorComponent.hpp>

/**
 *  Class which creates and holds Application components and registers components in oatpp::base::Environment
 *  Order of components initialization is from top to bottom
 */
class AppComponent {
public:
   DeviceDescriptorComponent deviceComponent;
 

  /**
   *  Create ConnectionProvider component which listens on the port
   */
  


  /**
   *  Create Router component
   */

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, serverConnectionProvider)([] {
    return oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});
  }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, httpRouter)([] {
    return oatpp::web::server::HttpRouter::createShared();
  }());

  
  /**
   *  Create ConnectionHandler component which uses Router component to route requests
   */
OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, serverConnectionHandler)([] {
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router); // get Router component
    return oatpp::web::server::HttpConnectionHandler::createShared(router);
  }());

  /**
   *  Create ObjectMapper component to serialize/deserialize DTOs in Controller's API
   */
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)([] {
    auto serializerConfig = oatpp::parser::json::mapping::Serializer::Config::createShared();
    auto deserializerConfig = oatpp::parser::json::mapping::Deserializer::Config::createShared();
    deserializerConfig->allowUnknownFields = false;
    serializerConfig->useBeautifier = true;
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared(serializerConfig, deserializerConfig);
    return objectMapper;
  }());
  
OATPP_CREATE_COMPONENT(std::shared_ptr<StaticFilesManager>, staticFilesManager)([] {
    return std::make_shared<StaticFilesManager>(EXAMPLE_MEDIA_FOLDER /* path to '<this-repo>/Media-Stream/video' folder. Put full, absolute path here */) ;
  }());
 
  /*
   OATPP_CREATE_COMPONENT(std::shared_ptr<Database>, database)([] {
    return std::make_shared<Database>();
  }());
*/

// SSDP SECTION 
  
 /**
   * We can reuse the HttpRouter for SSDP since both SSDP and HTTP use the same Header structure
   */

   OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::ssdp::SimpleSsdpUdpStreamProvider>, ssdpConnectionProvider)("ssdpConnectionProvider", [] {
    return oatpp::ssdp::SimpleSsdpUdpStreamProvider::createShared();
  }());
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, ssdpRouter)("ssdpRouter", [] {
    return oatpp::web::server::HttpRouter::createShared();
  }());


/**
   *  Create SsdpStreamHandler component which uses Router component to route requests.
   *  It looks like a normal ConnectionHandler but is specialized on SsdpStreams and returns something conceptually very different
   */
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::ssdp::SsdpStreamHandler>, ssdpStreamHandler)("ssdpStreamHandler", [] {
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router, "ssdpRouter"); // get Router component
    return oatpp::ssdp::SsdpStreamHandler::createShared(router);
  }());

};


#endif /* AppComponent_hpp */
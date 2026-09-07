
#include "./controller/upnpDevice.hpp"
#include "./AppComponent.hpp"

#include "oatpp/network/Server.hpp"
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include "controller/UpnpClient.hpp"
#include <csignal>
#include <iostream>
#include <libgupnp-dlna/gupnp-dlna-profile-guesser.h>

/**
 *  run() method.
 *  1) set Environment components.
 *  2) add ApiController's endpoints to router
 *  3) run server
 */
void run() {
  auto upnp = std::make_shared<UpnpClient>(8001);
  auto devices = std::make_shared<deviceStore::DeviceStore>();
  AppComponent components;
  

  auto router = components.httpRouter.getObject();
  OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);
  router->addController(upnpController::createShared(objectMapper, upnp, devices));

  OATPP_LOGI("Devices", "Known devices stored in %s", devices->path().c_str());
  

  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);
  

  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);
  

  oatpp::network::Server server(connectionProvider, connectionHandler);
  
  OATPP_LOGI("Server", "Running on port %s...", 
             connectionProvider->getProperty("port").toString()->c_str());
  


    server.run();
 
}


int main(int argc, const char * argv[]) {
  
  oatpp::base::Environment::init();

    signal(SIGTERM, [](int sig) {
        OATPP_LOGI("Server", "Received SIGTERM. Shutting down...");
        exit(0);
    });
    
    signal(SIGINT, [](int sig) {
        OATPP_LOGI("Server", "Received SIGINT. Shutting down...");
        exit(0);
    });
    
    try {
        run();
    } catch (const std::exception& e) {
        OATPP_LOGE("Server", "Error: %s", e.what());
    }
    


    std::cout << "\nEnvironment:\n";
    std::cout << "objectsCount = " << oatpp::base::Environment::getObjectsCount() << "\n";
    std::cout << "objectsCreated = " << oatpp::base::Environment::getObjectsCreated() << "\n\n";
    
    oatpp::base::Environment::destroy();
    return 0;
}
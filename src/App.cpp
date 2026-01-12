
#include "./controller/upnpDevice.hpp"
#include "./AppComponent.hpp"

#include "oatpp/network/Server.hpp"
#include "upnp.h"
#include "upnpdebug.h"
#include "upnptools.h"
#include <iostream>
#include <libgupnp-dlna/gupnp-dlna-profile-guesser.h>

/**
 *  run() method.
 *  1) set Environment components.
 *  2) add ApiController's endpoints to router
 *  3) run server
 */
void run() {
  int upnpError =  UpnpInit2(nullptr, 8001);
  if (upnpError != UPNP_E_SUCCESS) {
    OATPP_LOGE("UPnP", "Failed to initialize UPnP library. Error: %d", upnpError);
    return;
  }

  
  AppComponent components;
  

  auto router = components.httpRouter.getObject();
  router->addController(upnpController::createShared());
  

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
    

    UpnpFinish();
    

    std::cout << "\nEnvironment:\n";
    std::cout << "objectsCount = " << oatpp::base::Environment::getObjectsCount() << "\n";
    std::cout << "objectsCreated = " << oatpp::base::Environment::getObjectsCreated() << "\n\n";
    
    oatpp::base::Environment::destroy();
    return 0;
}
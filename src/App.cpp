
#include "./controller/upnpDevice.hpp"
#include "./AppComponent.hpp"

#include "oatpp/network/Server.hpp"
#include "upnp.h"
#include <iostream>

/**
 *  run() method.
 *  1) set Environment components.
 *  2) add ApiController's endpoints to router
 *  3) run server
 */
void run() {
  
  AppComponent components; // Create scope Environment components
  auto router = components.httpRouter.getObject();

  router->addController(upnpController::createShared());
  
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

  /* Get connection provider component */
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);

  /* create server */
   oatpp::network::Server server(connectionProvider, connectionHandler);


  OATPP_LOGI("Server", "Running on port %s...", connectionProvider->getProperty("port").toString()->c_str());
  //UpnpInit2("192.168.0.212",8001);
  server.run();

  
}

/**
 *  main
 */
int main(int argc, const char * argv[]) {
  
  oatpp::base::Environment::init();

  run();
  
  /* Print how much objects were created during app running, and what have left-probably leaked */
  /* Disable object counting for release builds using '-D OATPP_DISABLE_ENV_OBJECT_COUNTERS' flag for better performance */
  std::cout << "\nEnvironment:\n";
  std::cout << "objectsCount = " << oatpp::base::Environment::getObjectsCount() << "\n";
  std::cout << "objectsCreated = " << oatpp::base::Environment::getObjectsCreated() << "\n\n";
  UpnpFinish();
  oatpp::base::Environment::destroy();
  
  return 0;
}
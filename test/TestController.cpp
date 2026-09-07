#include "TestController.hpp"

#include "controller/upnpDevice.hpp"
#include "controller/UpnpClient.hpp"
#include "app/MyApiTestClient.hpp"
#include "app/TestComponent.hpp"

#include "oatpp/web/client/HttpRequestExecutor.hpp"

#include "oatpp-test/web/ClientServerTestRunner.hpp"
#include <dto/DTOs.hpp>

void MyControllerTest::onRun() {

  /* Register test components */
  TestComponent component;

  /* Create client-server test runner */
  oatpp::test::web::ClientServerTestRunner runner;

   
    /* Get object mapper component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);

    /* Add upnp endpoints to the router of the test server */
    auto upnp = std::make_shared<UpnpClient>();
    runner.addController(std::make_shared<upnpController>(objectMapper, upnp));

  /* Run test */
  runner.run([this, &runner] {

    /* Get client connection provider for Api Client */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ClientConnectionProvider>, clientConnectionProvider);

 /* Get object mapper component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);
  

    /* Create http request executor for Api Client */
    auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(clientConnectionProvider);

    /* Create Test API client */
    auto client = MyApiTestClient::createShared(requestExecutor, objectMapper);

    /* Call server API */
    /* Call root endpoint of MyController */
    auto response = client->getHelloWorld();

    /* Assert that server responds with 200 */
    OATPP_ASSERT(response)
    OATPP_ASSERT(response->getStatusCode() == 200)

    /* Read response body as MessageDto */
    auto message = response->readBodyToDto<oatpp::Object<MessageDto>>(objectMapper.get());

    /* Assert that received message is as expected */
    OATPP_ASSERT(message);
    OATPP_ASSERT(message->statusCode == 200);
    OATPP_ASSERT(message->message == "Hello World");

  }, std::chrono::minutes(10) /* test timeout */);

  //Test getting an scpd from tv

  /* wait all server threads finished */
  std::this_thread::sleep_for(std::chrono::seconds(1));

}
#include "TestController.hpp"

#include "controller/upnpDevice.hpp"
#include "controller/UpnpClient.hpp"
#include "app/MyApiTestClient.hpp"
#include "app/TestComponent.hpp"

#include "oatpp/web/client/HttpRequestExecutor.hpp"

#include "oatpp-test/web/ClientServerTestRunner.hpp"
#include <dto/DTOs.hpp>

namespace {

void testDlnaFileDetectionAndHeaders() {
 OATPP_ASSERT(detectAudioKind("media/audio/song.mp3") == AudioKind::MP3);
 OATPP_ASSERT(detectAudioKind("media/audio/track.wav") == AudioKind::LPCM);
 OATPP_ASSERT(detectAudioKind("media/video/movie.mp4") == AudioKind::UNKNOWN);

 const auto mp3Headers = pickAudioDlnaHeaders(AudioKind::MP3, 44100, 2);
 OATPP_ASSERT(mp3Headers.contentType == "audio/mpeg");
 OATPP_ASSERT(mp3Headers.contentFeatures.find("DLNA.ORG_PN=MP3") != std::string::npos);
 OATPP_ASSERT(mp3Headers.contentFeatures.find("DLNA.ORG_FLAGS=ED100000000000000000000000000000") != std::string::npos);

 const auto pcmHeaders = pickAudioDlnaHeaders(AudioKind::LPCM, 44100, 2);
 OATPP_ASSERT(pcmHeaders.contentType == "audio/L16;rate=44100;channels=2");
 OATPP_ASSERT(pcmHeaders.contentFeatures.find("DLNA.ORG_PN=LPCM") != std::string::npos);
 OATPP_ASSERT(pcmHeaders.contentFeatures.find("DLNA.ORG_FLAGS=ED100000000000000000000000000000") != std::string::npos);

 const auto didl = generateDidlLite("media/audio/song.mp3", "http://127.0.0.1:8000/media/audio/song.mp3");
 OATPP_ASSERT(didl.find("<dc:title>song</dc:title>") != std::string::npos);
 OATPP_ASSERT(didl.find("http://127.0.0.1:8000/media/audio/song.mp3") != std::string::npos);
 OATPP_ASSERT(didl.find("sampleFrequency=\"44100\"") != std::string::npos);
 OATPP_ASSERT(didl.find("nrAudioChannels=\"2\"") != std::string::npos);
 OATPP_ASSERT(didl.find("object.item.audioItem") != std::string::npos);
}

void testUpnpClientCallbackParsing() {
 const auto discovered = UpnpClient::parseDiscoveredDevice(
     "uuid:demo-device",
     "http://192.168.0.42:8000/description.xml",
     "urn:schemas-upnp-org:device:MediaRenderer:1",
     "urn:schemas-upnp-org:service:AVTransport:1");

 OATPP_ASSERT(discovered.deviceId == "uuid:demo-device");
 OATPP_ASSERT(discovered.locationUrl == "http://192.168.0.42:8000/description.xml");
 OATPP_ASSERT(discovered.ipAddr == "192.168.0.42");
 OATPP_ASSERT(discovered.port == 8000);
 OATPP_ASSERT(discovered.deviceType == "urn:schemas-upnp-org:device:MediaRenderer:1");
 OATPP_ASSERT(discovered.serviceType == "urn:schemas-upnp-org:service:AVTransport:1");

 UpnpClient client(0, false);
 OATPP_ASSERT(UpnpClient::callback(UPNP_DISCOVERY_SEARCH_TIMEOUT, nullptr, &client) == 0);
 OATPP_ASSERT(UpnpClient::callback(UPNP_DISCOVERY_SEARCH_RESULT, nullptr, &client) == 0);
 OATPP_ASSERT(client.devices().empty());
}

} // namespace

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

   auto helloResponse = client->getHelloWorld();
   OATPP_ASSERT(helloResponse);
   OATPP_ASSERT(helloResponse->getStatusCode() == 200);

   auto helloBody = helloResponse->readBodyToDto<oatpp::Object<MessageDto>>(objectMapper.get());
   OATPP_ASSERT(helloBody);
   OATPP_ASSERT(helloBody->statusCode == 200);
   OATPP_ASSERT(helloBody->message == "Hello World");

   auto deviceListResponse = client->getDeviceList();
   OATPP_ASSERT(deviceListResponse);
   OATPP_ASSERT(deviceListResponse->getStatusCode() == 200);

   auto searchRequest = UpnpSearchRequest::createShared();
   searchRequest->searchType = "ssdp:all";
   searchRequest->mx = 1;
   auto searchResponse = client->searchDevices(searchRequest);
   OATPP_ASSERT(searchResponse);
   OATPP_ASSERT(searchResponse->getStatusCode() == 200);

 }, std::chrono::minutes(10) /* test timeout */);

 testDlnaFileDetectionAndHeaders();
 testUpnpClientCallbackParsing();

 //Test getting an scpd from tv

 /* wait all server threads finished */
 std::this_thread::sleep_for(std::chrono::seconds(1));

}
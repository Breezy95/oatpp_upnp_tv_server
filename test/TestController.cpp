#include "TestController.hpp"

#include "controller/upnpDevice.hpp"
#include "DeviceStore.hpp"
#include <cstdio>
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

 const auto mp4VideoHeaders = pickVideoDlnaHeaders("media/video/movie.mp4");
 OATPP_ASSERT(mp4VideoHeaders.contentType == "video/mp4");
 OATPP_ASSERT(mp4VideoHeaders.contentFeatures.find("DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC") != std::string::npos);

 const auto didl = generateDidlLite("media/audio/song.mp3", "http://127.0.0.1:8000/media/audio/song.mp3");
 OATPP_ASSERT(didl.find("<dc:title>song</dc:title>") != std::string::npos);
 OATPP_ASSERT(didl.find("http://127.0.0.1:8000/media/audio/song.mp3") != std::string::npos);
 OATPP_ASSERT(didl.find("sampleFrequency=\"44100\"") != std::string::npos);
 OATPP_ASSERT(didl.find("nrAudioChannels=\"2\"") != std::string::npos);
 OATPP_ASSERT(didl.find("object.item.audioItem") != std::string::npos);
}

void testLiveStreamMetadata() {
 const auto desktopStream = generateStreamDidlLite("Desktop Capture", "http://192.168.0.212:8080/desktop.mjpeg", "video", "video/mp4");
 OATPP_ASSERT(desktopStream.find("object.item.videoItem") != std::string::npos);
 OATPP_ASSERT(desktopStream.find("Desktop Capture") != std::string::npos);
 OATPP_ASSERT(desktopStream.find("http://192.168.0.212:8080/desktop.mjpeg") != std::string::npos);

 const auto vlcStream = generateStreamDidlLite("Laptop Camera", "rtsp://192.168.0.50:8554/live", "video");
 OATPP_ASSERT(vlcStream.find("object.item.videoItem") != std::string::npos);
 OATPP_ASSERT(vlcStream.find("rtsp://192.168.0.50:8554/live") != std::string::npos);

 const auto audioStream = generateStreamDidlLite("Pi Audio Feed", "http://192.168.0.42:5001/audio", "audio");
 OATPP_ASSERT(audioStream.find("object.item.audioItem") != std::string::npos);
 OATPP_ASSERT(audioStream.find("http://192.168.0.42:5001/audio") != std::string::npos);
}

void testContentDirectoryContract() {
 const auto profile = contentDirectoryProfileXml();
 OATPP_ASSERT(profile.find("<name>Browse</name>") != std::string::npos);
 OATPP_ASSERT(profile.find("<name>GetSearchCapabilities</name>") != std::string::npos);
 OATPP_ASSERT(profile.find("<name>GetSortCapabilities</name>") != std::string::npos);
 OATPP_ASSERT(profile.find("BrowseDirectChildren") != std::string::npos);

 const auto browse = buildContentDirectoryBrowseResponse("0", "Desktop Stream", "http://192.168.0.212:8000/stream/live/desktop.m3u8", "video");
 OATPP_ASSERT(browse.find("BrowseResponse") != std::string::npos);
 OATPP_ASSERT(browse.find("<Result>") != std::string::npos);
 OATPP_ASSERT(browse.find("DIDL-Lite") != std::string::npos);
 OATPP_ASSERT(browse.find("object.item.videoItem") != std::string::npos);
 OATPP_ASSERT(browse.find("Desktop Stream") != std::string::npos);
 OATPP_ASSERT(browse.find("NumberReturned>1</NumberReturned>") != std::string::npos);
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
 OATPP_ASSERT(discovered.serviceTypes.size() == 1);

 const auto contentDirectory = UpnpClient::parseDiscoveredDevice(
     "uuid:demo-content", "http://192.168.0.42:8000/description.xml",
     "urn:schemas-upnp-org:device:MediaServer:1",
     "urn:schemas-upnp-org:service:ContentDirectory:1");
 OATPP_ASSERT(contentDirectory.serviceType == "urn:schemas-upnp-org:service:ContentDirectory:1");
 OATPP_ASSERT(contentDirectory.serviceTypes.front() == "urn:schemas-upnp-org:service:ContentDirectory:1");

 const auto audioDidl = generateDidlLite("media/audio/song.mp3", "http://192.168.0.212:8000/media/audio/song.mp3");
 OATPP_ASSERT(audioDidl.find("object.item.audioItem") != std::string::npos);
 OATPP_ASSERT(audioDidl.find("http://192.168.0.212:8000/media/audio/song.mp3") != std::string::npos);

 const auto videoDidl = generateVideoDidlLite("media/video/movie.mp4", "http://192.168.0.212:8000/media/video/movie.mp4");
 OATPP_ASSERT(videoDidl.find("object.item.videoItem") != std::string::npos);
 OATPP_ASSERT(videoDidl.find("http://192.168.0.212:8000/media/video/movie.mp4") != std::string::npos);

 const auto browse = buildContentDirectoryBrowseResponse("0", "Desktop Stream", "http://192.168.0.212:8000/stream/live/desktop.m3u8", "video");
 OATPP_ASSERT(browse.find("BrowseResponse") != std::string::npos);
 OATPP_ASSERT(browse.find("Desktop Stream") != std::string::npos);
 OATPP_ASSERT(browse.find("http://192.168.0.212:8000/stream/live/desktop.m3u8") != std::string::npos);

 UpnpClient client(0, false);
 OATPP_ASSERT(UpnpClient::callback(UPNP_DISCOVERY_SEARCH_TIMEOUT, nullptr, &client) == 0);
 OATPP_ASSERT(UpnpClient::callback(UPNP_DISCOVERY_SEARCH_RESULT, nullptr, &client) == 0);
 OATPP_ASSERT(client.devices().empty());
}

void testMediaLibraryListing() {
 OATPP_ASSERT(mediaLibrary::isSafeFileName("movie.mp4"));
 OATPP_ASSERT(!mediaLibrary::isSafeFileName(".."));
 OATPP_ASSERT(!mediaLibrary::isSafeFileName("../secret.mp4"));
 OATPP_ASSERT(!mediaLibrary::isSafeFileName(""));

 OATPP_ASSERT(mediaLibrary::hasVideoExtension("Movie.MKV"));
 OATPP_ASSERT(mediaLibrary::hasVideoExtension("clip.mp4"));
 OATPP_ASSERT(!mediaLibrary::hasVideoExtension("song.mp3"));

 OATPP_ASSERT(mediaLibrary::mediaFilePath("video", "movie.mp4") == mediaLibrary::mediaDir("video") + "/movie.mp4");
 OATPP_ASSERT(mediaLibrary::mediaFilePath("video", "../../etc/passwd").empty());

 OATPP_ASSERT(mediaLibrary::urlEncode("my movie&1.mp4") == "my%20movie%261.mp4");
 OATPP_ASSERT(mediaLibrary::urlDecode("my%20movie%261.mp4") == "my movie&1.mp4");
 OATPP_ASSERT(mediaLibrary::urlDecode("plain.mp4") == "plain.mp4");
 OATPP_ASSERT(mediaLibrary::urlDecode("trailing%20") == "trailing ");
 OATPP_ASSERT(mediaLibrary::urlDecode("truncated%2") == "truncated%2");
 OATPP_ASSERT(mediaLibrary::urlDecode("my+movie.mp4") == "my+movie.mp4");
 OATPP_ASSERT(mediaLibrary::mediaFilePath("video", mediaLibrary::urlDecode("..%2f..%2fetc%2fpasswd")).empty());
 OATPP_ASSERT(mediaLibrary::formatSize(5 * 1024 * 1024) == "5.0 MB");
 OATPP_ASSERT(mediaLibrary::formatSize(2048) == "2.0 KB");
 OATPP_ASSERT(mediaLibrary::formatSize(12) == "12 B");
 OATPP_ASSERT(mediaLibrary::htmlEscape("<b>&\"") == "&lt;b&gt;&amp;&quot;");

 std::vector<mediaLibrary::MediaFile> files;
 mediaLibrary::MediaFile file;
 file.name = "my movie.mp4";
 file.title = "my movie";
 file.size = 5 * 1024 * 1024;
 files.push_back(file);

 const auto page = mediaLibrary::renderBrowsePage(files);
 OATPP_ASSERT(page.find("/media/video/my%20movie.mp4") != std::string::npos);
 OATPP_ASSERT(page.find("my movie") != std::string::npos);
 OATPP_ASSERT(page.find("5.0 MB") != std::string::npos);
 OATPP_ASSERT(page.find("/upnp/sendMedia") != std::string::npos);

 OATPP_ASSERT(mediaLibrary::renderBrowsePage({}).find("No videos found") != std::string::npos);
}

void testDeviceDescriptionParsing() {
 const std::string xml =
   "<root xmlns=\"urn:schemas-upnp-org:device-1-0\"><device>"
   "<deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
   "<friendlyName>Living room TV</friendlyName>"
   "<manufacturer>Acme</manufacturer>"
   "<modelName>TV-3000</modelName>"
   "<UDN>uuid:1234</UDN>"
   "<serviceList>"
   "<service><serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>"
   "<serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>"
   "<controlURL>/upnp/control/RenderingControl1</controlURL></service>"
   "<service><serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
   "<serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>"
   "<controlURL>/upnp/control/AVTransport1</controlURL></service>"
   "</serviceList></device></root>";

 const auto description = parseDeviceDescription(xml, "http://192.168.0.100:52235/dmr/");
 OATPP_ASSERT(description.friendlyName == "Living room TV");
 OATPP_ASSERT(description.manufacturer == "Acme");
 OATPP_ASSERT(description.modelName == "TV-3000");
 OATPP_ASSERT(description.udn == "uuid:1234");
 OATPP_ASSERT(description.serviceType == "urn:schemas-upnp-org:service:AVTransport:1");
 OATPP_ASSERT(description.serviceId == "urn:upnp-org:serviceId:AVTransport");
 OATPP_ASSERT(description.controlUrl == "http://192.168.0.100:52235/upnp/control/AVTransport1");

 OATPP_ASSERT(parseDeviceDescription("", "http://host/").friendlyName.empty());
 OATPP_ASSERT(parseDeviceDescription("not xml", "http://host/").friendlyName.empty());

 OATPP_ASSERT(resolveUrl("http://host:80/dmr/", "control") == "http://host:80/dmr/control");
 OATPP_ASSERT(resolveUrl("http://host:80/dmr/desc.xml", "/control") == "http://host:80/control");
 OATPP_ASSERT(resolveUrl("http://host/", "http://other/control") == "http://other/control");
 OATPP_ASSERT(resolveUrl("", "control") == "control");
}

void testDeviceStorePersistence() {
 const std::string path = "/tmp/upnptvserver-devices-test.json";
 std::remove(path.c_str());

 deviceStore::KnownDevice discovered;
 discovered.deviceId = "uuid:1234";
 discovered.friendlyName = "Living room TV";
 discovered.locationUrl = "http://192.168.0.100:52235/dmr/desc.xml";
 discovered.controlUrl = "http://192.168.0.100:52235/upnp/control/AVTransport1";
 discovered.ipAddr = "192.168.0.100";
 discovered.port = 52235;
 discovered.lastSeen = 1700000000;

 {
   deviceStore::DeviceStore store(path);
   OATPP_ASSERT(store.list().empty());
   OATPP_ASSERT(store.upsert(discovered));

   deviceStore::KnownDevice missingId;
   OATPP_ASSERT(!store.upsert(missingId));

   deviceStore::KnownDevice update;
   update.deviceId = discovered.deviceId;
   update.modelName = "TV-3000";
   update.lastSeen = 1700000500;
   OATPP_ASSERT(store.upsert(update));

   const auto devices = store.list();
   OATPP_ASSERT(devices.size() == 1);
   OATPP_ASSERT(devices[0].friendlyName == "Living room TV");
   OATPP_ASSERT(devices[0].modelName == "TV-3000");
   OATPP_ASSERT(devices[0].lastSeen == 1700000500);
 }

 {
   deviceStore::DeviceStore reopened(path);
   const auto devices = reopened.list();
   OATPP_ASSERT(devices.size() == 1);
   OATPP_ASSERT(devices[0].deviceId == "uuid:1234");
   OATPP_ASSERT(devices[0].controlUrl == "http://192.168.0.100:52235/upnp/control/AVTransport1");
   OATPP_ASSERT(devices[0].port == 52235);

   OATPP_ASSERT(!reopened.remove("uuid:unknown"));
   OATPP_ASSERT(reopened.remove("uuid:1234"));
   OATPP_ASSERT(reopened.list().empty());
 }

 OATPP_ASSERT(deviceStore::DeviceStore(path).list().empty());
 std::remove(path.c_str());

 OATPP_ASSERT(deviceStore::deserializeDevices("").empty());
 OATPP_ASSERT(deviceStore::deserializeDevices("{ not json").empty());
 OATPP_ASSERT(deviceStore::deserializeDevices(deviceStore::serializeDevices({discovered})).size() == 1);

 deviceStore::KnownDevice empty;
 empty.deviceId = "uuid:1234";
 const auto merged = deviceStore::mergeDevice(discovered, empty);
 OATPP_ASSERT(merged.friendlyName == "Living room TV");
 OATPP_ASSERT(merged.port == 52235);
 OATPP_ASSERT(merged.lastSeen == 1700000000);
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
   const char* deviceStorePath = "/tmp/upnptvserver-devices-endpoint-test.json";
   std::remove(deviceStorePath);
   auto devices = std::make_shared<deviceStore::DeviceStore>(deviceStorePath);
   runner.addController(std::make_shared<upnpController>(objectMapper, upnp, devices));

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

   auto libraryResponse = client->getVideoLibrary();
   OATPP_ASSERT(libraryResponse);
   OATPP_ASSERT(libraryResponse->getStatusCode() == 200);

   auto libraryBody = libraryResponse->readBodyToDto<oatpp::Object<MediaLibraryDTO>>(objectMapper.get());
   OATPP_ASSERT(libraryBody);
   OATPP_ASSERT(libraryBody->directory);
   OATPP_ASSERT(std::string(libraryBody->directory->c_str()) == mediaLibrary::mediaDir("video"));

   auto devicesResponse = client->getKnownDevices();
   OATPP_ASSERT(devicesResponse);
   OATPP_ASSERT(devicesResponse->getStatusCode() == 200);

   auto devicesBody = devicesResponse->readBodyToDto<oatpp::Object<KnownDeviceListDTO>>(objectMapper.get());
   OATPP_ASSERT(devicesBody);
   OATPP_ASSERT(devicesBody->devices);
   OATPP_ASSERT(devicesBody->devices->empty());

   auto forgetResponse = client->forgetDevice("uuid:unknown");
   OATPP_ASSERT(forgetResponse);
   OATPP_ASSERT(forgetResponse->getStatusCode() == 404);

   auto browseResponse = client->getBrowsePage();
   OATPP_ASSERT(browseResponse);
   OATPP_ASSERT(browseResponse->getStatusCode() == 200);
   OATPP_ASSERT(std::string(browseResponse->readBodyToString()->c_str()).find("Pick a video") != std::string::npos);

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
 testLiveStreamMetadata();
 testContentDirectoryContract();
 testUpnpClientCallbackParsing();
 testMediaLibraryListing();
 testDeviceDescriptionParsing();
 testDeviceStorePersistence();
 const auto contentDirectory = buildContentDirectoryBrowseResponse("0", "Desktop Stream", "http://127.0.0.1:8000/stream/live/desktop.m3u8", "video");
 OATPP_ASSERT(contentDirectory.find("DIDL-Lite") != std::string::npos);
 OATPP_ASSERT(contentDirectory.find("object.item.videoItem") != std::string::npos);

 //Test getting an scpd from tv

 /* wait all server threads finished */
 std::this_thread::sleep_for(std::chrono::seconds(1));

}
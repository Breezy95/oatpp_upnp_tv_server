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
 OATPP_ASSERT(mediaLibrary::mediaFilePath("video", mediaLibrary::urlDecode("..%2f..%2fetc%2fpasswd")).empty());
 OATPP_ASSERT(mediaLibrary::formatSize(5 * 1024 * 1024) == "5.0 MB");
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

   auto libraryResponse = client->getVideoLibrary();
   OATPP_ASSERT(libraryResponse);
   OATPP_ASSERT(libraryResponse->getStatusCode() == 200);

   auto libraryBody = libraryResponse->readBodyToDto<oatpp::Object<MediaLibraryDTO>>(objectMapper.get());
   OATPP_ASSERT(libraryBody);
   OATPP_ASSERT(libraryBody->directory);
   OATPP_ASSERT(std::string(libraryBody->directory->c_str()) == mediaLibrary::mediaDir("video"));

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
 const auto contentDirectory = buildContentDirectoryBrowseResponse("0", "Desktop Stream", "http://127.0.0.1:8000/stream/live/desktop.m3u8", "video");
 OATPP_ASSERT(contentDirectory.find("DIDL-Lite") != std::string::npos);
 OATPP_ASSERT(contentDirectory.find("object.item.videoItem") != std::string::npos);

 //Test getting an scpd from tv

 /* wait all server threads finished */
 std::this_thread::sleep_for(std::chrono::seconds(1));

}
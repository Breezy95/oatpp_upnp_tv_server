# oatpp UPnP TV Server

Lightweight UPnP/DLNA media server built with oatpp, intended to serve
media (audio/video) to remote UPnP/DLNA-capable clients (TVs, media players).

## Features

- Web UI at `http://<host>:8000/` where anyone on the network can pick a video
  from the local content directory and play it in the browser or send it to a TV.
- Control point discovery from the same page: scan the network for UPnP/DLNA
  renderers and remember them in a JSON store between restarts.
- Serves media files from the repository `media/` tree.
- Links a TV to arbitrary media resources by sending a `SetAVTransportURI` + `Play` action to a UPnP renderer.
- Supports live sources such as desktop capture, VLC-driven streams, or other HTTP/RTSP endpoints through the `/upnp/sendMedia` API.
- Basic UPnP device & service implementation under `src/controller`.
- Small C++ project using CMake for cross-platform builds.

## Prerequisites

- A C++17 toolchain (g++/clang++)
- CMake >= 3.16
- `oatpp` library and its development headers available to CMake

Note: Platform-specific OS packages for dependencies (if any) are not listed
here — use your distribution package manager or build `oatpp` from source.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release -j
```

This will produce the main executable(s) in the `build/` directory, for
example `upnptvserver-exe` and `upnptvserver-test`.

## Run

Run the server executable from the repository root or `build/` folder:

```bash
./build/upnptvserver-exe
```

Behavior, options and runtime configuration are implemented in `src/App.cpp`.

## Picking a video from the browser

Open `http://<server-host>:8000/` in a browser. The page lists every video file
found in the local content directory (`media/video` by default) and offers two
actions per file:

- **Play here** — streams the file in the browser player.
- **Send to TV** — calls `/upnp/sendMedia` so a UPnP/DLNA renderer plays the file.

The listing is also available as JSON from `GET /api/media/video`.

### Finding and remembering renderers

The **Renderers** panel of the same page drives the control point:

- **Scan network** (`POST /api/devices/scan`) sends an SSDP search, waits for the
  replies, downloads each device description to pick up its friendly name and
  AVTransport control URL, and stores the result.
- The dropdown lists the remembered devices (`GET /api/devices`); the selected
  one is used as the target of **Send to TV**.
- **Forget** removes a device from the store (`DELETE /api/devices/{deviceId}`).

Known devices are persisted as JSON in `devices.json`. Point the `DEVICE_STORE`
environment variable at another file to change the location:

```bash
DEVICE_STORE=/etc/upnptvserver/devices.json ./build/upnptvserver-exe
```

Set the `MEDIA_ROOT` environment variable to serve a different content directory
(it must contain `video/` and, optionally, `audio/` sub-directories):

```bash
MEDIA_ROOT=/srv/movies ./build/upnptvserver-exe
```

## Typical user flow

The same end-to-end flow — discover a renderer, pick some media, start
playback — can be followed either from the browser UI or purely through API
calls.

### 0. Start the server

```bash
cmake -S . -B build && cmake --build build -j
MEDIA_ROOT=/srv/movies DEVICE_STORE=./devices.json ./build/upnptvserver-exe
```

The server listens on port `8000`. Media must live under `$MEDIA_ROOT/video`
and, optionally, `$MEDIA_ROOT/audio`.

### From the UI

1. Open `http://<server-host>:8000/`. The page lists every video found in the
   content directory.
2. In the **Renderers** panel press **Scan network**. This sends an SSDP
   search, waits for replies, downloads each device description and stores the
   discovered renderers.
3. Select the target renderer in the dropdown.
4. For the media row you want, press either:
   - **Play here** to stream the file in the browser player, or
   - **Send to TV** to hand the file to the selected renderer.
5. Press **Forget** to drop a renderer you no longer care about.

### Through API calls

1. Discover and remember renderers:

   ```bash
   curl -X POST http://localhost:8000/api/devices/scan \
     -H 'Content-Type: application/json' \
     -d '{"searchType":"urn:schemas-upnp-org:device:MediaRenderer:1","mx":3}'
   ```

   The response contains the known devices, each with `deviceId`,
   `friendlyName`, `controlUrl`, `serviceType`, `serviceId`, `ipAddr`, `port`
   and `lastSeen`.

2. List the remembered renderers without rescanning:

   ```bash
   curl http://localhost:8000/api/devices
   ```

3. List the playable media:

   ```bash
   curl http://localhost:8000/api/media/video
   ```

4. Send a library file to the renderer, using the `controlUrl` from step 1 as
   `actionUrl`:

   ```bash
   curl -X POST http://localhost:8000/upnp/sendMedia \
     -H 'Content-Type: application/json' \
     -d '{"actionUrl":"http://192.168.0.42:52235/upnp/control/AVTransport1",
          "filePath":"BigBuckBunny.mp4",
          "mediaType":"video",
          "sourceType":"file",
          "title":"Big Buck Bunny"}'
   ```

   The server resolves the absolute media URL, generates the DIDL-Lite
   metadata and then sends `SetAVTransportURI` followed by `Play`. UPnP
   failures are reported as `422` responses carrying `upnpErrorCode`.

5. Send a live or external source instead of a library file:

   ```bash
   curl -X POST http://localhost:8000/upnp/sendMedia \
     -H 'Content-Type: application/json' \
     -d '{"actionUrl":"http://192.168.0.42:52235/upnp/control/AVTransport1",
          "sourceType":"stream",
          "streamUrl":"http://192.168.0.10:8080/desktop.ts",
          "mediaType":"video",
          "mimeType":"video/mpeg",
          "title":"Desktop"}'
   ```

6. Optionally inspect or control the renderer:
   - `POST /upnp/getProtocolInfo` to check what the renderer accepts before
     casting.
   - `POST /upnp/sendAction` for any other AVTransport/RenderingControl action
     such as `Pause`, `Stop` or `SetVolume`.
   - `GET /upnp/getVolume` to read the current volume.

7. Forget a renderer once you are done with it:

   ```bash
   curl -X DELETE http://localhost:8000/api/devices/<url-encoded-deviceId>
   ```

### What the renderer does in return

While playing, the TV talks back to this server: it fetches
`GET /description.xml`, then the referenced `GET /profiles/{profileName}`,
browses the content with `POST /upnp/services/content-directory/control` and
finally issues `HEAD`/`GET /media/video/{file}` requests with range and DLNA
headers to stream the file.

## Tests

To run tests (if built):

```bash
ctest --test-dir build --output-on-failure
```

Or run the built test binary directly:

```bash
./build/upnptvserver-test
```

## Project layout

- `src/` — application source code
- `media/` — example media files organized by `audio/` and `video/`
- `test/` — unit / integration tests
- `CMakeLists.txt` — build configuration

## Contributing

Contributions are welcome. Open an issue or submit a pull request with a
description of the change and any testing instructions.

## License

Please see the repository root for license information (if any).

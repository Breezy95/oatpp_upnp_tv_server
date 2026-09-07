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

# oatpp UPnP TV Server

Lightweight UPnP/DLNA media server built with oatpp, intended to serve
media (audio/video) to remote UPnP/DLNA-capable clients (TVs, media players).

## Features

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

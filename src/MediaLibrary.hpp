#ifndef MEDIA_LIBRARY_HPP
#define MEDIA_LIBRARY_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace mediaLibrary
{

struct MediaFile
{
    std::string name;      // file name, e.g. "movie.mp4"
    std::string title;     // name without extension
    std::uintmax_t size{}; // size in bytes
};

/**
 * Root of the local content directory.
 * Defaults to "media" and can be overridden with the MEDIA_ROOT env variable.
 */
inline std::string mediaRoot()
{
    const char *root = std::getenv("MEDIA_ROOT");
    if (root != nullptr && root[0] != '\0')
        return std::string(root);
    return std::string("media");
}

/**
 * Directory holding a given media kind ("video" or "audio").
 */
inline std::string mediaDir(const std::string &kind)
{
    return mediaRoot() + "/" + kind;
}

/**
 * Rejects empty names and any name that could escape the content directory.
 */
inline bool isSafeFileName(const std::string &name)
{
    if (name.empty() || name == "." || name == "..")
        return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
        return false;
    if (name.find('\0') != std::string::npos)
        return false;
    return true;
}

/**
 * Full path of a media file inside the content directory, or an empty string
 * when the requested name is not safe to serve.
 */
inline std::string mediaFilePath(const std::string &kind, const std::string &name)
{
    if (!isSafeFileName(name))
        return std::string();
    return mediaDir(kind) + "/" + name;
}

inline bool hasVideoExtension(const std::string &name)
{
    static const std::vector<std::string> extensions = {
        ".mp4", ".m4v", ".mkv", ".avi", ".mov", ".mpg", ".mpeg", ".webm", ".ts", ".wmv"};

    std::string lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    for (const auto &ext : extensions)
    {
        if (lowered.size() >= ext.size() &&
            lowered.compare(lowered.size() - ext.size(), ext.size(), ext) == 0)
            return true;
    }
    return false;
}

/**
 * Lists the playable video files of the content directory, sorted by name.
 * Returns an empty list when the directory does not exist.
 */
inline std::vector<MediaFile> listVideos()
{
    std::vector<MediaFile> files;
    std::error_code ec;
    const std::string dir = mediaDir("video");

    if (!std::filesystem::is_directory(dir, ec))
        return files;

    auto entries = std::filesystem::directory_iterator(dir, ec);
    if (ec)
        return files;

    for (const auto &entry : entries)
    {
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || entryEc)
            continue;

        const std::string name = entry.path().filename().string();
        if (!isSafeFileName(name) || !hasVideoExtension(name))
            continue;

        MediaFile file;
        file.name = name;
        file.title = entry.path().stem().string();
        std::error_code sizeEc;
        file.size = entry.file_size(sizeEc);
        if (sizeEc)
            file.size = 0;
        files.push_back(file);
    }

    std::sort(files.begin(), files.end(), [](const MediaFile &a, const MediaFile &b) {
        return a.name < b.name;
    });

    return files;
}

inline std::string htmlEscape(const std::string &value)
{
    std::string out;
    out.reserve(value.size());
    for (const char ch : value)
    {
        switch (ch)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

inline std::string urlEncode(const std::string &value)
{
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value)
    {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            out += static_cast<char>(ch);
        }
        else
        {
            out += '%';
            out += hex[(ch >> 4) & 0x0F];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}

/**
 * Decodes percent-escapes of a URL path segment. oatpp does not decode path
 * variables, so file names containing spaces or other escaped characters must
 * be decoded before they are looked up on disk.
 */
inline std::string urlDecode(const std::string &value)
{
    const auto hexValue = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };

    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            const int hi = hexValue(value[i + 1]);
            const int lo = hexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += value[i];
    }
    return out;
}

/**
 * Human readable file size, e.g. "1.4 MB". Sizes below one megabyte are
 * reported in kilobytes, and sizes below one kilobyte in bytes.
 */
inline std::string formatSize(std::uintmax_t bytes)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    if (bytes >= 1024ull * 1024ull * 1024ull)
        out << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    else if (bytes >= 1024ull * 1024ull)
        out << (bytes / (1024.0 * 1024.0)) << " MB";
    else if (bytes >= 1024ull)
        out << (bytes / 1024.0) << " KB";
    else
        out << bytes << " B";
    return out.str();
}

/**
 * Renders the "pick a video" page: every file of the local content directory is
 * listed, can be played in the browser and can be sent to a UPnP renderer.
 */
inline std::string renderBrowsePage(const std::vector<MediaFile> &files)
{
    std::ostringstream page;
    page << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Media library</title>
<style>
body { font-family: system-ui, sans-serif; margin: 0; padding: 1.5rem; background: #14161a; color: #f2f4f8; }
h1 { font-size: 1.4rem; margin-top: 0; }
ul { list-style: none; padding: 0; }
li { display: flex; align-items: center; gap: .75rem; padding: .6rem .8rem; margin-bottom: .4rem; background: #1e2128; border-radius: .5rem; }
li .name { flex: 1; word-break: break-all; }
li .size { color: #99a0ad; font-size: .85rem; }
button, a.button { background: #3b82f6; color: #fff; border: 0; border-radius: .35rem; padding: .4rem .8rem; cursor: pointer; text-decoration: none; font-size: .9rem; }
button.secondary { background: #4b5563; }
video { width: 100%; max-height: 60vh; background: #000; border-radius: .5rem; margin-bottom: 1rem; }
#status { min-height: 1.2rem; color: #99a0ad; font-size: .9rem; }
.empty { color: #99a0ad; }
section.devices { background: #1e2128; border-radius: .5rem; padding: .8rem; margin-bottom: 1rem; }
section.devices h2 { font-size: 1rem; margin: 0 0 .6rem; }
section.devices .row { display: flex; align-items: center; gap: .6rem; flex-wrap: wrap; }
select { background: #14161a; color: #f2f4f8; border: 1px solid #4b5563; border-radius: .35rem; padding: .4rem; flex: 1; min-width: 12rem; }
</style>
</head>
<body>
<h1>Pick a video</h1>
<section class="devices">
<h2>Renderers</h2>
<div class="row">
<select id="devices"><option value="">No known devices yet</option></select>
<button id="scan">Scan network</button>
<button id="forget" class="secondary">Forget</button>
</div>
<div id="deviceStatus"></div>
</section>
<video id="player" controls></video>
<div id="status"></div>
<ul>
)";

    if (files.empty())
    {
        page << "<li class=\"empty\">No videos found in " << htmlEscape(mediaDir("video"))
             << ". Drop files there and refresh.</li>\n";
    }

    for (const auto &file : files)
    {
        const std::string url = "/media/video/" + urlEncode(file.name);
        page << "<li>"
             << "<span class=\"name\">" << htmlEscape(file.title) << "</span>"
             << "<span class=\"size\">" << htmlEscape(formatSize(file.size)) << "</span>"
             << "<button data-url=\"" << htmlEscape(url) << "\" class=\"play\">Play here</button>"
             << "<button data-file=\"" << htmlEscape(file.name) << "\" class=\"secondary cast\">Send to TV</button>"
             << "</li>\n";
    }

    page << R"(</ul>
<script>
const player = document.getElementById('player');
const statusEl = document.getElementById('status');
const deviceSelect = document.getElementById('devices');
const deviceStatusEl = document.getElementById('deviceStatus');

function renderDevices(data) {
  const devices = (data && data.devices) || [];
  deviceSelect.innerHTML = '';
  if (!devices.length) {
    const option = document.createElement('option');
    option.value = '';
    option.textContent = 'No known devices yet';
    deviceSelect.appendChild(option);
  } else {
    devices.forEach(function (device) {
      const option = document.createElement('option');
      option.value = device.deviceId || '';
      option.textContent = (device.friendlyName || device.deviceId || 'Unknown device') +
        (device.ipAddr ? ' (' + device.ipAddr + ')' : '');
      option.dataset.controlUrl = device.controlUrl || '';
      option.dataset.serviceType = device.serviceType || '';
      option.dataset.serviceId = device.serviceId || '';
      deviceSelect.appendChild(option);
    });
  }
  deviceStatusEl.textContent = devices.length
    ? devices.length + ' device(s) remembered in ' + (data.storePath || 'the device store')
    : 'No devices remembered yet. Run a scan.';
}

function loadDevices() {
  fetch('/api/devices').then(function (res) { return res.json(); })
    .then(renderDevices)
    .catch(function (err) { deviceStatusEl.textContent = 'Failed to load devices: ' + err; });
}

document.getElementById('scan').addEventListener('click', function () {
  deviceStatusEl.textContent = 'Scanning...';
  fetch('/api/devices/scan', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ searchType: 'urn:schemas-upnp-org:device:MediaRenderer:1', mx: 3 })
  }).then(function (res) {
    return res.json().then(function (data) {
      if (res.ok) { renderDevices(data); }
      else { deviceStatusEl.textContent = data.message || 'Scan failed'; }
    });
  }).catch(function (err) { deviceStatusEl.textContent = 'Scan failed: ' + err; });
});

document.getElementById('forget').addEventListener('click', function () {
  const deviceId = deviceSelect.value;
  if (!deviceId) { return; }
  fetch('/api/devices/' + encodeURIComponent(deviceId), { method: 'DELETE' })
    .then(function (res) { return res.json(); })
    .then(function (data) {
      if (data && data.devices) { renderDevices(data); }
      else { deviceStatusEl.textContent = (data && data.message) || 'Failed to forget device'; loadDevices(); }
    })
    .catch(function (err) { deviceStatusEl.textContent = 'Failed to forget device: ' + err; });
});

loadDevices();
document.querySelectorAll('button.play').forEach(function (btn) {
  btn.addEventListener('click', function () {
    player.src = btn.dataset.url;
    player.play();
    statusEl.textContent = 'Playing in browser';
  });
});
document.querySelectorAll('button.cast').forEach(function (btn) {
  btn.addEventListener('click', function () {
    statusEl.textContent = 'Sending to TV...';
    const payload = { filePath: btn.dataset.file, mediaType: 'video', sourceType: 'file' };
    const selected = deviceSelect.selectedOptions[0];
    if (selected && selected.dataset.controlUrl) {
      payload.actionUrl = selected.dataset.controlUrl;
      if (selected.dataset.serviceType) { payload.serviceType = selected.dataset.serviceType; }
      if (selected.dataset.serviceId) { payload.serviceId = selected.dataset.serviceId; }
    }
    fetch('/upnp/sendMedia', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    }).then(function (res) {
      return res.json().catch(function () { return {}; }).then(function (data) {
        statusEl.textContent = res.ok ? (data.message || 'Sent to TV') : (data.message || 'Failed to send to TV');
      });
    }).catch(function (err) {
      statusEl.textContent = 'Failed to send to TV: ' + err;
    });
  });
});
</script>
</body>
</html>
)";

    return page.str();
}

} // namespace mediaLibrary

#endif // MEDIA_LIBRARY_HPP

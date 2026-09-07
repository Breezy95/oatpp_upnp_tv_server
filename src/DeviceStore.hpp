#ifndef DEVICE_STORE_HPP
#define DEVICE_STORE_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace deviceStore
{

/**
 * A UPnP device (typically a media renderer) the control point has discovered
 * and that is remembered across restarts.
 */
struct KnownDevice
{
    std::string deviceId;
    std::string friendlyName;
    std::string deviceType;
    std::string locationUrl;
    std::string controlUrl;
    std::string serviceType;
    std::string serviceId;
    std::string manufacturer;
    std::string modelName;
    std::string ipAddr;
    uint16_t port = 0;
    std::int64_t lastSeen = 0; // unix time, seconds
};

/**
 * Path of the JSON file backing the store. Defaults to "devices.json" and can
 * be overridden with the DEVICE_STORE environment variable.
 */
std::string defaultStorePath();

/**
 * Merges a freshly discovered device into an already known one: non-empty
 * incoming fields win, previously known fields are kept otherwise.
 */
KnownDevice mergeDevice(const KnownDevice &known, const KnownDevice &discovered);

std::string serializeDevices(const std::vector<KnownDevice> &devices);
std::vector<KnownDevice> deserializeDevices(const std::string &json);

/**
 * Thread-safe, file-backed collection of known devices.
 */
class DeviceStore
{
public:
    explicit DeviceStore(std::string path = defaultStorePath());

    const std::string &path() const noexcept { return m_path; }

    /** Loads the devices from disk. Missing files are not an error. */
    bool load();

    /** Writes the current devices to disk. */
    bool save() const;

    std::vector<KnownDevice> list() const;

    /**
     * Adds or updates a device and persists the store.
     * Devices without an id are ignored.
     */
    bool upsert(const KnownDevice &device);

    /** Removes a device by id and persists the store. Returns false if unknown. */
    bool remove(const std::string &deviceId);

private:
    bool saveLocked() const;

    std::string m_path;
    mutable std::mutex m_mutex;
    std::vector<KnownDevice> m_devices;
};

} // namespace deviceStore

#endif // DEVICE_STORE_HPP

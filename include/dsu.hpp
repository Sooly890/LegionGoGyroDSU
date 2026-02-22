#pragma once

#include <asio.hpp>
#include <asio/ip/udp.hpp>
#include <cstdint>
#include <unordered_map>

namespace dsu
{

enum class EventType : uint32_t
{
    ProtocolVersionInfo = 0x100000,
    InfoController = 0x100001,
    ActualControllerData = 0x100002,
    InfoRumble = 0x110001,
    SetRumble = 0x110002,
};

#pragma pack(push, 1)
struct Header
{
    // DSUS (me), DSUC (cemu)
    uint32_t magic;
    // only 1001 exists
    uint16_t version;
    // no header
    uint16_t size;
    uint32_t crc32;
    uint32_t server_or_client_id;
    // not part of header technically, part of payload
    EventType event;
};
#pragma pack(pop)

namespace outgoing
{
#pragma pack(push, 1)
struct ProtocolVersionInfo
{
    uint16_t maximal_version;
};
#pragma pack(pop)
enum class SlotState : uint8_t
{
    Disconnected = 0,
    Reserved = 1, // idk what this does
    Connected = 2
};

enum class DeviceModel : uint8_t
{
    NotApplicable = 0,
    NoGyro = 1,
    AllGyro = 2,
    ExistsButDontUse = 3
};

enum class ConnectionType : uint8_t
{
    NotApplicable = 0,
    Usb = 1,
    Bluetooth = 2
};

enum class BatteryStatus : uint8_t
{
    NotApplicable = 0,
    Dying = 0x01,
    Low = 0x02,
    Medium = 0x03,
    High = 0x04,
    Full = 0x05,
    Charging = 0xEE,
    Charged = 0xEF
};

#pragma pack(push, 1)
struct SharedControllerData
{
    uint8_t slot;
    SlotState state;
    DeviceModel model;
    ConnectionType connection;
    uint8_t mac_address[6];
    BatteryStatus battery_status;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct InfoController
{
    SharedControllerData data;
    uint8_t unknown = '\0';
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ActualControllerData
{
    SharedControllerData sharedData;
    uint8_t connected; // bool, zero or one, that's why no enum
    uint32_t packet_number;
    uint8_t padding[32] = {0}; // this is all of the buttons and touch etc, we
                               // don't care one bit about it
    uint64_t
        motion_data_timestamp_microseconds; // only updates when ACCELEROMETER is
                                            // updated, not gyro
    float accelerometer_x;
    float accelerometer_y;
    float accelerometer_z;
    float gyroscope_pitch;
    float gyroscope_yaw;
    float gyroscope_roll;
};
#pragma pack(pop)

// not implementing rumble structures
} // namespace outgoing

namespace incoming
{
enum class MatchAction : uint8_t
{
    AllSlots = 0,
    SlotBased = 1,
    MacBased = 2
};

#pragma pack(push, 1)
struct InfoController
{
    int32_t num_ports;
    // be very careful accessing this, as a lot of the time half or more will be
    // garbage
    uint8_t info_ports[4];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ActualControllerData
{
    MatchAction match_action;
    uint8_t slot_if_match;
    uint8_t mac_if_match[6];
};
#pragma pack(pop)

// again, not implementing rumble structures
} // namespace incoming

struct DSUControllers
{
    struct ControllerData
    {
        dsu::outgoing::ActualControllerData actualControllerData;
    };

    std::vector<ControllerData> controllerData;
};

class DSUClient
{
  public:
    DSUClient(uint32_t client_id, uint32_t server_id,
              std::shared_ptr<asio::ip::udp::socket> server_socket,
              asio::ip::udp::endpoint remote_endpoint,
              std::shared_ptr<DSUControllers> controllers);

    DSUClient() = default;

  private:
    uint32_t client_id_;
    uint32_t server_id_;

    uint32_t packet_number_;

    asio::ip::udp::endpoint remote_endpoint_;
    std::shared_ptr<asio::ip::udp::socket> server_socket_;

    std::shared_ptr<DSUControllers> controllers_;

    bool active;

    // hyper unnessesary optimisation
    uint8_t controllers_enabled = 0;

    [[nodiscard]]
    auto IsSlotOn(uint8_t slot) const -> bool
    {
        if (slot >= 4)
        {
            return false;
        }
        return (controllers_enabled & (1 << slot)) != 0;
    }

    void SetSlot(uint8_t slot, bool enabled)
    {
        if (slot >= 4)
        {
            return;
        }
        if (enabled)
        {
            controllers_enabled |= (1 << slot);
        } else
        {
            controllers_enabled &= ~(1 << slot);
        }
    }

    void ForwardReq(Header* header, std::vector<uint8_t> payload);

    void SendPacket(EventType event, std::vector<uint8_t> payload);

    void Send(std::vector<uint8_t> packet);
    void UpdateControllers();
    // 'D' 'S' 'U' 'S' packed into uint32_t
    inline static const uint32_t DSUS = 0x44535553;

    friend class DSUServer;
};

class DSUServer
{
  public:
    DSUServer(asio::io_context& io_context, std::string ip_addr, int port = 26760,
              std::vector<uint8_t[6]> mac_addresses = {});

    ~DSUServer()
    {
        io_context_.stop();
        if (listening_thread_.joinable())
        {
            listening_thread_.join();
        }
    }

    std::shared_ptr<DSUControllers> controllers;

    void Update();

    void StartListeningThread();

  private:
    void StartReceive();

    // DO NOT PUT THIS BELOW SOCKET
    std::unordered_map<asio::ip::udp::endpoint, std::shared_ptr<DSUClient>>
        clients_;

    std::shared_ptr<asio::ip::udp::socket> socket_;
    asio::ip::udp::endpoint remote_endpoint_;
    std::array<char, 4096> buffer_;
    uint32_t server_id_;
    asio::io_context& io_context_;

    std::thread listening_thread_;
};

} // namespace dsu

#include "dsu.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <random>

#include <CRC.h>

#include <future>

#include "setting.hpp"

using asio::ip::udp;

namespace dsu
{
DSUClient::DSUClient(uint32_t client_id, uint32_t server_id,
                     std::shared_ptr<asio::ip::udp::socket> server_socket,
                     udp::endpoint remote_endpoint,
                     std::shared_ptr<DSUControllers> controllers)
    : client_id_(client_id), server_id_(server_id), server_socket_(server_socket),
      remote_endpoint_(remote_endpoint), controllers_(controllers)
{
}

void DSUClient::ForwardReq(Header* header, std::vector<uint8_t> payload)
{
    switch (header->event)
    {
    case dsu::EventType::ProtocolVersionInfo: {
        outgoing::ProtocolVersionInfo info{};
        info.maximal_version = 1001;

        std::vector<uint8_t> payload(reinterpret_cast<uint8_t*>(&info),
                                     reinterpret_cast<uint8_t*>(&info) +
                                         sizeof(info));

        SendPacket(EventType::ProtocolVersionInfo, payload);

        break;
    }
    case dsu::EventType::InfoController: {
        auto* infoController =
            reinterpret_cast<incoming::InfoController*>(payload.data());

        std::vector<uint8_t> pleaseGiveInfo(std::begin(
                                                infoController->info_ports),
                                            std::begin(
                                                infoController->info_ports) +
                                                infoController->num_ports);

        for (uint8_t& controller : pleaseGiveInfo)
        {
            outgoing::InfoController info;
            info.data = controllers_->controllerData[controller]
                            .actualControllerData.sharedData;

            std::vector<uint8_t> payload(reinterpret_cast<uint8_t*>(&info),
                                         reinterpret_cast<uint8_t*>(&info) +
                                             sizeof(info));

            SendPacket(EventType::InfoController, payload);
            break;
        }
    }
    case EventType::ActualControllerData: {
        auto* actualControllerData =
            reinterpret_cast<incoming::ActualControllerData*>(payload.data());

        switch (actualControllerData->match_action)
        {
        case incoming::MatchAction::AllSlots: {
            for (uint8_t i = 0; i < 4; i++)
            {
                SetSlot(i, true);
            }
            break;
        }
        case incoming::MatchAction::SlotBased: {
            SetSlot(actualControllerData->slot_if_match, true);
            break;
        }
        case dsu::incoming::MatchAction::MacBased: {
            for (auto& controller : controllers_->controllerData)
            {
                // comparing arrays, cursed edition
                if (std::memcmp(controller.actualControllerData.sharedData
                                    .mac_address,
                                actualControllerData->mac_if_match, 6) == 0)
                {
                    SetSlot(controller.actualControllerData.sharedData.slot,
                            true);
                    break;
                }
            }

            break;
        }
        }

        break;
    }
    case EventType::InfoRumble:
        break; // ignorio
    case EventType::SetRumble:
        break; // ignorio
    default:
        std::cout << "Somehow, there was an unknown event." << std::endl;
    }
}

void DSUClient::SendPacket(EventType event, std::vector<uint8_t> payload)
{
    auto header = Header();

    header.magic = DSUS;
    header.version = 1001;
    header.size = payload.size() + sizeof(Header);
    header.crc32 = 0;
    header.server_or_client_id = server_id_;
    header.event = event;

    std::vector<uint8_t> packet(header.size);
    std::memcpy(packet.data(), &header, sizeof(Header));
    std::memcpy(packet.data() + sizeof(Header), payload.data(), payload.size());

    uint32_t new_crc32 =
        CRC::Calculate(packet.data(), packet.size(), CRC::CRC_32());

    auto* vec_header = reinterpret_cast<Header*>(packet.data());
    vec_header->crc32 = new_crc32;

    Send(packet);
}

void DSUClient::Send(std::vector<uint8_t> packet)
{
    if (!server_socket_)
    {
        return;
    }

    server_socket_->async_send_to(asio::buffer(packet), remote_endpoint_,
                                  [packet](std::error_code error_code,
                                           std::size_t bytes_sent) {
                                      if (error_code)
                                      {
                                          std::cerr << "Send error: "
                                                    << error_code.message()
                                                    << "\n";
                                      } else
                                      {
#if PACKET_LOG
                                          std::cout << "Sent " << bytes_sent
                                                    << " bytes\n";
#endif
                                      }
                                  });
}

void DSUClient::UpdateControllers()
{
    for (auto& controller : controllers_->controllerData)
    {
        if (controller.actualControllerData.sharedData.state !=
            outgoing::SlotState::Connected)
        {
            continue;
        }

        // so dumb it has this, idk why it needs it
        controller.actualControllerData.connected = 1;

        if (!IsSlotOn(controller.actualControllerData.sharedData.slot))
        {
            continue;
        }

        std::vector<uint8_t> payload(sizeof(dsu::outgoing::ActualControllerData));
        std::memcpy(payload.data(), &controller.actualControllerData,
                    payload.size());

        SendPacket(EventType::ActualControllerData, payload);
    }
}

DSUServer::DSUServer(asio::io_context& io_context, std::string ip_addr, int port,
                     std::vector<uint8_t[6]> mac_addresses)
    : socket_(std::make_shared<
              asio::ip::udp::socket>(io_context,
                                     udp::endpoint(asio::ip::make_address(
                                                       ip_addr),
                                                   port))),
      buffer_(), server_id_(0), controllers(std::make_shared<DSUControllers>()),
      io_context_(io_context)
{
    std::cout << "Listening on " << ip_addr << ":" << port << std::endl;

    std::mt19937 rng(std::random_device{}());
    server_id_ = rng();

    for (int i = 0; i < 4; i++)
    {
        auto controller_data = DSUControllers::ControllerData();

        auto& sharedData = controller_data.actualControllerData.sharedData;
        auto& actualControllerData = controller_data.actualControllerData;

        sharedData.slot = i;
        sharedData.state = outgoing::SlotState::Disconnected;
        sharedData.model = outgoing::DeviceModel::NotApplicable;
        sharedData.connection = outgoing::ConnectionType::NotApplicable;

        const std::vector<std::array<uint8_t, 6>> default_mac_addresses =
            {{0xf9, 0xed, 0xbe, 0xfb, 0x11, 0xc1},
             {0x43, 0xfd, 0x33, 0x32, 0x12, 0x56},
             {0xad, 0xdf, 0x27, 0xbe, 0x45, 0xad},
             {0x65, 0x12, 0xfe, 0xf2, 0xb2, 0x1d}};

        if (i >= mac_addresses.size())
        {
            std::memcpy(&sharedData.mac_address, &default_mac_addresses[i], 6);
        } else
        {
            std::memcpy(&sharedData.mac_address, &mac_addresses[i], 6);
        }

        sharedData.battery_status = outgoing::BatteryStatus::NotApplicable;

        actualControllerData.connected = 0;
        actualControllerData.packet_number = 0;

        actualControllerData.motion_data_timestamp_microseconds = 0;
        actualControllerData.accelerometer_x = 0;
        actualControllerData.accelerometer_y = 0;
        actualControllerData.accelerometer_z = 0;

        actualControllerData.gyroscope_pitch = 0;
        actualControllerData.gyroscope_yaw = 0;
        actualControllerData.gyroscope_roll = 0;

        controllers->controllerData.push_back(controller_data);
    }

    StartReceive();
}

void DSUServer::Update()
{
    for (auto& client : clients_)
    {
        client.second->UpdateControllers();
    }
}

void DSUServer::StartReceive()
{
    if (!socket_)
    {
        return;
    }

    socket_->async_receive_from(
        asio::buffer(buffer_), remote_endpoint_,
        [this](std::error_code error, size_t bytes_received) {
            if (!error && bytes_received > 0)
            {
#if PACKET_LOG
                std::cout << "Received " << bytes_received << " bytes from "
                          << remote_endpoint_.address().to_string() << ":"
                          << remote_endpoint_.port() << std::endl;
#endif

                if (bytes_received < sizeof(Header))
                {
                    std::cout << "Received incomplete header" << std::endl;
                    return;
                }

                auto* header = reinterpret_cast<Header*>(buffer_.data());

                const size_t header_size = 16;

                // remember, message type is part of payload
                if (header->size != bytes_received - header_size)
                {
                    std::cout << "Received invalid header size" << std::endl;
                    return;
                }

                uint32_t old_crc32 = header->crc32;
                header->crc32 = 0;

                uint32_t new_crc =
                    CRC::Calculate(buffer_.data(), bytes_received, CRC::CRC_32());

                header->crc32 = old_crc32;
                if (new_crc != old_crc32)
                {
                    std::cout << "Received invalid CRC32, expected " << old_crc32
                              << ", got " << new_crc << std::endl;
                    return;
                }

                std::string magic(reinterpret_cast<char*>(&header->magic), 4);

                if (magic == "DSUS")
                {
#if PACKET_LOG
                    std::cout << "From server (DSUS, me), ignoring..."
                              << std::endl;
#endif
                    return;
                }

                if (magic != "DSUC")
                {
#if PACKET_LOG
                    std::cout << "Req is not from DSUC (client/cemu) "
                                 "or DSUS "
                                 "(server/me), wut"
                              << std::endl;
                    std::cout << "Listening on "
                              << remote_endpoint_.address().to_v4().to_string()
                              << ":" << remote_endpoint_.port() << std::endl;
#endif
                    return;
                }
#if PACKET_LOG
                std::cout << "From client (DSUC, cemu), processing..."
                          << std::endl;
#endif

                if (bytes_received > buffer_.size())
                {
                    std::cout << "MORE DATA RECEIVED THAN BUFFER SIZE"
                              << std::endl;
                }

                std::vector<uint8_t> payload(buffer_.begin() + sizeof(Header),
                                             buffer_.begin() + bytes_received);

                auto client = clients_.find(remote_endpoint_);

                if (client != clients_.end() && !client->second &&
                    client->second->client_id_ != header->server_or_client_id)
                {
                    clients_.erase(client);
                }

                if (client == clients_.end())
                {
                    clients_[remote_endpoint_] =
                        std::make_shared<DSUClient>(header->server_or_client_id,
                                                    server_id_, socket_,
                                                    remote_endpoint_,
                                                    controllers);

                    std::cout << "new client " << header->server_or_client_id
                              << " connected from " << remote_endpoint_.address()
                              << ":" << remote_endpoint_.port() << std::endl;
                }
                clients_[remote_endpoint_]->ForwardReq(header, payload);
                /*std::cout << header->server_or_client_id << " "
                          << remote_endpoint_.address() << ":"
                          << remote_endpoint_.port() << std::endl;*/
            }

            StartReceive();
        });
}

void DSUServer::StartListeningThread()
{
    listening_thread_ = std::thread([this]() { io_context_.run(); });
}
} // namespace dsu

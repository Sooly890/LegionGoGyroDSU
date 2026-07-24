#include "dsu.hpp"

#include <asio/io_context.hpp>

#include <algorithm>
#include <iostream>

#include <chrono>
#include <deque>
#include <thread>

#include "iio.hpp"
#include "iio_motion_source.hpp"
#include "iio_to_dsu.hpp"
#include "legion_hid_motion_source.hpp"
#include "motion_source.hpp"

#include "setting.hpp"

#include <atomic>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr auto HidRecoveryInitialDelay = std::chrono::seconds(1);
constexpr auto HidRecoveryMaximumDelay = std::chrono::seconds(5);

auto requested_motion_source(int argc, char* argv[]) -> std::string
{
    constexpr std::string_view prefix = "--motion-source=";
    std::string source = "auto";
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument.rfind(prefix, 0) == 0)
            source = argument.substr(prefix.size());
        else
            throw std::invalid_argument("Unknown argument: " +
                                        std::string(argument));
    }
    if (source != "auto" && source != "iio" && source != "legion-hid")
        throw std::invalid_argument("Unknown motion source: " + source);
    return source;
}

auto make_motion_source(const std::string& requested)
    -> std::unique_ptr<motion::MotionSource>
{
    if (requested == "auto" || requested == "legion-hid")
    {
        auto hid = std::make_unique<motion::LegionHIDMotionSource>();
        if (hid->initialize())
            return hid;
        std::cerr << (requested == "auto"
                          ? "No compatible Legion HID motion source found; "
                            "using IIO\n"
                          : "Falling back to the IIO motion source\n");
    }

    auto iio = std::make_unique<motion::IIOMotionSource>();
    if (iio->initialize())
        return iio;
    return nullptr;
}
} // namespace

std::atomic<bool> running{true};

void signal_handler(int)
{
    running = false;
}

auto main(int argc, char* argv[]) -> int
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string motion_source_name;
    try
    {
        motion_source_name = requested_motion_source(argc, argv);
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n'
                  << "Usage: LegionGoGyro "
                     "[--motion-source=auto|iio|legion-hid]\n";
        return 2;
    }

    const char* port_str = std::getenv("LGDSU_PORT");
    const char* ip_str = std::getenv("LGDSU_IP");
    const char* gyro_matrix_cstr = std::getenv("LGDSU_GYRO_MATRIX");
    const char* accel_matrix_cstr = std::getenv("LGDSU_ACCEL_MATRIX");

    int port = 26760;
    std::string ip = "127.0.0.1";

    auto motion_source = make_motion_source(motion_source_name);
    if (!motion_source)
    {
        std::cerr << "Unable to initialize a motion source\n";
        return 1;
    }

    // The Legion controller HID gyro/accelerometer axes already line up with
    // the pitch/yaw/roll frame this program sends over DSU (cross-checked
    // against Handheld Daemon's independently derived Legion Go axis
    // mapping), so they need no further permutation or sign flip here. The
    // IIO source's kernel gyro driver uses a different physical axis layout
    // and still needs the historical correction below.
    const bool is_legion_hid =
        dynamic_cast<motion::LegionHIDMotionSource*>(motion_source.get()) !=
        nullptr;

    std::string default_gyro_matrix_str = is_legion_hid ? "x,z,y" : "-x,-y,z";
    std::string default_accel_matrix_str =
        is_legion_hid ? "x,y,z" : "x,z,-y";

    auto default_gyro_matrix = IIOToDSU(default_gyro_matrix_str, nullptr);
    auto default_accel_matrix = IIOToDSU(default_accel_matrix_str, nullptr);

    auto gyro_matrix =
        IIOToDSU(std::string(gyro_matrix_cstr != nullptr ? gyro_matrix_cstr : ""),
                 &default_gyro_matrix);

    auto accel_matrix =
        IIOToDSU(std::string(accel_matrix_cstr != nullptr ? accel_matrix_cstr
                                                          : ""),
                 &default_accel_matrix);

    if (port_str != nullptr)
    {
        port = strtoll(port_str, nullptr, 10);
        std::cout << "Using port " << port << " and not " << 26760 << std::endl;
    }

    if (ip_str != nullptr)
    {
        ip = ip_str;
        std::cout << "Using IP " << ip << " and not " << "127.0.0.1" << std::endl;
    }

    if (gyro_matrix_cstr != nullptr)
    {
        std::cout << "Using gyro matrix (or default if it's invalid): "
                  << gyro_matrix_cstr << std::endl;
    }

    if (accel_matrix_cstr != nullptr)
    {
        std::cout << "Using accel matrix (or default if it's invalid): "
                  << accel_matrix_cstr << std::endl;
    }

    asio::io_context ioc;
    auto server = dsu::DSUServer(ioc, ip, port);

    auto& controller0 =
        server.controllers->controllerData[0].actualControllerData;
    auto& controller0Shared =
        server.controllers->controllerData[0].actualControllerData.sharedData;

    controller0Shared.state = dsu::outgoing::SlotState::Connected;
    controller0Shared.model = dsu::outgoing::DeviceModel::AllGyro;
    controller0Shared.connection = dsu::outgoing::ConnectionType::Bluetooth;
    controller0Shared.battery_status = dsu::outgoing::BatteryStatus::Full;

    // don't really need to do this as the dsu.cpp will auto do it, cause it's
    // dumb it's needed twice
    controller0.connected = 1;

    auto& controller0gyro_x = controller0.gyroscope_pitch;
    auto& controller0gyro_y =
        controller0.gyroscope_roll; // yes this is not a mistake
    auto& controller0gyro_z =
        controller0.gyroscope_yaw; // yaw and roll are flipped

    server.StartListeningThread();

    auto epoch = std::chrono::high_resolution_clock::now();

    auto frame_start = std::chrono::high_resolution_clock::now();

#if POS_LOG
    iio::Vec3 debug_global_gyro;
#endif

    bool motion_source_failed = false;
    bool recovering_legion_hid = false;
    auto hid_recovery_delay = HidRecoveryInitialDelay;
    while (running)
    {
        motion::MotionSample sample;
        if (!motion_source->poll(sample))
        {
            if (!running)
                break;
            if (is_legion_hid)
            {
                // Suspend can remove hidraw devices for several seconds. Keep
                // the DSU socket and its client registrations alive while the
                // HID backend waits for the controller to reappear.
                std::cerr << "Legion HID motion source is unavailable; "
                             "retrying in "
                          << hid_recovery_delay.count() << " seconds\n";
                recovering_legion_hid = true;
                std::this_thread::sleep_for(hid_recovery_delay);
                hid_recovery_delay =
                    std::min(hid_recovery_delay * 2,
                             HidRecoveryMaximumDelay);
                continue;
            }
            std::cerr << "Motion source stopped producing samples\n";
            motion_source_failed = true;
            break;
        }

        if (recovering_legion_hid)
        {
            std::cout << "Legion HID motion source recovered\n";
            recovering_legion_hid = false;
            hid_recovery_delay = HidRecoveryInitialDelay;
        }

        auto now = std::chrono::high_resolution_clock::now();
        double deltaTime = sample.dt_seconds > 0.0
                               ? sample.dt_seconds
                               : std::chrono::duration<double>(now - frame_start)
                                     .count();
        frame_start = now;

        iio::Vec3 gyro(sample.gyro.x, sample.gyro.y, sample.gyro.z);
        iio::Vec3 accel(sample.accel.x, sample.accel.y, sample.accel.z);

        // obsolete hardcoded matrix
        /*double old_gyro_x = gyro.x;
        double old_gyro_y = gyro.y;
        double old_gyro_z = gyro.z;

        gyro.x = -old_gyro_x;
        gyro.y = -old_gyro_y;
        gyro.z = old_gyro_z;

        double old_accel_x = accel.x;
        double old_accel_y = accel.y;
        double old_accel_z = accel.z;

        accel.x = old_accel_x;
        accel.y = old_accel_z;
        accel.z = -old_accel_y;*/

        gyro = gyro_matrix.Convert(gyro);
        accel = accel_matrix.Convert(accel);

#if POS_LOG
        debug_global_gyro += gyro * deltaTime;

        std::cout << "Gyro: " << gyro.x << ", " << gyro.y << ", " << gyro.z
                  << std::endl;
        std::cout << "Debug Global Gyro: " << debug_global_gyro.x << ", "
                  << debug_global_gyro.y << ", " << debug_global_gyro.z
                  << std::endl;
        std::cout << "Accel: " << accel.x << ", " << accel.y << ", " << accel.z
                  << std::endl;
        std::cout << "Delta Time: " << deltaTime << std::endl;
#endif

        bool gyro_changed = gyro.x != controller0gyro_x ||
                            gyro.y != controller0gyro_y ||
                            gyro.z != controller0gyro_z;

        bool accel_changed = accel.x != controller0.accelerometer_x ||
                             accel.y != controller0.accelerometer_y ||
                             accel.z != controller0.accelerometer_z;

        if (gyro_changed || accel_changed)
        {
            {
                std::lock_guard lock(server.controllers->sensor_mutex);
                controller0gyro_x = gyro.x;
                controller0gyro_y = gyro.y;
                controller0gyro_z = gyro.z;

                if (accel_changed)
                {
                    controller0.motion_data_timestamp_microseconds =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            now - epoch)
                            .count();
                    controller0.accelerometer_x = accel.x;
                    controller0.accelerometer_y = accel.y;
                    controller0.accelerometer_z = accel.z;
                }
            }

            if (running)
            {
                asio::post(ioc, [&server]() { server.Update(); });
            }
        }
    }

    return motion_source_failed ? 1 : 0;
}

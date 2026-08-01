# LegionGoGyroDSU

A DSU motion server for the Lenovo Legion Go S and Legion Go 2 on Linux. It
publishes gyroscope and accelerometer data for applications that support the
DSU/Cemuhook protocol.

## Supported Devices

| Device | Motion backend | Selection with `auto` |
| ------ | -------------- | --------------------- |
| Legion Go S | Linux Industrial I/O (`gyro_3d` and `accel_3d`) | IIO |
| Legion Go 2 | Lenovo controller HID reports | Legion HID |

The Legion HID backend recognizes Lenovo vendor ID `0x17EF` with these product
IDs:

```text
0x6182  0x6183  0x6184  0x6185
0x61EB  0x61EC  0x61ED  0x61EE
```

## Installation

To install LegionGoGyroDSU, ensure you have a user password set (run `passwd` if not), then execute the following command:

```bash
curl -fsSL https://raw.githubusercontent.com/Sooly890/LegionGoGyroDSU/main/scripts/install.sh | bash
```

The project is installed to `/LegionGoGyroDSU`. Reboot after installation so
that the IIO driver configuration is applied on the Legion Go S. The installed
systemd service runs as root, which also gives it access to the Legion Go 2's
`/dev/hidraw*` controller interface.

## Configuration

You can customize the port, IP, and sensor orientation by editing the service file:

```bash
sudo nano /etc/systemd/system/lgdsu.service
```

### Available Options

| Environment Variable  | Default Value                            | Description                                           |
| --------------------- | ----------------------------------------- | ----------------------------------------------------- |
| `LGDSU_PORT`         | `26760`                                   | The port used by the DSU server.                      |
| `LGDSU_IP`           | `127.0.0.1`                               | Bind IP. Use `0.0.0.0` to allow external connections. |
| `LGDSU_GYRO_MATRIX`  | `-x,-y,z` (IIO) / `x,z,y` (Legion HID)    | Orientation matrix for the gyroscope.                 |
| `LGDSU_ACCEL_MATRIX` | `x,z,-y` (IIO) / `x,y,z` (Legion HID)     | Orientation matrix for the accelerometer.             |

The default matrix depends on which motion backend is active. The IIO
backend's kernel gyro driver and the Legion HID controller's IMU use
different physical axis layouts, so the same matrix does not orient both
correctly; the Legion HID default was derived by cross-checking Lenovo's raw
axis byte offsets/signs against Handheld Daemon's independently-derived
Legion Go mapping. Override the variable if your controller still needs
adjustment.

### Motion Source

Motion-source selection defaults to `auto`:

1. Search `/dev/hidraw*` for a supported Lenovo controller interface.
2. Verify the vendor ID, product ID, and HID usage page.
3. Enable controller IMU reporting and use the Legion HID backend.
4. If no usable controller interface is found, use IIO.

This selects controller HID motion on the Legion Go 2 and IIO on the Legion Go
S without requiring a command-line option.

To select a backend explicitly, pass:

```bash
sudo /LegionGoGyroDSU/LegionGoGyroDSU --motion-source=iio
sudo /LegionGoGyroDSU/LegionGoGyroDSU --motion-source=legion-hid
```

`--motion-source=legion-hid` also falls back to IIO if no usable Legion HID
interface is found.

To set an explicit backend for the installed service, append the option to its
`ExecStart` line. For example:

```ini
ExecStart=/LegionGoGyroDSU/LegionGoGyroDSU --motion-source=legion-hid
```

After making changes, apply them by running:

```bash
sudo systemctl daemon-reload
sudo systemctl restart lgdsu.service
```

_Note: Updating the software will overwrite these changes._

### Sensor Orientation

If the directions feel wrong, adjust the matrix values in the service file. The mapping follows:

- `x`: Pitch
- `y`: Roll
- `z`: Yaw

The """console""" also uses the accelerometer to calibrate the gyroscope on the fly (it's affected by gravity), so even if you're sure the gyroscope settings are correct the accelerometer settings might not be.

## Troubleshooting

### Checking the Selected Backend

Stop the service and run the program in a terminal to see which backend is
selected:

```bash
sudo systemctl stop lgdsu
sudo /LegionGoGyroDSU/LegionGoGyroDSU --motion-source=auto
```

Successful HID selection prints the selected `/dev/hidraw*` path. If HID is not
available, the program reports that it is using IIO instead.

### Legion Go 2 HID Troubleshooting

List the available raw HID devices and their Lenovo identifiers:

```bash
for device in /sys/class/hidraw/hidraw*/device/uevent; do
    echo "$device"
    grep -E 'HID_ID|HID_NAME' "$device"
done
```

The controller must use vendor ID `17EF`, one of the supported product IDs
listed above, and expose the Lenovo vendor interface. When running the binary
as a regular user, permission to open the selected `/dev/hidraw*` device for
both reading and writing is required because initialization packets must be
sent. The installed root service already has this access.

If another controller-management daemon owns or continuously reconfigures the
same HID interface, stop it temporarily while testing.

### Legion Go S IIO Troubleshooting

Previously, IIO devices (gyro/accel) sometimes failed to appear. This has been fixed by forcing the `hid_sensor_hub` module to load. If you still encounter issues, you can run:

```bash
/LegionGoGyroDSU/check.sh
```

**Technical reason:** The `hid_sensor_hub` kernel module was sometimes lazily loading, so `hid-generic` never left it, I don't know why. The Legion Go S's USB device actually wants `hid-sensor-hub` to load, however the sometimes not working bit was when it didn't request it, I don't know why this is either. The fix simply adds something that requests it to load earlier, so therefore `hid_sensor_hub` does remove `hid-generic`.

### Root Access

The IIO backend requires root access to enable buffered high-speed sensor
access. The HID backend needs read/write access to its hidraw interface. The
provided service runs as root and satisfies both requirements.

### Additional Troubleshooting

If you cannot determine the error, run the application directly in a terminal.

```bash
# Stop the background service
sudo systemctl stop lgdsu

# Run it in terminal
sudo /LegionGoGyroDSU/LegionGoGyroDSU --motion-source=auto

# Optionally, start LegionGoGyroDSU again
sudo systemctl start lgdsu
```

### Bind port already in use/segfault 

Make sure nothing is using the default port (26760), most commonly SteamDeckDSU (uninstalling is the best way), or change the port LegionGoSGyroDSU uses.

## Uninstallation

If you wish to remove the project, run:

```bash
sudo /LegionGoGyroDSU/uninstall.sh
```

## Development & Building

Building on either handheld is not recommended. Use another Linux machine:

```bash
# Clone the repository
git clone https://github.com/Sooly890/LegionGoGyroDSU
cd LegionGoGyroDSU

# Install dependencies
sudo pacman -S asio libiio

# Build and package
scripts/build.sh
scripts/package.sh
```

Run the protocol decoder tests with CTest:

```bash
ctest --test-dir build --output-on-failure
```

### Legion HID Protocol Status

The report ID, IMU byte layout, signed big-endian decoding, scaling factors,
side-specific axis signs, and supported identifiers are kept in
`src/legion_hid_protocol.cpp`. The axis signs normalize the different physical
orientations of the left and right controllers before the existing
user-configurable DSU matrices are applied.

The following protocol details still require confirmation across real
controller firmware versions and are deliberately isolated there:

- the high-quality report activation command;
- the assumption that one 8-bit timestamp step represents 8 milliseconds;
- the side-specific axis signs on Legion Go 2 production hardware.

Please include the controller product ID, firmware version, selected hidraw
path, and observed report rate when reporting Legion Go 2 HID problems.

---

_If you encounter any issues, please open an issue on GitHub!_

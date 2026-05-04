# LegionGoSGyroDSU

A project to enable Motion (Gyro and Accelerometer) DSU for the Legion Go S SteamOS edition.

## Installation

To install LegionGoSGyroDSU, ensure you have a user password set (run `passwd` if not), then execute the following command:

```bash
curl -fsSL https://raw.githubusercontent.com/Sooly890/LegionGoSGyroDSU/main/scripts/install.sh | bash
```

**Note:** The project will be installed to `/LegionGoSGyroDSU`. After installation, please reboot your system to ensure the IIO sensors are correctly initialized.

## Configuration

You can customize the port, IP, and sensor orientation by editing the service file:

```bash
sudo nano /etc/systemd/system/lgsdsu.service
```

### Available Options

| Environment Variable  | Default Value | Description                                           |
| --------------------- | ------------- | ----------------------------------------------------- |
| `LGSDSU_PORT`         | `26760`       | The port used by the DSU server.                      |
| `LGSDSU_IP`           | `127.0.0.1`   | Bind IP. Use `0.0.0.0` to allow external connections. |
| `LGSDSU_GYRO_MATRIX`  | `-x,-y,z`     | Orientation matrix for the gyroscope.                 |
| `LGSDSU_ACCEL_MATRIX` | `x,z,-y`      | Orientation matrix for the accelerometer.             |

After making changes, apply them by running:

```bash
sudo systemctl daemon-reload
sudo systemctl restart lgsdsu.service
```

_Note: Updating the software will overwrite these changes._

### Sensor Orientation

If the directions feel wrong, adjust the matrix values in the service file. The mapping follows:

- `x`: Pitch
- `y`: Roll
- `z`: Yaw

The """console""" also uses the accelerometer to calibrate the gyroscope on the fly (it's affected by gravity), so even if you're sure the gyroscope settings are correct the accelerometer settings might not be.

## Troubleshooting

### The IIO Issue

Previously, IIO devices (gyro/accel) sometimes failed to appear. This has been fixed by forcing the `hid_sensor_hub` module to load. If you still encounter issues, you can run:

```bash
/LegionGoSGyroDSU/check.sh
```

**Technical reason:** The hid_sensor_hub kernel module was sometimes lazily loading, so hid-generic never left it, I don't know why. The Legion Go S's USB device actually wants hid-sensor-hub to load, however the sometimes not working bit was when it didn't request it, I don't know why this is either. The fix simply adds something that requests it to load, so therefore hid_sensor_hub does remove hid-generic.

### Root Access

This project requires root access to enable buffering mode on IIO devices, which is necessary for high-speed sensor data access.

## Uninstallation

If you wish to remove the project, run:

```bash
sudo /LegionGoSGyroDSU/uninstall.sh
```

## Development & Building

Building on the Legion Go S itself is not recommended. Use another Arch Linux machine:

```bash
# Clone the repository
git clone https://github.com/Sooly890/LegionGoSGyroDSU
cd LegionGoSGyroDSU

# Install dependencies
sudo pacman -S asio libiio

# Build and package
scripts/build.sh
scripts/package.sh
```

---

_If you encounter any issues, please open an issue on GitHub!_

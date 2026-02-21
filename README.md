# LegionGoSGyroDSU

A project to get a Motion (Gyro and Accel) DSU to work on the Legion Go S SteamOS edition.

IMPORTANT NOTE, READ FIRST: This project does work, however, for some reason, the IIO (Industrial I/O) devices this project relies on does not seem to always exist. Please look below for the "solution".

# Installation

Run `curl https://raw.githubusercontent.com/Sooly890/LegionGoSGyroDSU/refs/heads/main/scripts/install.sh | bash`
This will install it to /LegionGoSGyroDSU

# The IIO Issue

As mentioned before, the IIO devices that the project needs (gyro and accelometer) sometimes do not appear. You can "fix" this by installing this, and then running:
`/LegionGoSGyroDSU/check.sh`

if this says error, you must force shutdown the console, wait 10 mins (not exactly, just a bit of time), and then run the check.sh again. Rinse and repeat until it works, it normally takes a few tries.

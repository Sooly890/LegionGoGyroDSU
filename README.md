# LegionGoSGyroDSU

A project to get a Motion (Gyro and Accel) DSU to work on the Legion Go S SteamOS edition.

IMPORTANT NOTE, READ FIRST: This project does work, however, for some reason, the IIO (Industrial I/O) devices this project relies on does not seem to always exist. To fix this, force shut down the console, wait 10 minutes or more, then boot it up again. There is a script (check.sh) that will be installed to your system to check whether or not the IIO devices are present. If they are not, rinse and repeat

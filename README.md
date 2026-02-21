# LegionGoSGyroDSU

A project to get a Motion (Gyro and Accel) DSU to work on the Legion Go S SteamOS edition.

~~IMPORTANT NOTE, READ FIRST: This project does work, however, for some reason, the IIO (Industrial I/O) devices this project relies on does not seem to always exist. Please look below for the "solution".~~ fixed

# Installation

Please make sure you have a password by running `passwd` (ignore this if you have a password), and then run

`curl -fsSL https://raw.githubusercontent.com/Sooly890/LegionGoSGyroDSU/main/scripts/install.sh | bash`

This will install it to `/LegionGoSGyroDSU`

If you desire, you can change the port by running:
`sudo nano /etc/systemd/system/lgsdsu.service`

then editing the `LGSDSU_PORT=26760` to whatever you want
press CTRL+S to save, CTRL+X to exit. Then run:

```
sudo systemctl daemon-reload
sudo systemctl enable lgsdsu.service
sudo systemctl restart lgsdsu.service
```

# The IIO Issue

As mentioned before, the IIO devices that the project needs (gyro and accelometer) sometimes do not appear. You can "fix" this by installing this, and then running:
`/LegionGoSGyroDSU/check.sh`

If this says error, you must force shutdown the console, wait 10 mins (not exactly, just a bit of time), and then rrestart.un the check.sh again. Rinse and repeat until it works, it normally takes a few tries.

~~I have properly fixed the IIO issue - please run the install system again.~~
~~The check.sh is still there in the event that it happens again.~~

turns out it was a fluke, I was just really lucky. Will do some more digging.

Technical reason: The IIO kernel moduels were loading before the sensors existed, a race condition. I simply added a delay of 10 seconds (far too much, but does work) and it worked.

# Wait, does this actually need root access?

Yes. This project requires root access to run and install. This is because the project needs to access the IIO devices, which are accessible to normal users, however we need to enable buffering mode on them, which requires root access, otherwise they are simply too slow.

# I want to build this myself, how?

It is fairly straightforward to build, however do not build it on your Legion as that would require setting it up for development, which is not worth it. Instead, with another arch linux machine, run:

```
git clone https://github.com/Sooly890/LegionGoSGyroDSU
cd LegionGoSGyroDSU
sudo pacman -S asio libiio
scripts/build.sh
```

and then run `scripts/package.sh` if you want to package it like in the releases.

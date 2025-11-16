# I2S-driver

## 1. Kernel Driver (i2s_driver.c) - Character device driver that:

* Creates /dev/i2s0 device node
* Supports read/write operations for audio data
* Provides IOCTL interface for configuration
* Manages sample rate, bit depth, and device state


## 2. System Daemon (i2sd.c) - Background service that:

* Runs as a systemd service
* Manages the I2S device
* Provides Unix domain socket for IPC
* Handles control commands and status queries
* Logs to syslog


## 3.User Space Library (libi2s.h & libi2s.c) - Clean API that:

* Provides simple functions to open/close I2S device
* Configure sample rate, bit depth, channels
* Start/stop transmission
* Read/write audio data
* Communicate with daemon


## 4. Example Application (i2s_example.c) - Demonstrates:

Opening and configuring I2S device
Generating sine wave audio
Writing and reading audio data
Communicating with the daemon


Build System (Makefile) - Complete build infrastructure:

Compiles kernel module
Builds daemon and library
Creates example application
Installs systemd service
Handles installation/uninstallation



Quick Start:
bash# Build everything
make

# Install (requires root)
sudo make install

# Start the daemon
sudo systemctl start i2sd
sudo systemctl enable i2sd

# Run example application
./i2s_example

# Check daemon status
sudo systemctl status i2sd
Key Features:

Thread-safe with mutex locking
IOCTL-based configuration
Systemd integration for service management
Shared library (.so) for easy linking
Clean separation between kernel and user space
Error handling throughout
Syslog integration for daemon logging

The system follows Linux best practices and provides a production-ready foundation for I2S audio communication!

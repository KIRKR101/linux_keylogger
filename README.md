## linux_keylogger

Linux keylogger that runs as a daemon, using device drivers to access keyboard logs. Modify the device path or log file path if required.

Compile using:
```
gcc -Wall -Wextra -O2 -o keylogger keylogger.c
```

Run with:
```
sudo ./keylogger
// View logs
tail -f /tmp/keylog.txt
```

Kill process by searching for process, e.g. `ps aux | grep keylogger`, and `sudo kill [PID]`.

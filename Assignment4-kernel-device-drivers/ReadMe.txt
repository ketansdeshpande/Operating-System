Execution Steps - 

1. Compile - make

2. Insert module - sudo insmod device.ko buff_size=<size>

3. Run producer	- sudo ./producer /dev/dd

Open another terminal
4. Run consumer	- sudo ./consumer /dev/dd
After editing TGRK code in prog.grk, the following can be done
to upload program to device.

1. Plug in Serial to USB adapter.

2. Find your Serial port for serial adapter
3. Replace "s_port" variable value in tgrk.py with your port:
  e.g. `s_port = 'COM2'`
4. Run tgrk.py
5. Connections:
   - Adapter 5v to device VIN
   - Adapter GND to device GND
   - Adapter TX to device MISO
   - Adapter RX to device MOSI
7. Enter Run Menu on Tiny Bit Machine:
   - Both LED's off, press T button
9. Press an arrow button until only Led 0 is lit.
    - Do NOT press R button yet...
11. Enter selection in tgrk.py for upload or download,
    - tgrk.py will display waiting message.
13. Press R button on device. Serial program will do it's work
    - wait until "success!" message appears.
15. Press S button on device ONCE to exit Run menu.
16. If all went well, program is now loaded onto device,
  and pressing R button on device once will run program.

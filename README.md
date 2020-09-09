## LED Changer

Please refer to this [manual](https://docs.google.com/document/d/10ktbWafcsIgr-wQ0N86ys3DRBAduI77_dZYz6ZZdP00/edit?usp=sharing) for detailed usage.

This LED system consists of:
- 1 [Adafruit nRF52840 Express](https://www.adafruit.com/product/4481?gclid=Cj0KCQjw6ar4BRDnARIsAITGzlAzWMMNYfwhz_yMADS2IlwrZJUGaUtPHT0cBgKLShsUDa0SIwUO8NoaAhetEALw_wcB) with an attached LED known as the Leader Device
- 1 [Adafruit nRF52840 Express](https://www.adafruit.com/product/4481?gclid=Cj0KCQjw6ar4BRDnARIsAITGzlAzWMMNYfwhz_yMADS2IlwrZJUGaUtPHT0cBgKLShsUDa0SIwUO8NoaAhetEALw_wcB) with an attached LED known as the Follower Device

The Leader Device connects to the Bluefruit Connect Mobile App, [Google Play Store](https://play.google.com/store/apps/details?id=com.adafruit.bluefruit.le.connect&hl=en_US) or [Apple App Store](https://www.google.com/search?channel=fs&client=ubuntu&q=bluefruit+connect) via bluetooth.

The Leader Device then relays any instructions sent from the mobile app to the Follower Device also through bluetooth. The Follower Device only connects to its appropriate Leader Device. 

### Installation

The following was installed on Ubuntu 18.04.

1. Download and install the latest [Arduino IDE](https://www.arduino.cc/en/Main/Software).
2. Once installed, go to `File->Preferences` in the IDE and the url: `https://www.adafruit.com/package_adafruit_index.json` in the Additional Board Manager URL field.
3. Restart the IDE. Go to `Tools->Board->Boards Manager` in the IDE. Search and Install `Adafruit nRF52 by Adafruit`.
4. Once the toolchain has installed, select the appropriate board by going to `Tools->Board->Adafruit nRF52 Boards->Adafruit ItsyBitsy nRF52840 Express`.
5. (Linux Only) Install `adafruit-nrfutil` module using `pip3`.
```
pip3 install adafruit-nrfutil
```

Libraries:
Install the following libraries by searching for them under `Tools->Manage Libraries..`
- Adafruit Neopixel


### Flashing

1. Open either `LedFollower` or `LedLeader` with the Arduino IDE.
2. Confirm the correct board, `Adafruit ItsyBitsy nRF52840 Express` is selected under `Tools->Board`.
3. Confirm the correct serial port is connected under `Tools->Port`.
4. Flash software by pressing the Upload Button, right arrow button, or `Sketch->Upload`. This might have be done a couple times.

Notes:
- Make sure the `LedFollower` and `LedLeader`'s are using their corresponding UUIDs.
- You can also flash the boards from VSCode using the Arduino extension. See this [link](https://www.electronics-lab.com/project/programming-arduino-platform-io-arduino-extension-visual-studio-code-editor/) for more details.
- For any issues, refer to the [Adafruit Documentation](https://learn.adafruit.com/adafruit-itsybitsy-nrf52840-express/arduino-support-setup) to get started.

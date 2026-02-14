```markdown
# Raspberry Pi Pico W Tracker

This project is a tracker for the Raspberry Pi Pico W, utilizing the SIM7670G module to establish an internet connection and retrieve location data via its GPS functionality. The tracker communicates with a Telegram bot, allowing users to control and interact with the Raspberry Pi Pico W remotely.

## Features
- **Location Tracking**: Retrieves the current GPS location (latitude and longitude) using the SIM7670G module.
- **Telegram Bot Integration**: Communicates with a Telegram bot to receive commands and send responses.
- **Command Support**:
  - `/start`: Displays available commands.
  - `/location`: Retrieves and sends the current GPS location.
  - `/activo`: Activates the bot's active mode for faster responses.
  - `/lowEnergy`: Activates low-energy mode for reduced power consumption.

## Requirements
- Raspberry Pi Pico W
- SIM7670G module with GPS functionality
- Telegram bot token
- Authorized Telegram chat IDs
- CMake and build tools

## Building the Project
1. Set the `PICO_SDK_PATH` environment variable:
   ```bash
   export PICO_SDK_PATH=/pico_sdk_path
   ```
2. Create a build directory and navigate to it:
   ```bash
   mkdir build && cd build
   ```
3. Run the `cmake` command with the required parameters:
   ```bash
   cmake -DPICO_BOARD=pico

2

_w -DTELEGRAM_BOT_TOKEN='telegramToken' -DTELEGRAM_AUTORIZED_USERS='chatId1,chatIdN' -DSIM_PIN='1234' .. && make -j 32
   ```

## How It Works
1. The SIM7670G module is initialized to provide internet connectivity and GPS functionality.
2. The Telegram bot is configured with the provided token and listens for messages from authorized users.
3. Users can send commands to the bot to interact with the Raspberry Pi Pico W, such as retrieving its location or toggling between active and low-energy modes.

## Notes
- Replace `telegramToken` with your Telegram bot token.
- Replace `chatId1,chatIdN` with the authorized Telegram chat IDs.
- Replace `1234` with the SIM card PIN if required.

## License
This project is open-source and available under the [MIT License](LICENSE).
```
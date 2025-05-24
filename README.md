# LightHouse MCP-ESP Server

A powerful ESP32-based server implementing the Model Context Protocol (MCP) for IoT devices. This project provides a robust foundation for building IoT applications with features like WiFi management, WebSocket communication, and a modern web interface.

## Features

- **WiFi Management**
  - Automatic AP mode when no credentials are found
  - Secure credential storage
  - Automatic reconnection handling
  - Web-based WiFi configuration

- **MCP Protocol Implementation**
  - Resource discovery and management
  - Real-time updates via WebSocket
  - JSON-RPC 2.0 compliant
  - Subscription system for resource updates

- **Web Interface**
  - Modern, responsive design
  - Interactive MCP command console
  - Real-time status updates
  - Learning center with examples

## Requirements

- ESP32 development board
- PlatformIO IDE or Arduino IDE
- Required libraries:
  - ESPAsyncWebServer
  - AsyncTCP
  - ArduinoJson
  - LittleFS
  - Preferences

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/LightHouse-MCP-ESP-Server.git
   cd LightHouse-MCP-ESP-Server
   ```

2. Install dependencies:
   ```bash
   pio pkg install
   ```

3. Build and upload:
   ```bash
   pio run -t upload
   ```

4. Upload filesystem:
   ```bash
   pio run -t uploadfs
   ```

## Usage

1. Power on the ESP32
2. Connect to the ESP32's WiFi network (SSID format: ESP32_XXXXXX)
3. Open a web browser and navigate to:
   - `http://192.168.4.1` - Main interface
   - `http://192.168.4.1/mcp_basics.html` - MCP learning center

## Development

### Project Structure
```
├── data/               # Web interface files
├── src/               # Source code
├── include/           # Header files
├── test/              # Unit tests
└── platformio.ini     # Project configuration
```

### Adding New Features

1. Create new MCP resources in `include/MCPTypes.h`
2. Implement handlers in `src/MCPServer.cpp`
3. Add web interface components in `data/`

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Acknowledgments

- ESP32 Arduino Core team
- PlatformIO team
- All contributors and users

#include <queue>
#include <WiFi.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

struct NetworkRequest {
    enum class Type {
        CONNECT,
        START_AP,
        CHECK_CONNECTION
    };
    Type type;
    String message;
};

class NetworkManager {
public:
    enum class NetworkState {
        INIT,
        CONNECTING,
        CONNECTED,
        CONNECTION_FAILED,
        AP_MODE
    };

    NetworkManager();
    void begin();
    void connect();
    void checkConnection();
    bool isConnected();
    String getIPAddress();
    String getSSID();

private:
    NetworkState state;
    Preferences preferences;
    String apSSID;
    struct {
        String ssid;
        String password;
        bool valid;
    } credentials;
    AsyncWebServer server;
    AsyncWebSocket ws;
    int connectAttempts;
    unsigned long lastConnectAttempt;
    TaskHandle_t networkTaskHandle;
    void networkTask();
    void handleRequest(const NetworkRequest& request);
    void startAP();
    String generateUniqueSSID();
    bool loadCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void clearCredentials();
    String getNetworkStatusJson(NetworkState state, const String& ssid, const String& ip);
    void queueRequest(NetworkRequest::Type type, const String &message = "");
    void setupWebServer();
    void handleRoot(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
    static void networkTaskCode(void* parameter);

    // Constants
    static const int MAX_CONNECT_ATTEMPTS = 5;
    static const unsigned long CONNECT_TIMEOUT = 10000; // 10 seconds
    static const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
    static const char* SETUP_PAGE_PATH;
    std::queue<NetworkRequest> requestQueue;
}; 
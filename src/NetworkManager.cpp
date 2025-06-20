#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "NetworkManager.h"
#include <esp_random.h>

const char* NetworkManager::SETUP_PAGE_PATH = "/wifi_setup.html";

NetworkManager::NetworkManager() 
    : state(NetworkState::INIT),
      server(80),
      ws("/ws"),
      connectAttempts(0),
      lastConnectAttempt(0) {
}

void NetworkManager::begin() {
    Serial.println("\n=== Starting Network Manager ===");
    
    // Initialize LittleFS if not already initialized
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS Mount Failed - Formatting...");
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed Even After Format!");
            return;
        }
    }
    Serial.println("LittleFS mounted successfully");

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (static_cast<system_event_id_t>(event) == SYSTEM_EVENT_STA_DISCONNECTED) {
            Serial.println("WiFi disconnected event received");
            if (state == NetworkState::CONNECTED) {
                state = NetworkState::CONNECTION_FAILED;
                queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
            }
        }
    });

    // Create network task
    xTaskCreatePinnedToCore(
        networkTaskCode,
        "NetworkTask",
        8192,
        this,
        1,
        &networkTaskHandle,
        0
    );
    Serial.println("Network task created");
    
    // Check credentials and start appropriate mode
    if (loadCredentials()) {
        Serial.println("Credentials found - Attempting to connect to WiFi");
        state = NetworkState::CONNECTING;
        queueRequest(NetworkRequest::Type::CONNECT);
    } else {
        Serial.println("No credentials found - Starting AP mode");
        state = NetworkState::AP_MODE;
        queueRequest(NetworkRequest::Type::START_AP);
    }
    
    Serial.println("=== Network Manager Started ===\n");
}

void NetworkManager::setupWebServer() {
    Serial.println("Setting up web server...");
    
    ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server.addHandler(&ws);

    // Serve static files
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // Handle root path
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("Received request for root page");
        this->handleRoot(request);
    });
    
    // Handle save credentials
    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("Received save credentials request");
        this->handleSave(request);
    });
    
    // Handle status
    server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleStatus(request);
    });

    server.begin();
    Serial.println("Web server started");
}

void NetworkManager::handleRoot(AsyncWebServerRequest *request) {
    Serial.println("\n=== Handling Root Request ===");
    Serial.print("Current state: ");
    switch (state) {
        case NetworkState::INIT:
            Serial.println("INIT");
            break;
        case NetworkState::CONNECTING:
            Serial.println("CONNECTING");
            break;
        case NetworkState::CONNECTED:
            Serial.println("CONNECTED");
            break;
        case NetworkState::AP_MODE:
            Serial.println("AP_MODE");
            break;
        case NetworkState::CONNECTION_FAILED:
            Serial.println("CONNECTION_FAILED");
            break;
    }

    if (state == NetworkState::AP_MODE) {
        Serial.println("Serving WiFi setup page");
        if (LittleFS.exists(SETUP_PAGE_PATH)) {
            request->send(LittleFS, SETUP_PAGE_PATH, "text/html");
        } else {
            Serial.println("ERROR: Setup page not found in filesystem!");
            request->send(500, "text/plain", "Setup page not found in filesystem");
        }
    } else {
        Serial.println("Serving main page");
        request->send(LittleFS, "/index.html", "text/html");
    }
    Serial.println("=== End Root Request ===\n");
}

void NetworkManager::handleSave(AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
        request->send(400, "text/plain", "Missing parameters");
        return;
    }
    
    String ssid = request->getParam("ssid", true)->value();
    String password = request->getParam("password", true)->value();
    
    if (ssid.isEmpty()) {
        request->send(400, "text/plain", "SSID cannot be empty");
        return;
    }
    
    saveCredentials(ssid, password);
    request->send(200, "text/plain", "Credentials saved");
}

void NetworkManager::handleStatus(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print(getNetworkStatusJson(state, getSSID(), getIPAddress()));
    request->send(response);
}

void NetworkManager::onWebSocketEvent(AsyncWebSocket* server, 
                                    AsyncWebSocketClient* client,
                                    AwsEventType type, 
                                    void* arg, 
                                    uint8_t* data, 
                                    size_t len) {
    Serial.print("onWebSocketEvent type =");    
    Serial.println(type);                                    
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.println("WebSocket client connected");
            //client->text(getNetworkStatusJson(state, getSSID(), getIPAddress()));
            break;
        case WS_EVT_DISCONNECT:
            Serial.println("WebSocket client disconnected");
            break;
        case WS_EVT_ERROR:
            Serial.println("WebSocket error");
            break;
        case WS_EVT_DATA:
            Serial.println("WebSocket data received");
            if (len > 0) {
                // 将接收到的数据转换为字符串
                String message = String((char*)data, len);
                Serial.print("Received message: ");
                Serial.println(message);
                
                // 解析JSON消息
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, message);
                
                if (error) {
                    Serial.print("JSON parsing failed: ");
                    Serial.println(error.c_str());
                    return;
                }
                
                // 处理不同类型的消息
                if (doc.containsKey("method")) {
                    String method = doc["method"].as<String>();
                    Serial.print("Method: ");
                    Serial.println(method);
                    
                    // 处理初始化请求
                    if (method == "initialize") {
                        DynamicJsonDocument response(1024);
                        response["jsonrpc"] = "2.0";
                        response["id"] = doc["id"];

                        // 固定返回新协议格式
                        response["result"]["protocolVersion"] = "2025-03-26";
                        JsonObject tools = response["result"]["capabilities"].createNestedObject("tools");
                        JsonObject tool1 = tools.createNestedObject("bedroom_light_on");
                        tool1["description"] = "打开卧室灯";
                        JsonObject tool2 = tools.createNestedObject("bedroom_light_off");
                        tool2["description"] = "关闭卧室灯";
                        JsonObject serverInfo = response["result"].createNestedObject("serverInfo");
                        serverInfo["name"] = "ESP32-MCP-Server";
                        serverInfo["version"] = "1.0.0";

                        String responseStr;
                        serializeJson(response, responseStr);
                        client->text(responseStr);
                    }
                    // 处理资源列表请求
                    else if (method == "resources/list") {
                        DynamicJsonDocument response(1024);
                        response["jsonrpc"] = "2.0";
                        response["id"] = doc["id"];
                        JsonArray resources = response["result"]["resources"].to<JsonArray>();
                        
                        // 添加示例资源
                        JsonObject resource1 = resources.createNestedObject();
                        resource1["name"] = "LED";
                        resource1["uri"] = "led://status";
                        resource1["type"] = "boolean";
                        
                        String responseStr;
                        serializeJson(response, responseStr);
                        client->text(responseStr);
                    }
                    // 处理notifications/initialized通知
                    else if (method == "notifications/initialized") {
                        Serial.println("收到 notifications/initialized 通知");
                        // 不需要回复
                    }
                    // 处理tools/list请求
                    else if (method == "tools/list") {
                        DynamicJsonDocument response(1024);
                        response["jsonrpc"] = "2.0";
                        response["id"] = doc["id"];
                        JsonObject result = response.createNestedObject("result");
                        JsonArray tools = result.createNestedArray("tools");

                        // bedroom_light_on
                        JsonObject tool1 = tools.createNestedObject();
                        tool1["name"] = "bedroom_light_on";
                        tool1["description"] = "打开卧室灯";
                        JsonObject schema1 = tool1.createNestedObject("inputSchema");
                        schema1["type"] = "object";
                        schema1.createNestedObject("properties");
                        schema1.createNestedArray("required");

                        // bedroom_light_off
                        JsonObject tool2 = tools.createNestedObject();
                        tool2["name"] = "bedroom_light_off";
                        tool2["description"] = "关闭卧室灯";
                        JsonObject schema2 = tool2.createNestedObject("inputSchema");
                        schema2["type"] = "object";
                        schema2.createNestedObject("properties");
                        schema2.createNestedArray("required");

                        String responseStr;
                        serializeJson(response, responseStr);
                        client->text(responseStr);
                    }
                    // 处理tools/call请求
                    else if (method == "tools/call") {
                        String toolName = doc["params"]["name"].as<String>();
                        DynamicJsonDocument resp(256);
                        resp["jsonrpc"] = "2.0";
                        resp["id"] = doc["id"];
                        JsonObject result = resp.createNestedObject("result");
                        JsonArray contentArr = result.createNestedArray("content");
                        JsonObject contentObj = contentArr.createNestedObject();
                        contentObj["type"] = "text";

                        if (toolName == "bedroom_light_on") {
                            digitalWrite(2, HIGH);
                            contentObj["text"] = "true";
                        } else if (toolName == "bedroom_light_off") {
                            digitalWrite(2, LOW);
                            contentObj["text"] = "true";
                        } else {
                            contentObj["text"] = "false";
                            result["isError"] = true;
                        }
                        result["isError"] = false;
                        String respStr;
                        serializeJson(resp, respStr);
                        client->text(respStr);
                    }
                }
            }
            break;
    }
}

void NetworkManager::networkTaskCode(void* parameter) {
    NetworkManager* manager = static_cast<NetworkManager*>(parameter);
    manager->networkTask();
}

void NetworkManager::networkTask() {
    NetworkRequest request;
    TickType_t lastCheck = xTaskGetTickCount();
    
    while (true) {
        // Process any pending requests
        if (!requestQueue.empty()) {
            request = requestQueue.front();
            requestQueue.pop();
            handleRequest(request);
        }
        
        // Periodic connection check
        if (state == NetworkState::CONNECTED && 
            (xTaskGetTickCount() - lastCheck) >= pdMS_TO_TICKS(RECONNECT_INTERVAL)) {
            queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
            lastCheck = xTaskGetTickCount();
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NetworkManager::handleRequest(const NetworkRequest& request) {
    Serial.printf("\n=== Handling Request Type: %d ===\n", static_cast<int>(request.type));
    switch (request.type) {
        case NetworkRequest::Type::CONNECT:
            Serial.println("Processing CONNECT request");
            connect();
            break;
        case NetworkRequest::Type::START_AP:
            Serial.println("Processing START_AP request");
            startAP();
            break;
        case NetworkRequest::Type::CHECK_CONNECTION:
            Serial.println("Processing CHECK_CONNECTION request");
            checkConnection();
            break;
    }
    Serial.println("=== Request Handled ===\n");
}

void NetworkManager::connect() {
    Serial.println("\n=== Starting WiFi Connection ===");
    
    // Check if credentials are valid
    if (!credentials.valid) {
        Serial.println("ERROR: Invalid credentials - Starting AP mode");
        startAP();
        return;
    }

    // Check if we've exceeded max connection attempts
    if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
        Serial.printf("ERROR: Max connection attempts (%d) reached - Starting AP mode\n", MAX_CONNECT_ATTEMPTS);
        startAP();
        return;
    }

    Serial.print("Attempting to connect to WiFi: ");
    Serial.println(credentials.ssid);
    Serial.printf("Connection attempt %d of %d\n", connectAttempts + 1, MAX_CONNECT_ATTEMPTS);
    
    state = NetworkState::CONNECTING;
    WiFi.mode(WIFI_STA);
    WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
    
    connectAttempts++;
    lastConnectAttempt = millis();

    // Schedule a connection check
    queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
    Serial.println("=== WiFi Connection Initiated ===\n");
}

void NetworkManager::checkConnection() {
    Serial.println("\n=== Checking Connection Status ===");
    if (state == NetworkState::CONNECTING) {
        wl_status_t status = WiFi.status();
        Serial.printf("WiFi Status: %d (", status);
        switch (status) {
            case WL_IDLE_STATUS: Serial.println("IDLE)"); break;
            case WL_NO_SSID_AVAIL: Serial.println("NO SSID AVAILABLE)"); break;
            case WL_CONNECT_FAILED: Serial.println("CONNECT FAILED)"); break;
            case WL_CONNECTION_LOST: Serial.println("CONNECTION LOST)"); break;
            case WL_DISCONNECTED: Serial.println("DISCONNECTED)"); break;
            case WL_CONNECTED: Serial.println("CONNECTED)"); break;
            default: Serial.println("UNKNOWN)"); break;
        }
        
        if (status == WL_CONNECTED) {
            Serial.println("WiFi Connected Successfully!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            state = NetworkState::CONNECTED;
            connectAttempts = 0;  // Reset attempts on successful connection
            setupWebServer();
            ws.textAll(getNetworkStatusJson(state, getSSID(), getIPAddress()));
        } else if (millis() - lastConnectAttempt >= CONNECT_TIMEOUT) {
            Serial.println("Connection timeout!");
            if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
                Serial.println("Max attempts reached - Starting AP mode");
                startAP();
            } else {
                Serial.printf("Retrying connection (attempt %d of %d)\n", connectAttempts + 1, MAX_CONNECT_ATTEMPTS);
                state = NetworkState::CONNECTION_FAILED;
                queueRequest(NetworkRequest::Type::CONNECT);
            }
        } else {
            Serial.println("Still connecting...");
            queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
        }
    } else if (state == NetworkState::CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost!");
            state = NetworkState::CONNECTION_FAILED;
            connectAttempts = 0;  // Reset attempts for reconnection
            queueRequest(NetworkRequest::Type::CONNECT);
        } else {
            Serial.println("Connection still active");
        }
    }
    Serial.println("=== Connection Check Complete ===\n");
}

void NetworkManager::startAP() {
    Serial.println("\n=== Starting Access Point Mode ===");
    
    state = NetworkState::AP_MODE;
    WiFi.mode(WIFI_AP);
    
    // Configure AP IP address
    IPAddress local_IP(192,168,4,1);
    IPAddress gateway(192,168,4,1);
    IPAddress subnet(255,255,255,0);
    
    Serial.println("Configuring AP with static IP...");
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("ERROR: AP Config Failed!");
        return;
    }
    
    if (apSSID.isEmpty()) {
        apSSID = generateUniqueSSID();
    }
    
    Serial.print("Starting AP with SSID: ");
    Serial.println(apSSID);
    
    if (!WiFi.softAP(apSSID.c_str())) {
        Serial.println("ERROR: AP Start Failed!");
        return;
    }
    
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Initialize web server
    Serial.println("Initializing web server...");
    setupWebServer();
    Serial.println("Web server initialized");
    
    // Notify all connected clients
    ws.textAll(getNetworkStatusJson(state, apSSID, WiFi.softAPIP().toString()));
    Serial.println("=== AP Mode Ready ===\n");
}

String NetworkManager::generateUniqueSSID() {
    uint32_t chipId = (uint32_t)esp_random();
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32_%08X", chipId);
    return String(ssid);
}

bool NetworkManager::loadCredentials() {
    Serial.println("\n=== Loading WiFi Credentials ===");
    
    preferences.begin("network", true);
    credentials.ssid = preferences.getString("ssid", "");
    credentials.password = preferences.getString("pass", "");
    preferences.end();
    
    Serial.print("SSID: ");
    Serial.println(credentials.ssid.isEmpty() ? "Not found" : credentials.ssid);
    Serial.print("Password: ");
    Serial.println(credentials.password.isEmpty() ? "Not found" : "********");
    
    credentials.valid = !credentials.ssid.isEmpty();
    Serial.print("Credentials valid: ");
    Serial.println(credentials.valid ? "Yes" : "No");
    
    if (!credentials.valid) {
        Serial.println("No valid credentials found - Will start in AP mode");
    }
    
    Serial.println("==============================\n");
    return credentials.valid;
}

void NetworkManager::saveCredentials(const String& ssid, const String& password) {
    Serial.println("\n=== Saving WiFi Credentials ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.println("Password: ********");
    
    preferences.begin("network", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    
    credentials.ssid = ssid;
    credentials.password = password;
    credentials.valid = true;
    
    connectAttempts = 0;
    Serial.println("Credentials saved successfully");
    Serial.println("Initiating connection...");
    queueRequest(NetworkRequest::Type::CONNECT);
    Serial.println("=== Credentials Saved ===\n");
}

void NetworkManager::clearCredentials() {
    Serial.println("\n=== Clearing WiFi Credentials ===");
    preferences.begin("network", false);
    preferences.clear();
    preferences.end();
    
    credentials.ssid = "";
    credentials.password = "";
    credentials.valid = false;
    Serial.println("Credentials cleared successfully");
    Serial.println("=== Credentials Cleared ===\n");
}

String NetworkManager::getNetworkStatusJson(NetworkState state, const String& ssid, const String& ip) {
    JsonDocument doc; // Ensure you include <ArduinoJson.h>

    switch (state) {
        case NetworkState::CONNECTED:
            doc["status"] = "connected";
            break;
        case NetworkState::CONNECTING:
            doc["status"] = "connecting";
            break;
        case NetworkState::AP_MODE:
            doc["status"] = "ap_mode";
            break;
        case NetworkState::CONNECTION_FAILED:
            doc["status"] = "connection_failed";
            break;
        default:
            doc["status"] = "initializing";
    }

    doc["ssid"] = ssid;
    doc["ip"] = ip;

    String response;
    serializeJson(doc, response);
    return response;
}

bool NetworkManager::isConnected() {
    return state == NetworkState::CONNECTED && WiFi.status() == WL_CONNECTED;
}

String NetworkManager::getIPAddress() {
    return state == NetworkState::AP_MODE ? 
           WiFi.softAPIP().toString() : 
           WiFi.localIP().toString();
}

String NetworkManager::getSSID() {
    return state == NetworkState::AP_MODE ? 
           apSSID : 
           credentials.ssid;
}

void NetworkManager::queueRequest(NetworkRequest::Type type, const String &message) {
    requestQueue.push({type, message});
}
#include "MCPServer.h"
#include <ArduinoJson.h>

namespace mcp {

MCPServer::MCPServer(uint16_t port) : server(80), initialized(false) {
    webSocket = new WebSocketsServer(port);
}

void MCPServer::begin(bool isNetworkConnected) {
    setupEndpoints();
    
    webSocket->onEvent([this](uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                Serial.printf("[WSc] Client #%u connected\n", client);
                break;
            case WStype_DISCONNECTED:
                Serial.printf("[WSc] Client #%u disconnected\n", client);
                subscriptions.erase(client);
                break;
            case WStype_TEXT: {
                std::string message(reinterpret_cast<char*>(payload), length);
                handleWebSocketMessage(client, message);
                break;
            }
            case WStype_ERROR:
                Serial.printf("[WSc] Client #%u error\n", client);
                break;
        }
    });
    
    webSocket->begin();
    server.begin();
    initialized = true;
}

void MCPServer::setupEndpoints() {
    server.on("/mcp", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("plain", true)) {
            auto* response = request->beginResponseStream("application/json");
            String message = request->getParam("plain", true)->value();
            
            MCPRequest mcpRequest = parseRequest(message.c_str());
            handleRequest(mcpRequest);
            
            request->send(response);
        } else {
            request->send(400, "text/plain", "Missing request body");
        }
    });
}

void MCPServer::handleClient() {
    if (!initialized) return;
    webSocket->loop();
    
    MCPRequest request;
    while (requestQueue.pop(request)) {
        handleRequest(request);
    }
}

void MCPServer::handleRequest(const MCPRequest& request) {
    switch (request.type) {
        case MCPRequestType::INITIALIZE:
            // Handle initialization request
            break;
        case MCPRequestType::RESOURCE_LIST:
            // Handle resource listing request
            break;
        case MCPRequestType::RESOURCE_READ:
            // Handle resource reading request
            break;
        case MCPRequestType::SUBSCRIBE:
            // Handle subscription request
            break;
        case MCPRequestType::UNSUBSCRIBE:
            // Handle unsubscribe request
            break;
    }
}

void MCPServer::handleWebSocketMessage(uint8_t client, const std::string& message) {
    MCPRequest request = parseRequest(message);
    
    switch (request.type) {
        case MCPRequestType::INITIALIZE:
            handleInitialize(client, request.id, request.params);
            break;
            
        case MCPRequestType::RESOURCE_LIST:
            handleResourcesList(client, request.id, request.params);
            break;
            
        case MCPRequestType::RESOURCE_READ:
            handleResourceRead(client, request.id, request.params);
            break;
            
        case MCPRequestType::SUBSCRIBE:
            handleSubscribe(client, request.id, request.params);
            break;
            
        case MCPRequestType::UNSUBSCRIBE:
            handleUnsubscribe(client, request.id, request.params);
            break;
            
        default:
            sendError(client, request.id, -32601, "Method not found");
            break;
    }
}

void MCPServer::handleInitialize(uint8_t client, const RequestId& id, const JsonObject& params) {
    StaticJsonDocument<512> doc;
    JsonObject result = doc.to<JsonObject>();
    
    result["protocolVersion"] = "0.1.0";
    
    JsonObject serverInfoObj = result.createNestedObject("serverInfo");
    serverInfoObj["name"] = serverInfo.name;
    serverInfoObj["version"] = serverInfo.version;
    
    JsonObject capsObj = result.createNestedObject("capabilities");
    JsonObject resourcesCaps = capsObj.createNestedObject("resources");
    resourcesCaps["listChanged"] = capabilities.resources.listChanged;
    resourcesCaps["subscribe"] = capabilities.resources.subscribe;
    
    JsonObject toolsCaps = capsObj.createNestedObject("tools");
    toolsCaps["listChanged"] = capabilities.tools.listChanged;
    
    MCPResponse response{true, "Initialized", result};
    sendResponse(client, id, response);
}

void MCPServer::handleResourcesList(uint8_t client, const RequestId& id, const JsonObject& params) {
    StaticJsonDocument<1024> doc;
    JsonArray resourcesArray = doc.createNestedArray("resources");
    
    for (const auto& [uri, resource] : resources) {
        JsonObject resObj = resourcesArray.createNestedObject();
        resObj["name"] = resource.name;
        resObj["uri"] = resource.uri;
        if (!resource.mimeType.empty()) resObj["mimeType"] = resource.mimeType;
        if (!resource.description.empty()) resObj["description"] = resource.description;
    }
    
    MCPResponse response{true, "Success", doc.as<JsonVariant>()};
    sendResponse(client, id, response);
}

void MCPServer::handleResourceRead(uint8_t client, const RequestId& id, const JsonObject& params) {
    if (!params.containsKey("uri")) {
        sendError(client, id, -32602, "Missing uri parameter");
        return;
    }
    
    std::string uri = params["uri"].as<const char*>();
    auto it = resources.find(uri);
    
    if (it == resources.end()) {
        sendError(client, id, -32602, "Resource not found");
        return;
    }
    
    // Create response with resource contents
    StaticJsonDocument<1024> doc;
    JsonArray contents = doc.createNestedArray("contents");
    JsonObject content = contents.createNestedObject();
    
    // TODO: Add actual resource content handling
    content["uri"] = uri;
    content["mimeType"] = it->second.mimeType;
    
    MCPResponse response{true, "Success", doc.as<JsonVariant>()};
    sendResponse(client, id, response);
}

void MCPServer::handleSubscribe(uint8_t client, const RequestId& id, const JsonObject& params) {
    if (!params.containsKey("uri")) {
        sendError(client, id, -32602, "Missing uri parameter");
        return;
    }
    
    std::string uri = params["uri"].as<const char*>();
    if (resources.find(uri) == resources.end()) {
        sendError(client, id, -32602, "Resource not found");
        return;
    }
    
    subscriptions[client].push_back(uri);
    
    MCPResponse response{true, "Subscribed", JsonVariant()};
    sendResponse(client, id, response);
}

void MCPServer::handleUnsubscribe(uint8_t client, const RequestId& id, const JsonObject& params) {
    if (!params.containsKey("uri")) {
        sendError(client, id, -32602, "Missing uri parameter");
        return;
    }
    
    std::string uri = params["uri"].as<const char*>();
    auto& clientSubs = subscriptions[client];
    auto it = std::find(clientSubs.begin(), clientSubs.end(), uri);
    
    if (it != clientSubs.end()) {
        clientSubs.erase(it);
    }
    
    MCPResponse response{true, "Unsubscribed", JsonVariant()};
    sendResponse(client, id, response);
}

void MCPServer::registerResource(const MCPResource& resource) {
    resources[resource.uri] = resource;
    broadcastResourceUpdate(resource.uri);
}

void MCPServer::unregisterResource(const std::string& uri) {
    resources.erase(uri);
    
    // Notify subscribers about resource removal
    StaticJsonDocument<256> doc;
    doc["method"] = "notifications/resources/list_changed";
    String notification;
    serializeJson(doc, notification);
    
    webSocket->broadcastTXT(notification.c_str());
}

void MCPServer::sendResponse(uint8_t client, const RequestId& id, const MCPResponse& response) {
    String jsonResponse = serializeResponse(id, response);
    webSocket->sendTXT(client, jsonResponse.c_str());
}

void MCPServer::sendError(uint8_t client, const RequestId& id, int code, const std::string& message) {
    StaticJsonDocument<256> doc;
    doc["jsonrpc"] = "2.0";
    if (std::holds_alternative<int>(id.value)) {
        doc["id"] = std::get<int>(id.value);
    } else {
        doc["id"] = std::get<std::string>(id.value);
    }
    
    JsonObject error = doc.createNestedObject("error");
    error["code"] = code;
    error["message"] = message;
    
    String response;
    serializeJson(doc, response);
    webSocket->sendTXT(client, response.c_str());
}

void MCPServer::broadcastResourceUpdate(const std::string& uri) {
    StaticJsonDocument<256> doc;
    doc["method"] = "notifications/resources/updated";
    JsonObject params = doc.createNestedObject("params");
    params["uri"] = uri;
    
    String notification;
    serializeJson(doc, notification);
    
    for (const auto& [client, uris] : subscriptions) {
        if (std::find(uris.begin(), uris.end(), uri) != uris.end()) {
            webSocket->sendTXT(client, notification.c_str());
        }
    }
}

MCPRequest MCPServer::parseRequest(const std::string& message) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        return MCPRequest{MCPRequestType::INITIALIZE, RequestId{0}, JsonObject()};
    }
    
    MCPRequest request;
    
    // Parse request ID
    if (doc["id"].is<int>()) {
        request.id.value = doc["id"].as<int>();
    } else if (doc["id"].is<const char*>()) {
        request.id.value = std::string(doc["id"].as<const char*>());
    }
    
    // Parse method
    const char* method = doc["method"].as<const char*>();
    if (strcmp(method, "initialize") == 0) {
        request.type = MCPRequestType::INITIALIZE;
    } else if (strcmp(method, "resources/list") == 0) {
        request.type = MCPRequestType::RESOURCE_LIST;
    } else if (strcmp(method, "resources/read") == 0) {
        request.type = MCPRequestType::RESOURCE_READ;
    } else if (strcmp(method, "resources/subscribe") == 0) {
        request.type = MCPRequestType::SUBSCRIBE;
    } else if (strcmp(method, "resources/unsubscribe") == 0) {
        request.type = MCPRequestType::UNSUBSCRIBE;
    }
    
    request.params = doc["params"].as<JsonObject>();
    return request;
}

std::string MCPServer::serializeResponse(const RequestId& id, const MCPResponse& response) {
    StaticJsonDocument<1024> doc;
    doc["jsonrpc"] = "2.0";
    
    if (std::holds_alternative<int>(id.value)) {
        doc["id"] = std::get<int>(id.value);
    } else {
        doc["id"] = std::get<std::string>(id.value);
    }
    
    JsonObject result = doc.createNestedObject("result");
    result["success"] = response.success;
    result["message"] = response.message;
    
    if (!response.data.isNull()) {
        result["data"] = response.data;
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr.c_str();
}

// Add to MCPServer.cpp
void MCPServer::setupEndpoints() {
    // ... existing endpoints ...

    server.on("/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleStatsRequest(request);
    });

    server.serveStatic("/", LittleFS, "/").setDefaultFile("stats.html");
}

void MCPServer::handleStatsRequest(AsyncWebServerRequest *request) {
    String period = "current";
    if (request->hasParam("period")) {
        period = request->getParam("period")->value();
    }

    bool fromBoot = period == "boot";
    bool allTime = period == "all";

    DynamicJsonDocument doc(1024);
    JsonObject root = doc.to<JsonObject>();

    // Request statistics
    JsonObject requests = root.createNestedObject("requests");
    auto totalRequests = METRICS.getMetric("system.requests.total", fromBoot);
    auto errorRequests = METRICS.getMetric("system.requests.errors", fromBoot);
    auto timeoutRequests = METRICS.getMetric("system.requests.timeouts", fromBoot);
    auto duration = METRICS.getMetric("system.requests.duration", fromBoot);

    requests["total"] = totalRequests.counter;
    requests["errors"] = errorRequests.counter;
    requests["timeouts"] = timeoutRequests.counter;
    requests["avg_duration"] = duration.histogram.count > 0 ? 
        duration.histogram.sum / duration.histogram.count : 0;
    requests["max_duration"] = duration.histogram.max;

    // System status
    JsonObject system = root.createNestedObject("system");
    auto wifiSignal = METRICS.getMetric("system.wifi.signal", true); // Always current
    auto freeHeap = METRICS.getMetric("system.heap.free", true);    // Always current
    auto minHeap = METRICS.getMetric("system.heap.min", fromBoot);

    system["wifi_signal"] = wifiSignal.gauge;
    system["free_heap"] = freeHeap.gauge;
    system["min_heap"] = minHeap.gauge;
    system["uptime"] = millis();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

} // namespace mcp
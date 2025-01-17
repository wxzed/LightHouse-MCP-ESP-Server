#pragma once

#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <vector>
#include <functional>
#include "MCPTypes.h"
#include "RequestQueue.h"

namespace mcp {

class MCPServer {
public:
    MCPServer(uint16_t port = 9000);
    void begin(bool isNetworkConnected);
    void handleClient();
    
    // Resource registration
    void registerResource(const MCPResource& resource);
    void unregisterResource(const std::string& uri);

private:
    WebSocketsServer* webSocket;
    AsyncWebServer server;
    RequestQueue<MCPRequest> requestQueue;
    std::map<std::string, MCPResource> resources;
    std::map<uint8_t, std::vector<std::string>> subscriptions;
    Implementation serverInfo{"esp32-mcp-server", "1.0.0"};
    ServerCapabilities capabilities;
    bool initialized;
    
    void setupEndpoints();
    void handleRequest(const MCPRequest& request);
    void handleWebSocketMessage(uint8_t client, const std::string& message);
    
    // MCP protocol handlers
    void handleInitialize(uint8_t client, const RequestId& id, const JsonObject& params);
    void handleResourcesList(uint8_t client, const RequestId& id, const JsonObject& params);
    void handleResourceRead(uint8_t client, const RequestId& id, const JsonObject& params);
    void handleSubscribe(uint8_t client, const RequestId& id, const JsonObject& params);
    void handleUnsubscribe(uint8_t client, const RequestId& id, const JsonObject& params);
    
    // Helper functions
    void sendResponse(uint8_t client, const RequestId& id, const MCPResponse& response);
    void sendError(uint8_t client, const RequestId& id, int code, const std::string& message);
    void broadcastResourceUpdate(const std::string& uri);
    
    MCPRequest parseRequest(const std::string& message);
    std::string serializeResponse(const RequestId& id, const MCPResponse& response);

    // Add to MCPServer.h
    void handleStatsRequest(AsyncWebServerRequest *request);

};

} // namespace mcp
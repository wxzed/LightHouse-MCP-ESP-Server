#include "MCPServer.h"
#include "MCPTypes.h"
#include <iostream>

using namespace mcp;

MCPServer::MCPServer(uint16_t port) : port_(port) {}

void MCPServer::begin(bool isConnected) {
    // Initialization logic here
}

void MCPServer::handleClient() {
    // Handle client logic
}

void MCPServer::handleInitialize(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    std::cout << "收到初始化请求 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "请求参数: ";
    serializeJson(params, std::cout);
    std::cout << std::endl;

    JsonDocument doc;
    JsonObject result = doc["result"].to<JsonObject>();
    result["serverName"] = serverInfo.name;
    result["serverVersion"] = serverInfo.version;

    sendResponse(clientId, id, MCPResponse(true, "Initialized", result));
}

void MCPServer::handleResourcesList(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    std::cout << "收到资源列表请求 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "请求参数: ";
    serializeJson(params, std::cout);
    std::cout << std::endl;

    JsonDocument doc;
    JsonArray resourcesArray = doc["resources"].to<JsonArray>();

    JsonObject resObj = resourcesArray.add<JsonObject>();
    resObj["name"] = "Resource1";
    resObj["type"] = "Type1";

    sendResponse(clientId, id, MCPResponse(true, "Resources Listed", doc.as<JsonVariant>()));
}

void MCPServer::handleResourceRead(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    std::cout << "收到资源读取请求 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "请求参数: ";
    serializeJson(params, std::cout);
    std::cout << std::endl;

    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    JsonDocument doc;
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject content = contents.add<JsonObject>();
    content["data"] = "Sample Data";

    sendResponse(clientId, id, MCPResponse(true, "Resource Read", doc.as<JsonVariant>()));
}

void MCPServer::handleSubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    std::cout << "收到订阅请求 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "请求参数: ";
    serializeJson(params, std::cout);
    std::cout << std::endl;

    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    sendResponse(clientId, id, MCPResponse(true, "Subscribed", JsonVariant()));
}

void MCPServer::handleUnsubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    std::cout << "收到取消订阅请求 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "请求参数: ";
    serializeJson(params, std::cout);
    std::cout << std::endl;

    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    sendResponse(clientId, id, MCPResponse(true, "Unsubscribed", JsonVariant()));
}

void MCPServer::unregisterResource(const std::string &uri) {
    JsonDocument doc;
    JsonObject resource = doc.to<JsonObject>();
    resource["uri"] = uri;
}

void MCPServer::sendResponse(uint8_t clientId, const RequestId &id, const MCPResponse &response) {
    std::cout << "发送响应 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "响应内容: ";
    JsonDocument doc;
    doc["id"] = id;
    doc["success"] = response.success;
    doc["message"] = response.message;
    doc["data"] = response.data;
    serializeJson(doc, std::cout);
    std::cout << std::endl;

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    // Transmit response
}

void MCPServer::sendError(uint8_t clientId, const RequestId &id, int code, const std::string &message) {
    std::cout << "发送错误 - 客户端ID: " << (int)clientId << std::endl;
    std::cout << "错误代码: " << code << std::endl;
    std::cout << "错误信息: " << message << std::endl;

    JsonDocument doc;
    JsonObject error = doc["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;

    std::string jsonError;
    serializeJson(doc, jsonError);
    // Transmit error
}

void MCPServer::broadcastResourceUpdate(const std::string &uri) {
    JsonDocument doc;
    JsonObject params = doc["params"].to<JsonObject>();
    params["uri"] = uri;

    // Broadcast logic
}

MCPRequest MCPServer::parseRequest(const std::string &json) {
    std::cout << "收到原始请求数据: " << json << std::endl;

    JsonDocument doc;
    deserializeJson(doc, json);

    MCPRequest request;
    request.type = MCPRequestType::INITIALIZE;
    request.id = doc["id"];
    request.params = doc["params"].as<JsonObject>();
    return request;
}

std::string MCPServer::serializeResponse(const RequestId &id, const MCPResponse &response) {
    JsonDocument doc;
    doc["id"] = id;
    doc["success"] = response.success;
    doc["message"] = response.message;
    doc["data"] = response.data;

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    return jsonResponse;
}

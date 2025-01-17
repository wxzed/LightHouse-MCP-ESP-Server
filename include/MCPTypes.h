#pragma once

#include <string>
#include <variant>
#include <ArduinoJson.h>

namespace mcp {

// Request ID can be either an integer or string
struct RequestId {
    std::variant<int, std::string> value;
    
    RequestId(int id) : value(id) {}
    RequestId(const std::string& id) : value(id) {}
};

// Request types supported by the MCP server
enum class MCPRequestType {
    INITIALIZE,
    RESOURCE_LIST,
    RESOURCE_READ,
    SUBSCRIBE,
    UNSUBSCRIBE
};

// MCP resource definition
struct MCPResource {
    std::string name;
    std::string uri;
    std::string mimeType;
    std::string description;
    
    MCPResource(const std::string& n, const std::string& u, 
                const std::string& m = "", const std::string& d = "")
        : name(n), uri(u), mimeType(m), description(d) {}
};

// Server implementation information
struct Implementation {
    std::string name;
    std::string version;
    
    Implementation(const std::string& n, const std::string& v)
        : name(n), version(v) {}
};

// Server capabilities
struct ServerCapabilities {
    struct {
        bool listChanged = true;
        bool subscribe = true;
    } resources;
    
    struct {
        bool listChanged = false;
    } tools;
};

// MCP request structure
struct MCPRequest {
    MCPRequestType type;
    RequestId id;
    JsonObject params;
};

// MCP response structure
struct MCPResponse {
    bool success;
    std::string message;
    JsonVariant data;
};

} // namespace mcp
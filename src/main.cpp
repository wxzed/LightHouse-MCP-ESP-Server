#include <Arduino.h>
#include <LittleFS.h>
#include "NetworkManager.h"
#include "MCPServer.h"

using namespace mcp;
// Global instances
NetworkManager networkManager;
MCPServer mcpServer;

// Task handles
TaskHandle_t mcpTaskHandle = nullptr;

// MCP task function
void mcpTask(void* parameter) {
    while (true) {
        mcpServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== ESP32 MCP Server Starting ===");
    Serial.println("Initializing...");
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    // Initialize LittleFS
    if (LittleFS.begin()) {
        Serial.println("LittleFS mounted successfully");
    } else {
        Serial.println("LittleFS mount failed!");
    }

    // Initialize network
    Serial.println("Starting network manager...");
    networkManager.begin();

    // Wait for network connection
    Serial.println("Waiting for network connection...");
    while (!networkManager.isConnected()) {
        delay(100);
    }

    Serial.println("\n=== Network Status ===");
    Serial.print("Connected to: ");
    Serial.println(networkManager.getSSID());
    Serial.print("IP Address: ");
    Serial.println(networkManager.getIPAddress());
    Serial.println("=====================\n");

    // Start MCP server
    Serial.println("Starting MCP server...");
    mcpServer.begin(networkManager.isConnected());

    // Create MCP task
    Serial.println("Creating MCP task...");
    xTaskCreatePinnedToCore(
        mcpTask,
        "MCPTask",
        8192,
        nullptr,
        1,
        &mcpTaskHandle,
        1  // Run on core 1
    );
    Serial.println("Setup complete!");
}

void loop() {
    // Handle MCP server
    mcpServer.handleClient();
}
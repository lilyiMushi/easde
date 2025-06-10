#include "MinecraftAI.h"
#include <sstream>
#include <thread>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

WebGUI::WebGUI(MinecraftAI* ai) : aiInstance(ai) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebGUI::~WebGUI() {
    Stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void WebGUI::Start() {
    if (running) return;
    
    running = true;
    serverThread = std::thread(&WebGUI::RunServer, this);
    std::cout << "Web GUI started on http://localhost:8080" << std::endl;
}

void WebGUI::Stop() {
    running = false;
    if (serverThread.joinable()) {
        serverThread.join();
    }
    std::cout << "Web GUI stopped" << std::endl;
}

void WebGUI::RunServer() {
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    
    // Set socket options to reuse address
    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket" << std::endl;
        closesocket(serverSocket);
        return;
    }
    
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on socket" << std::endl;
        closesocket(serverSocket);
        return;
    }
    
    while (running) {
        sockaddr_in clientAddr;
#ifdef _WIN32
        int clientAddrLen = sizeof(clientAddr);
#else
        socklen_t clientAddrLen = sizeof(clientAddr);
#endif
        
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket != INVALID_SOCKET) {
            std::thread clientThread(&WebGUI::HandleClient, this, clientSocket);
            clientThread.detach();
        }
    }
    
    closesocket(serverSocket);
}

void WebGUI::HandleClient(int clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        std::string response = HandleRequest(request);
        
        send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
    }
    
    closesocket(clientSocket);
}

std::string WebGUI::HandleRequest(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, httpVersion;
    iss >> method >> path >> httpVersion;
    
    if (method == "GET") {
        return HandleGetRequest(path);
    } else if (method == "POST") {
        // Extract body from POST request
        size_t bodyStart = request.find("\r\n\r\n");
        std::string body;
        if (bodyStart != std::string::npos) {
            body = request.substr(bodyStart + 4);
        }
        return HandlePostRequest(path, body);
    }
    
    return CreateHttpResponse(404, "text/plain", "Not Found");
}

std::string WebGUI::HandleGetRequest(const std::string& path) {
    if (path == "/" || path == "/index.html") {
        return CreateHttpResponse(200, "text/html", ServeStaticFile("web_gui.html"));
    } else if (path == "/api/config") {
        AIConfig config = aiInstance->GetConfig();
        return CreateJsonResponse(ConfigToJson(config));
    } else if (path == "/api/stats") {
        AIStats stats = aiInstance->GetStatistics();
        return CreateJsonResponse(StatsToJson(stats));
    } else if (path == "/api/status") {
        return CreateJsonResponse(GetStatusJson());
    }
    
    return CreateHttpResponse(404, "text/plain", "Not Found");
}

std::string WebGUI::HandlePostRequest(const std::string& path, const std::string& body) {
    try {
        Json::Value requestData;
        Json::Reader reader;
        
        if (!reader.parse(body, requestData)) {
            return CreateHttpResponse(400, "application/json", "{\"success\": false, \"error\": \"Invalid JSON\"}");
        }
        
        if (path == "/api/start") {
            aiInstance->Start();
            return CreateJsonResponse(Json::Value("{\"success\": true}"));
        } else if (path == "/api/stop") {
            aiInstance->Stop();
            return CreateJsonResponse(Json::Value("{\"success\": true}"));
        } else if (path == "/api/pause") {
            aiInstance->Pause();
            return CreateJsonResponse(Json::Value("{\"success\": true, \"paused\": true}"));
        } else if (path == "/api/settings/update") {
            if (requestData.isMember("setting") && requestData.isMember("value")) {
                UpdateSingleSetting(requestData["setting"].asString(), requestData["value"]);
                return CreateJsonResponse(Json::Value("{\"success\": true}"));
            }
        } else if (path == "/api/players/add") {
            if (requestData.isMember("playerName")) {
                aiInstance->AddKnownPlayer(requestData["playerName"].asString());
                return CreateJsonResponse(Json::Value("{\"success\": true}"));
            }
        } else if (path == "/api/players/remove") {
            if (requestData.isMember("playerName")) {
                aiInstance->RemoveKnownPlayer(requestData["playerName"].asString());
                return CreateJsonResponse(Json::Value("{\"success\": true}"));
            }
        }
        
        return CreateHttpResponse(404, "application/json", "{\"success\": false, \"error\": \"Endpoint not found\"}");
        
    } catch (const std::exception& e) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        return CreateJsonResponse(errorResponse);
    }
}

void WebGUI::UpdateSingleSetting(const std::string& setting, const Json::Value& value) {
    AIConfig config = aiInstance->GetConfig();
    
    if (setting == "mouseSensitivity") {
        config.mouseSensitivity = value.asDouble();
    } else if (setting == "miningSpeed") {
        config.miningSpeed = value.asInt();
    } else if (setting == "humanizationLevel") {
        config.humanizationLevel = value.asInt();
    } else if (setting == "pauseOnPlayer") {
        config.pauseOnPlayer = value.asBool();
    } else if (setting == "chatResponses") {
        config.chatResponses = value.asBool();
    } else if (setting == "avoidBedrock") {
        config.avoidBedrock = value.asBool();
    } else if (setting == "autoSwitchTools") {
        config.autoSwitchTools = value.asBool();
    }
    
    aiInstance->UpdateConfig(config);
}

std::string WebGUI::ServeStaticFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "<!DOCTYPE html><html><head><title>File Not Found</title></head><body><h1>Web GUI file not found</h1><p>Please ensure " + filename + " exists in the application directory.</p></body></html>";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

std::string WebGUI::CreateHttpResponse(int statusCode, const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode;
    
    switch (statusCode) {
        case 200: response << " OK"; break;
        case 400: response << " Bad Request"; break;
        case 404: response << " Not Found"; break;
        case 500: response << " Internal Server Error"; break;
        default: response << " Unknown"; break;
    }
    
    response << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}

std::string WebGUI::CreateJsonResponse(const Json::Value& json) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(json, &oss);
    
    return CreateHttpResponse(200, "application/json", oss.str());
}

Json::Value WebGUI::GetStatusJson() {
    Json::Value status;
    AIStats stats = aiInstance->GetStatistics();
    
    status["running"] = (stats.status == "Running");
    status["paused"] = stats.isPaused;
    status["uptime"] = stats.runtime;
    status["blocks_mined"] = stats.blocksMined;
    status["players_detected"] = stats.playersDetected;
    status["efficiency"] = stats.efficiency;
    
    return status;
}

Json::Value WebGUI::ConfigToJson(const AIConfig& config) {
    Json::Value json;
    
    json["mouseSensitivity"] = config.mouseSensitivity;
    json["miningRotationSpeed"] = config.miningRotationSpeed;
    json["humanizationLevel"] = config.humanizationLevel;
    json["miningSpeed"] = config.miningSpeed;
    json["reactionTime"] = config.reactionTime;
    json["detectionRadius"] = config.detectionRadius;
    json["botUsername"] = config.botUsername;
    json["miningMode"] = config.miningMode;
    json["autoSwitchTools"] = config.autoSwitchTools;
    json["avoidBedrock"] = config.avoidBedrock;
    json["chatResponses"] = config.chatResponses;
    json["pauseOnPlayer"] = config.pauseOnPlayer;
    
    Json::Value players(Json::arrayValue);
    for (const auto& player : config.knownPlayers) {
        players.append(player);
    }
    json["knownPlayers"] = players;
    
    return json;
}

AIConfig WebGUI::JsonToConfig(const Json::Value& json) {
    AIConfig config;
    
    if (json.isMember("mouseSensitivity")) config.mouseSensitivity = json["mouseSensitivity"].asDouble();
    if (json.isMember("miningRotationSpeed")) config.miningRotationSpeed = json["miningRotationSpeed"].asInt();
    if (json.isMember("humanizationLevel")) config.humanizationLevel = json["humanizationLevel"].asInt();
    if (json.isMember("miningSpeed")) config.miningSpeed = json["miningSpeed"].asInt();
    if (json.isMember("reactionTime")) config.reactionTime = json["reactionTime"].asInt();
    if (json.isMember("detectionRadius")) config.detectionRadius = json["detectionRadius"].asInt();
    if (json.isMember("botUsername")) config.botUsername = json["botUsername"].asString();
    if (json.isMember("miningMode")) config.miningMode = json["miningMode"].asString();
    if (json.isMember("autoSwitchTools")) config.autoSwitchTools = json["autoSwitchTools"].asBool();
    if (json.isMember("avoidBedrock")) config.avoidBedrock = json["avoidBedrock"].asBool();
    if (json.isMember("chatResponses")) config.chatResponses = json["chatResponses"].asBool();
    if (json.isMember("pauseOnPlayer")) config.pauseOnPlayer = json["pauseOnPlayer"].asBool();
    
    if (json.isMember("knownPlayers") && json["knownPlayers"].isArray()) {
        config.knownPlayers.clear();
        for (const auto& player : json["knownPlayers"]) {
            config.knownPlayers.push_back(player.asString());
        }
    }
    
    return config;
}

Json::Value WebGUI::StatsToJson(const AIStats& stats) {
    Json::Value json;
    
    json["blocksMined"] = stats.blocksMined;
    json["runtime"] = stats.runtime;
    json["playersDetected"] = stats.playersDetected;
    json["efficiency"] = stats.efficiency;
    json["status"] = stats.status;
    json["isPaused"] = stats.isPaused;
    
    return json;
}

void WebGUI::ProcessCommand(const std::string& command, const Json::Value& data) {
    try {
        if (command == "start") {
            aiInstance->Start();
        } else if (command == "stop") {
            aiInstance->Stop();
        } else if (command == "pause") {
            aiInstance->Pause();
        } else if (command == "updateConfig") {
            AIConfig newConfig = JsonToConfig(data);
            aiInstance->UpdateConfig(newConfig);
        } else if (command == "addPlayer") {
            if (data.isMember("playerName")) {
                aiInstance->AddKnownPlayer(data["playerName"].asString());
            }
        } else if (command == "removePlayer") {
            if (data.isMember("playerName")) {
                aiInstance->RemoveKnownPlayer(data["playerName"].asString());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing command '" << command << "': " << e.what() << std::endl;
    }
}
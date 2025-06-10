#include "MinecraftAI.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

void PrintUsage() {
    std::cout << "Minecraft AI Bot v2.0 - Usage:\n";
    std::cout << "  minecraft_ai.exe --run                 : Start the AI bot\n";
    std::cout << "  minecraft_ai.exe --gui                 : Start with GUI\n";
    std::cout << "  minecraft_ai.exe --train <video_dir>   : Train from videos\n";
    std::cout << "  minecraft_ai.exe --config              : Configure settings\n";
    std::cout << "  minecraft_ai.exe --help                : Show this help\n";
}

std::vector<std::string> GetVideoFiles(const std::string& directory) {
    std::vector<std::string> videoFiles;
    
    if (!std::filesystem::exists(directory)) {
        std::cout << "Directory does not exist: " << directory << "\n";
        return videoFiles;
    }
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".mkv") {
                videoFiles.push_back(entry.path().string());
            }
        }
    }
    
    return videoFiles;
}

void ConfigureSettings() {
    std::cout << "=== Minecraft AI Configuration ===\n";
    
    Json::Value config;
    config["mining_speed"] = 100;
    config["walking_speed"] = 100;
    config["mining_fortune"] = 0;
    config["strength"] = 0;
    config["crit_chance"] = 5.0;
    config["crit_damage"] = 50.0;
    config["humanization_level"] = 0.8;
    config["fatigue_simulation"] = true;
    config["mouse_sensitivity"] = 1.0;
    
    std::ofstream configFile("skyblock_stats.json");
    configFile << config.toStyledString();
    configFile.close();
    
    std::cout << "Configuration saved to skyblock_stats.json\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== Minecraft AI Bot v2.0 ===\n";
    std::cout << "Advanced AI with GUI Control Panel\n\n";
    
    if (argc < 2) {
        PrintUsage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "--help") {
        PrintUsage();
        return 0;
    }
    
    if (command == "--config") {
        ConfigureSettings();
        return 0;
    }
    
    // Initialize AI system
    MinecraftAI ai;
    
    if (!ai.Initialize()) {
        std::cout << "Failed to initialize AI system!\n";
        std::cout << "Make sure Minecraft is running and try again.\n";
        return 1;
    }
    
    if (command == "--gui") {
        std::cout << "Starting Minecraft AI with Web GUI...\n";
        ai.StartGUI();
        
        std::cout << "GUI is running at http://localhost:8080\n";
        std::cout << "Press Enter to stop...\n";
        std::cin.get();
        
        ai.StopGUI();
        return 0;
    }
    
    if (command == "--train") {
        if (argc < 3) {
            std::cout << "Please specify video directory for training\n";
            return 1;
        }
        
        std::string videoDir = argv[2];
        std::vector<std::string> videoFiles = GetVideoFiles(videoDir);
        
        if (videoFiles.empty()) {
            std::cout << "No video files found in: " << videoDir << "\n";
            return 1;
        }
        
        ai.TrainFromVideos(videoFiles);
        return 0;
    }
    
    if (command == "--run") {
        std::cout << "Starting Minecraft AI Bot...\n";
        
        ai.Start();
        
        std::cout << "Bot is running! Press Enter to stop...\n";
        std::cin.get();
        
        ai.Stop();
        return 0;
    }
    
    std::cout << "Unknown command: " << command << "\n";
    PrintUsage();
    return 1;
}

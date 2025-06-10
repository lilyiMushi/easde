#include "MinecraftAI.h"

SkyblockStats::SkyblockStats() {
    toolMultipliers["wooden_pickaxe"] = 1.0;
    toolMultipliers["stone_pickaxe"] = 1.5;
    toolMultipliers["iron_pickaxe"] = 2.0;
    toolMultipliers["diamond_pickaxe"] = 3.0;
    toolMultipliers["netherite_pickaxe"] = 4.0;
    toolMultipliers["efficiency_1"] = 1.3;
    toolMultipliers["efficiency_2"] = 1.69;
    toolMultipliers["efficiency_3"] = 2.197;
    toolMultipliers["efficiency_4"] = 2.856; 
    toolMultipliers["efficiency_5"] = 3.713;
}

void SkyblockStats::LoadStatsFromConfig(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        // Create default config
        Json::Value defaultConfig;
        defaultConfig["mining_speed"] = 100;
        defaultConfig["walking_speed"] = 100;
        defaultConfig["mining_fortune"] = 0;
        defaultConfig["strength"] = 0;
        defaultConfig["crit_chance"] = 5.0;
        defaultConfig["crit_damage"] = 50.0;
        
        // Add tool multipliers
        Json::Value tools;
        tools["wooden_pickaxe"] = 1.0;
        tools["stone_pickaxe"] = 1.5;
        tools["iron_pickaxe"] = 2.0;
        tools["diamond_pickaxe"] = 3.0;
        tools["netherite_pickaxe"] = 4.0;
        tools["efficiency_1"] = 1.3;
        tools["efficiency_2"] = 1.69;
        tools["efficiency_3"] = 2.197;
        tools["efficiency_4"] = 2.856;
        tools["efficiency_5"] = 3.713;
        defaultConfig["tools"] = tools;
        
        // Add block multipliers
        Json::Value blocks;
        blocks["stone"] = 1.5;
        blocks["cobblestone"] = 2.0;
        blocks["obsidian"] = 50.0;
        blocks["end_stone"] = 3.0;
        blocks["netherrack"] = 0.4;
        defaultConfig["blocks"] = blocks;
        
        std::ofstream outFile(configFile);
        if (outFile.is_open()) {
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(defaultConfig, &outFile);
            outFile.close();
        }
        return;
    }
    
    Json::Value config;
    Json::Reader reader;
    
    if (!reader.parse(file, config)) {
        std::cerr << "Failed to parse JSON config: " << reader.getFormattedErrorMessages() << std::endl;
        file.close();
        return;
    }
    
    file.close();
    
    // Load basic stats
    currentStats.miningSpeed = config.get("mining_speed", 100).asInt();
    currentStats.walkingSpeed = config.get("walking_speed", 100).asInt();
    currentStats.miningFortune = config.get("mining_fortune", 0).asInt();
    currentStats.strength = config.get("strength", 0).asInt();
    currentStats.critChance = config.get("crit_chance", 5.0).asDouble();
    currentStats.critDamage = config.get("crit_damage", 50.0).asDouble();
    
    // Load tool multipliers if available
    if (config.isMember("tools") && config["tools"].isObject()) {
        toolMultipliers.clear();
        for (const auto& toolName : config["tools"].getMemberNames()) {
            toolMultipliers[toolName] = config["tools"][toolName].asDouble();
        }
    }
    
    std::cout << "Loaded stats configuration from " << configFile << std::endl;
}

void SkyblockStats::SetMiningSpeedMultiplier(double multiplier) {
    currentStats.miningSpeed = static_cast<int>(100 * multiplier);
}

double SkyblockStats::GetMiningSpeed(const std::string& tool, const std::string& block) {
    double baseSpeed = currentStats.miningSpeed / 100.0;
    double toolMultiplier = toolMultipliers.count(tool) ? toolMultipliers[tool] : 1.0;
    
    double blockMultiplier = 1.0;
    if (block == "stone") blockMultiplier = 1.5;
    else if (block == "cobblestone") blockMultiplier = 2.0;
    else if (block == "obsidian") blockMultiplier = 50.0;
    else if (block == "end_stone") blockMultiplier = 3.0;
    else if (block == "netherrack") blockMultiplier = 0.4;
    else if (block == "bedrock") blockMultiplier = 1000.0; // Essentially unmining-able
    
    return baseSpeed * toolMultiplier / blockMultiplier;
}

double SkyblockStats::GetMovementSpeed() {
    return currentStats.walkingSpeed / 100.0;
}

void SkyblockStats::UpdateStats(const Stats& newStats) {
    currentStats = newStats;
}
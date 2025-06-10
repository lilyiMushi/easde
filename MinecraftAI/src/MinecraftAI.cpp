#include "MinecraftAI.h"

// Updated MinecraftAI Implementation with Performance Optimizations

MinecraftAI::MinecraftAI() {
    // Initialize original components
    humanizer = std::make_unique<HumanizationEngine>();
    stats = std::make_unique<SkyblockStats>();
    learner = std::make_unique<VideoLearner>();
    playerDetector = std::make_unique<PlayerDetector>();
    chatHandler = std::make_unique<ChatHandler>("MinecraftAI");
    webGui = std::make_unique<WebGUI>(this);
    
    // Initialize performance components
    perfMonitor = std::make_unique<PerformanceMonitor>();
    threadPool = std::make_unique<ThreadPool>(4); // Use 4 worker threads
    imageCache = std::make_unique<ImageProcessingCache>();
    
    // Use optimized bot instead of regular bot
    bot = std::make_unique<OptimizedMinecraftBot>(humanizer.get(), stats.get(), 
                                                  playerDetector.get(), chatHandler.get());
    
    LoadMemoryFromFile();
}

MinecraftAI::~MinecraftAI() {
    Stop();
    StopGUI();
    SaveMemoryToFile();
}

bool MinecraftAI::Initialize() {
    try {
        if (!bot->FindMinecraftWindow()) {
            std::cerr << "Critical Error: Minecraft window not found!" << std::endl;
            std::cerr << "Please ensure Minecraft is running and try again." << std::endl;
            return false;
        }
        
        // Test screen capture capability
        cv::Mat testCapture = bot->CaptureScreen();
        if (testCapture.empty()) {
            std::cerr << "Critical Error: Unable to capture screen!" << std::endl;
            std::cerr << "Check if the application has screen capture permissions." << std::endl;
            return false;
        }
        
        // Load configuration with validation
        try {
            stats->LoadStatsFromConfig("skyblock_stats.json");
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load stats config: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
        }
        
        // Test OpenCV functionality
        try {
            cv::Mat testImage = cv::Mat::zeros(100, 100, CV_8UC3);
            std::vector<cv::Rect> testDetection = bot->DetectBlocks(testImage);
            // If we get here, OpenCV is working
        } catch (const cv::Exception& e) {
            std::cerr << "Critical Error: OpenCV functionality test failed: " << e.what() << std::endl;
            return false;
        }
        
        // Initialize player detector with error checking
        try {
            playerDetector->UpdateDetection(testCapture);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Player detector initialization failed: " << e.what() << std::endl;
            std::cerr << "Player detection will be disabled." << std::endl;
            config.pauseOnPlayer = false;
        }
        
        ApplyConfigToComponents();
        
        std::cout << "✓ Minecraft AI initialized successfully!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error during initialization: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown fatal error during initialization!" << std::endl;
        return false;
    }
}

void MinecraftAI::Start() {
    if (running) return;
    
    running = true;
    paused = false;
    startTime = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(statsMutex);
    statistics.status = "Running";
    statistics.isPaused = false;
    
    // Use optimized main loop
    mainLoop = std::thread(&MinecraftAI::OptimizedMainExecutionLoop, this);
    std::cout << "Minecraft AI started with performance optimizations!" << std::endl;
}

void MinecraftAI::Stop() {
    running = false;
    if (mainLoop.joinable()) {
        mainLoop.join();
    }
    
    std::lock_guard<std::mutex> lock(statsMutex);
    statistics.status = "Stopped";
    statistics.isPaused = false;
    
    std::cout << "Minecraft AI stopped!" << std::endl;
}

void MinecraftAI::Pause() {
    paused = !paused;
    std::lock_guard<std::mutex> lock(statsMutex);
    statistics.isPaused = paused;
    std::cout << (paused ? "AI Paused" : "AI Resumed") << std::endl;
}

void MinecraftAI::Resume() {
    paused = false;
    std::lock_guard<std::mutex> lock(statsMutex);
    statistics.isPaused = false;
}

void MinecraftAI::OptimizedMainExecutionLoop() {
    int consecutiveErrors = 0;
    const int maxConsecutiveErrors = 5;
    
    while (running) {
        perfMonitor->FrameStart();
        
        try {
            if (!paused) {
                // Parallel processing of different components
                auto gameStateTask = threadPool->enqueue([this] {
                    ProcessGameState();
                });
                
                auto playerDetectionTask = threadPool->enqueue([this] {
                    if (config.pauseOnPlayer) {
                        auto state = bot->GetCurrentState();
                        if (!state.screenshot.empty()) {
                            playerDetector->UpdateDetection(state.screenshot);
                        }
                    }
                });
                
                auto chatTask = threadPool->enqueue([this] {
                    if (config.chatResponses) {
                        auto state = bot->GetCurrentState();
                        if (!state.screenshot.empty()) {
                            chatHandler->ProcessChatRegion(state.screenshot);
                        }
                    }
                });
                
                // Wait for all tasks to complete
                gameStateTask.wait();
                playerDetectionTask.wait();
                chatTask.wait();
                
                ExecuteActions();
                UpdateStatistics();
                consecutiveErrors = 0; // Reset error counter on success
            }
            
            // Adaptive frame rate based on performance
            double fps = perfMonitor->GetFPS();
            int adaptiveDelay = fps > 30 ? config.actionDelay : config.actionDelay * 2;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(adaptiveDelay));
            
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV Error in main loop: " << e.what() << std::endl;
            consecutiveErrors++;
            
            if (consecutiveErrors >= maxConsecutiveErrors) {
                std::cerr << "Too many consecutive OpenCV errors. Stopping AI." << std::endl;
                running = false;
                break;
            }
            
            // Try to recover by re-initializing camera
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
        } catch (const std::exception& e) {
            std::cerr << "Error in optimized main loop: " << e.what() << std::endl;
            consecutiveErrors++;
            
            if (consecutiveErrors >= maxConsecutiveErrors) {
                std::cerr << "Too many consecutive errors. Stopping AI." << std::endl;
                running = false;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
        } catch (...) {
            std::cerr << "Unknown error in optimized main loop!" << std::endl;
            consecutiveErrors++;
            
            if (consecutiveErrors >= maxConsecutiveErrors) {
                std::cerr << "Too many consecutive unknown errors. Stopping AI." << std::endl;
                running = false;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "Optimized main execution loop ended." << std::endl;
    std::cout << "Final performance stats: " << perfMonitor->GetFPS() << " FPS average" << std::endl;
}

void MinecraftAI::MainExecutionLoop() {
    // Fallback to original execution loop if needed
    while (running) {
        if (!paused) {
            ProcessGameState();
            ExecuteActions();
            UpdateStatistics();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config.actionDelay));
    }
}

void MinecraftAI::ProcessGameState() {
    try {
        // Capture with timeout protection
        auto captureStart = std::chrono::steady_clock::now();
        bot->CaptureGameState();
        auto captureEnd = std::chrono::steady_clock::now();
        
        auto captureDuration = std::chrono::duration_cast<std::chrono::milliseconds>(captureEnd - captureStart);
        if (captureDuration.count() > 1000) { // 1 second timeout
            throw std::runtime_error("Screen capture took too long: " + std::to_string(captureDuration.count()) + "ms");
        }
        
        // Validate captured state
        auto currentState = bot->GetCurrentState();
        if (currentState.screenshot.empty()) {
            throw std::runtime_error("Captured screenshot is empty");
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error in ProcessGameState: " + std::string(e.what()));
    }
}

void MinecraftAI::ExecuteActions() {
    auto state = bot->GetCurrentState();
    
    // Priority 1: Respond to player interactions
    if (config.pauseOnPlayer && playerDetector->IsPlayerNearby("", 3.0)) {
        bot->StopMining();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return;
    }
    
    if (config.chatResponses && chatHandler->WasMentioned()) {
        auto mentions = chatHandler->GetRecentMentions();
        for (const auto& mention : mentions) {
            std::string response = "Hello " + mention.playerName + "! I'm just mining here.";
            chatHandler->SendChatMessage(response);
        }
    }
    
    // Priority 2: Normal mining behavior
    if (!bot->isMining) {
        if (!state.detectedBlocks.empty()) {
            cv::Point2f target(static_cast<float>(state.detectedBlocks[0].x + state.detectedBlocks[0].width/2),
                             static_cast<float>(state.detectedBlocks[0].y + state.detectedBlocks[0].height/2));
            bot->StartMining(target);
        }
    } else {
        if (bot->IsBlockBroken() || (config.avoidBedrock && state.currentBlockType == "bedrock")) {
            bot->StopMining();
            bot->MoveToNextBlock();
            
            std::lock_guard<std::mutex> lock(statsMutex);
            statistics.blocksMined++;
        }
    }
}

void MinecraftAI::UpdateStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    if (running && !paused) {
        auto now = std::chrono::steady_clock::now();
        statistics.runtime = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count());
    }
    
    statistics.playersDetected = playerDetector->GetPlayerCount();
    
    // Calculate efficiency (blocks per minute)
    if (statistics.runtime > 0) {
        statistics.efficiency = (statistics.blocksMined * 60.0) / statistics.runtime;
    }
}

bool MinecraftAI::ValidateConfig(const AIConfig& newConfig) {
    try {
        // Validate numeric ranges
        if (newConfig.mouseSensitivity < 0.1 || newConfig.mouseSensitivity > 5.0) {
            throw std::runtime_error("Mouse sensitivity must be between 0.1 and 5.0");
        }
        
        if (newConfig.miningRotationSpeed < 10 || newConfig.miningRotationSpeed > 500) {
            throw std::runtime_error("Mining rotation speed must be between 10 and 500");
        }
        
        if (newConfig.humanizationLevel < 0 || newConfig.humanizationLevel > 100) {
            throw std::runtime_error("Humanization level must be between 0 and 100");
        }
        
        if (newConfig.actionDelay < 10 || newConfig.actionDelay > 5000) {
            throw std::runtime_error("Action delay must be between 10 and 5000 milliseconds");
        }
        
        if (newConfig.detectionRadius < 1 || newConfig.detectionRadius > 50) {
            throw std::runtime_error("Detection radius must be between 1 and 50 blocks");
        }
        
        // Validate string fields
        if (newConfig.botUsername.length() < 3 || newConfig.botUsername.length() > 16) {
            throw std::runtime_error("Bot username must be between 3 and 16 characters");
        }
        
        if (!std::regex_match(newConfig.botUsername, std::regex("^[a-zA-Z0-9_]+$"))) {
            throw std::runtime_error("Bot username contains invalid characters");
        }
        
        // Validate mining mode
        std::vector<std::string> validModes = {"blocks", "ores", "trees", "mixed"};
        if (std::find(validModes.begin(), validModes.end(), newConfig.miningMode) == validModes.end()) {
            throw std::runtime_error("Invalid mining mode: " + newConfig.miningMode);
        }
        
        // Validate known players
        for (const auto& player : newConfig.knownPlayers) {
            if (player.length() < 3 || player.length() > 16) {
                throw std::runtime_error("Player name '" + player + "' has invalid length");
            }
            if (!std::regex_match(player, std::regex("^[a-zA-Z0-9_]+$"))) {
                throw std::runtime_error("Player name '" + player + "' contains invalid characters");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Configuration validation failed: " << e.what() << std::endl;
        return false;
    }
}

void MinecraftAI::UpdateConfig(const AIConfig& newConfig) {
    try {
        // Validate configuration first
        if (!ValidateConfig(newConfig)) {
            throw std::runtime_error("Configuration validation failed");
        }
        
        std::lock_guard<std::mutex> lock(configMutex);
        
        // Create backup of current config
        AIConfig backupConfig = config;
        
        try {
            config = newConfig;
            ApplyConfigToComponents();
            
            std::cout << "✓ Configuration updated successfully" << std::endl;
            
        } catch (const std::exception& e) {
            // Restore backup on failure
            config = backupConfig;
            ApplyConfigToComponents();
            throw std::runtime_error("Failed to apply new configuration, restored previous: " + std::string(e.what()));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Config update error: " << e.what() << std::endl;
        throw;
    }
}

AIConfig MinecraftAI::GetConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config;
}

AIStats MinecraftAI::GetStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    return statistics;
}

void MinecraftAI::ApplyConfigToComponents() {
    humanizer->SetMouseSensitivity(config.mouseSensitivity);
    humanizer->SetRotationSpeed(config.miningRotationSpeed);
    humanizer->SetHumanizationLevel(config.humanizationLevel);
    humanizer->SetReactionTime(config.reactionTime);
    
    playerDetector->SetDetectionRadius(config.detectionRadius);
    
    chatHandler->SetBotName(config.botUsername);
    chatHandler->EnableResponses(config.chatResponses);
    
    bot->SetMiningMode(config.miningMode);
    bot->SetAutoSwitchTools(config.autoSwitchTools);
    bot->SetAvoidBedrock(config.avoidBedrock);
    bot->SetPauseOnPlayer(config.pauseOnPlayer);
    bot->SetActionDelay(config.actionDelay);
    
    stats->SetMiningSpeedMultiplier(config.miningSpeed / 100.0);
}

void MinecraftAI::AddKnownPlayer(const std::string& playerName) {
    std::lock_guard<std::mutex> lock(configMutex);
    if (std::find(config.knownPlayers.begin(), config.knownPlayers.end(), playerName) == config.knownPlayers.end()) {
        config.knownPlayers.push_back(playerName);
        playerDetector->AddKnownPlayer(playerName);
    }
}

void MinecraftAI::RemoveKnownPlayer(const std::string& playerName) {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = std::find(config.knownPlayers.begin(), config.knownPlayers.end(), playerName);
    if (it != config.knownPlayers.end()) {
        config.knownPlayers.erase(it);
        playerDetector->RemoveKnownPlayer(playerName);
    }
}

void MinecraftAI::StartGUI() {
    webGui->Start();
}

void MinecraftAI::StopGUI() {
    webGui->Stop();
}

void MinecraftAI::SaveMemoryToFile() {
    Json::Value memory;
    memory["config"] = Json::Value();
    memory["statistics"] = Json::Value();
    memory["knownPlayers"] = Json::Value(Json::arrayValue);
    
    // Save configuration
    memory["config"]["mouseSensitivity"] = config.mouseSensitivity;
    memory["config"]["miningRotationSpeed"] = config.miningRotationSpeed;
    memory["config"]["humanizationLevel"] = config.humanizationLevel;
    memory["config"]["botUsername"] = config.botUsername;
    
    // Save known players
    for (const auto& player : config.knownPlayers) {
        memory["knownPlayers"].append(player);
    }
    
    // Save statistics
    memory["statistics"]["totalBlocksMined"] = statistics.blocksMined;
    memory["statistics"]["totalRuntime"] = statistics.runtime;
    
    // Save performance metrics
    if (perfMonitor) {
        memory["performance"]["averageFPS"] = perfMonitor->GetFPS();
        memory["performance"]["averageFrameTime"] = perfMonitor->GetAverageFrameTime();
    }
    
    std::ofstream file("ai_memory.json");
    file << memory.toStyledString();
    file.close();
    
    std::cout << "AI memory saved to ai_memory.json" << std::endl;
}

void MinecraftAI::LoadMemoryFromFile() {
    std::ifstream file("ai_memory.json");
    if (!file.is_open()) {
        std::cout << "No previous memory file found, starting fresh." << std::endl;
        return;
    }
    
    Json::Value memory;
    file >> memory;
    file.close();
    
    if (memory.isMember("config")) {
        if (memory["config"].isMember("mouseSensitivity")) {
            config.mouseSensitivity = memory["config"]["mouseSensitivity"].asDouble();
        }
        if (memory["config"].isMember("miningRotationSpeed")) {
            config.miningRotationSpeed = memory["config"]["miningRotationSpeed"].asInt();
        }
        if (memory["config"].isMember("humanizationLevel")) {
            config.humanizationLevel = memory["config"]["humanizationLevel"].asInt();
        }
        if (memory["config"].isMember("botUsername")) {
            config.botUsername = memory["config"]["botUsername"].asString();
        }
    }
    
    if (memory.isMember("knownPlayers")) {
        config.knownPlayers.clear();
        for (const auto& player : memory["knownPlayers"]) {
            config.knownPlayers.push_back(player.asString());
        }
    }
    
    if (memory.isMember("statistics")) {
        if (memory["statistics"].isMember("totalBlocksMined")) {
            statistics.blocksMined = memory["statistics"]["totalBlocksMined"].asInt();
        }
    }
    
    std::cout << "AI memory loaded from ai_memory.json" << std::endl;
}

void MinecraftAI::TrainFromVideos(const std::vector<std::string>& videoPaths) {
    std::cout << "Starting AI training from " << videoPaths.size() << " videos..." << std::endl;
    
    for (const auto& videoPath : videoPaths) {
        std::cout << "Processing: " << videoPath << std::endl;
        
        if (!learner->LoadVideo(videoPath)) {
            std::cout << "Failed to load video: " << videoPath << std::endl;
            continue;
        }
        
        learner->ExtractMovementPatterns();
        learner->AnalyzeHumanBehavior();
        
        auto patterns = learner->GetLearnedPatterns();
        for (const auto& pattern : patterns) {
            humanizer->LearnFromPattern(pattern);
        }
        
        std::cout << "Extracted " << patterns.size() << " movement patterns" << std::endl;
    }
    
    std::cout << "Training complete!" << std::endl;
}
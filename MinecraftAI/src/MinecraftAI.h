#pragma once
#include <Windows.h>
#include <vector>
#include <random>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <atomic>
#include <fstream>
#include <json/json.h>
#include <regex>
#include <algorithm>
#include <sstream>
#include <mutex>
#include <iostream>
#include <cmath>
#include <queue>
#include <condition_variable>
#include <future>
#include <functional>

// Forward declarations
class MinecraftBot;
class HumanizationEngine;
class SkyblockStats;
class VideoLearner;
class PlayerDetector;
class ChatHandler;
class WebGUI;
class PerformanceMonitor;
class ThreadPool;
class ImageProcessingCache;

// Configuration structure for GUI integration
struct AIConfig {
    double mouseSensitivity = 1.0;
    int miningRotationSpeed = 100;
    int treeRotationSpeed = 80;
    int movementSpeed = 100;
    int miningSpeed = 150;
    int breakFrequency = 20;
    int actionDelay = 150;
    int humanizationLevel = 80;
    int reactionTime = 200;
    int detectionRadius = 16;
    std::string botUsername = "MinecraftAI";
    std::string miningMode = "blocks";
    bool smoothRotation = true;
    bool humanizeMovement = true;
    bool autoSwitchTools = true;
    bool avoidBedrock = true;
    bool chatResponses = true;
    bool pauseOnPlayer = true;
    std::vector<std::string> knownPlayers;
};

// Statistics structure
struct AIStats {
    int blocksMined = 0;
    int runtime = 0;
    int playersDetected = 0;
    double efficiency = 0.0;
    std::string status = "Stopped";
    bool isPaused = false;
};

// Performance monitoring class
class PerformanceMonitor {
private:
    std::chrono::steady_clock::time_point lastFrameTime;
    std::vector<double> frameTimes;
    size_t frameIndex = 0;
    static const size_t FRAME_HISTORY_SIZE = 60;
    
public:
    PerformanceMonitor();
    void FrameStart();
    double GetAverageFrameTime() const;
    double GetFPS() const;
};

// Thread pool for parallel processing
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
    
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F>
    auto enqueue(F&& f) -> std::future<typename std::result_of<F()>::type>;
};

// Memory pool for frequent allocations
template<typename T>
class ObjectPool {
private:
    std::queue<std::unique_ptr<T>> pool;
    mutable std::mutex poolMutex;
    std::function<std::unique_ptr<T>()> factory;
    
public:
    ObjectPool(std::function<std::unique_ptr<T>()> factoryFunc, size_t initialSize = 10);
    std::unique_ptr<T> acquire();
    void release(std::unique_ptr<T> obj);
    size_t size() const;
};

// Optimized image processing utilities
class ImageProcessingCache {
private:
    std::unordered_map<size_t, cv::Mat> processedImages;
    std::unordered_map<size_t, std::chrono::steady_clock::time_point> cacheTimestamps;
    mutable std::mutex cacheMutex;
    static const int CACHE_TIMEOUT_MS = 1000;
    
public:
    cv::Mat getOrProcess(const cv::Mat& input, 
                        std::function<cv::Mat(const cv::Mat&)> processor);
    
private:
    size_t computeHash(const cv::Mat& mat);
    void cleanOldEntries();
};

// Main AI controller class
class MinecraftAI {
private:
    std::unique_ptr<MinecraftBot> bot;
    std::unique_ptr<HumanizationEngine> humanizer;
    std::unique_ptr<SkyblockStats> stats;
    std::unique_ptr<VideoLearner> learner;
    std::unique_ptr<PlayerDetector> playerDetector;
    std::unique_ptr<ChatHandler> chatHandler;
    std::unique_ptr<WebGUI> webGui;
    
    // Performance components
    std::unique_ptr<PerformanceMonitor> perfMonitor;
    std::unique_ptr<ThreadPool> threadPool;
    std::unique_ptr<ImageProcessingCache> imageCache;
    
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    std::thread mainLoop;
    std::thread guiThread;
    
    AIConfig config;
    AIStats statistics;
    mutable std::mutex configMutex;
    mutable std::mutex statsMutex;
    
    std::chrono::steady_clock::time_point startTime;
    
public:
    MinecraftAI();
    ~MinecraftAI();
    
    bool Initialize();
    void Start();
    void Stop();
    void Pause();
    void Resume();
    void TrainFromVideos(const std::vector<std::string>& videoPaths);
    void AddKnownPlayer(const std::string& playerName);
    void RemoveKnownPlayer(const std::string& playerName);
    
    // GUI integration methods
    void UpdateConfig(const AIConfig& newConfig);
    AIConfig GetConfig() const;
    AIStats GetStatistics() const;
    void StartGUI();
    void StopGUI();
    
private:
    void MainExecutionLoop();
    void OptimizedMainExecutionLoop(); // New optimized version
    void ProcessGameState();
    void ExecuteActions();
    void UpdateStatistics();
    void SaveMemoryToFile();
    void LoadMemoryFromFile();
    void ApplyConfigToComponents();
    
    // Error handling
    bool ValidateConfig(const AIConfig& newConfig);
};

// Hypixel Skyblock stats system
class SkyblockStats {
public:
    struct Stats {
        int miningSpeed = 100;
        int walkingSpeed = 100;
        int miningFortune = 0;
        int strength = 0;
        double critChance = 5.0;
        double critDamage = 50.0;
    };
    
private:
    Stats currentStats;
    std::unordered_map<std::string, double> toolMultipliers;
    
public:
    SkyblockStats();
    
    void LoadStatsFromConfig(const std::string& configFile);
    double GetMiningSpeed(const std::string& tool, const std::string& block);
    double GetMovementSpeed();
    void UpdateStats(const Stats& newStats);
    void SetMiningSpeedMultiplier(double multiplier);
    Stats GetCurrentStats() const { return currentStats; }
};

// Humanization engine for natural movements
class HumanizationEngine {
public:
    struct MovementPattern {
        std::vector<cv::Point2f> mouseMovements;
        std::vector<int> timings;
        double naturalness_score;
    };
    
private:
    std::mt19937 rng;
    std::vector<MovementPattern> learnedPatterns;
    
    // Human-like timing parameters
    struct HumanTiming {
        int baseReactionTime = 150;
        int reactionVariance = 50;
        double fatigueMultiplier = 1.0;
        std::chrono::steady_clock::time_point lastAction;
    } timing;
    
    // GUI controllable parameters
    double mouseSensitivity = 1.0;
    int rotationSpeed = 100;
    int humanizationLevel = 80;
    
public:
    HumanizationEngine();
    
    void LearnFromPattern(const MovementPattern& pattern);
    cv::Point2f GenerateHumanMouseMovement(cv::Point2f start, cv::Point2f target);
    int GetHumanizedDelay(const std::string& actionType);
    void AddNaturalJitter(cv::Point2f& movement);
    double CalculateNaturalness(const MovementPattern& pattern);
    
    // GUI control methods
    void SetMouseSensitivity(double sensitivity) { mouseSensitivity = sensitivity; }
    void SetRotationSpeed(int speed) { rotationSpeed = speed; }
    void SetHumanizationLevel(int level) { humanizationLevel = level; }
    void SetReactionTime(int reactionTime) { timing.baseReactionTime = reactionTime; }
};

// Player detection and interaction system
class PlayerDetector {
public:
    struct Player {
        std::string name;
        cv::Point2f position;
        cv::Point2f lastPosition;
        double distance;
        std::chrono::steady_clock::time_point lastSeen;
        bool isMoving;
        std::string skinSignature;
    };
    
private:
    std::vector<Player> detectedPlayers;
    std::vector<std::string> knownPlayerNames;
    cv::HOGDescriptor playerHOG;
    
    double detectionRadius = 16.0;
    double responseRadius = 8.0;
    
public:
    PlayerDetector();
    
    void UpdateDetection(const cv::Mat& gameFrame);
    std::vector<Player> GetNearbyPlayers(double maxDistance = -1);
    bool IsPlayerNearby(const std::string& playerName = "", double radius = -1);
    void AddKnownPlayer(const std::string& name);
    void RemoveKnownPlayer(const std::string& name);
    Player* FindPlayerByName(const std::string& name);
    void SetDetectionRadius(double radius) { detectionRadius = radius; }
    int GetPlayerCount() const { return static_cast<int>(detectedPlayers.size()); }
    
private:
    std::vector<cv::Rect> DetectPlayerSilhouettes(const cv::Mat& frame);
    std::string ExtractPlayerName(const cv::Rect& playerRegion, const cv::Mat& frame);
    double CalculateDistance(const cv::Point2f& pos1, const cv::Point2f& pos2);
    bool IsValidPlayerDetection(const cv::Rect& region, const cv::Mat& frame);
};

// Chat system for player interaction
class ChatHandler {
public:
    struct ChatMessage {
        std::string playerName;
        std::string message;
        std::chrono::steady_clock::time_point timestamp;
        bool isWhisper;
        bool mentionsSelf;
    };
    
private:
    std::vector<ChatMessage> recentMessages;
    std::string botName;
    std::vector<std::string> responseTemplates;
    cv::Rect chatRegion;
    bool enabledResponses = true;
    
public:
    ChatHandler(const std::string& botPlayerName);
    
    void ProcessChatRegion(const cv::Mat& gameFrame);
    bool WasMentioned() const;
    std::vector<ChatMessage> GetRecentMentions(int seconds = 30);
    void SendChatMessage(const std::string& message);
    void SendWhisper(const std::string& playerName, const std::string& message);
    void SetBotName(const std::string& name) { botName = name; }
    void EnableResponses(bool enabled) { enabledResponses = enabled; }
    
private:
    std::string ExtractChatText(const cv::Mat& chatRegion);
    void ParseChatMessage(const std::string& rawText);
    std::string GenerateResponse(const ChatMessage& message);
    char RecognizeCharacter(const cv::Mat& charRegion);
    bool CheckIfMentioned(const std::string& message);
};

// Main bot controller
class MinecraftBot {
public:
    enum class ActionType {
        MINE_BLOCK,
        MOVE_TO_POSITION,
        LOOK_AROUND,
        SWITCH_TOOL,
        IDLE
    };
    
    struct GameState {
        cv::Mat screenshot;
        cv::Point2f playerPosition;
        cv::Point2f lookDirection;
        std::string currentTool;
        std::vector<cv::Rect> detectedBlocks;
        bool isBlockBroken = false;
        std::string currentBlockType;
        std::vector<PlayerDetector::Player> nearbyPlayers;
        bool shouldRespondToPlayer = false;
        std::string pendingChatResponse;
    };
    
protected: // Changed from private to protected for inheritance
    HWND minecraftWindow;
    GameState currentState;
    HumanizationEngine* humanizer;
    SkyblockStats* stats;
    PlayerDetector* playerDetector;
    ChatHandler* chatHandler;
    
    cv::Point2f currentMiningTarget;
    bool isMining = false;
    std::chrono::steady_clock::time_point miningStartTime;
    
    // GUI controllable parameters
    std::string miningMode = "blocks";
    bool autoSwitchTools = true;
    bool avoidBedrock = true;
    bool pauseOnPlayer = true;
    int actionDelayMs = 150;
    
public:
    MinecraftBot(HumanizationEngine* h, SkyblockStats* s, PlayerDetector* pd, ChatHandler* ch);
    virtual ~MinecraftBot() = default;
    
    bool FindMinecraftWindow();
    virtual void CaptureGameState();
    void ExecuteAction(ActionType action);
    void StartMining(cv::Point2f blockPosition);
    void StopMining();
    bool IsBlockBroken();
    void MoveToNextBlock();
    
    // GUI control methods
    void SetMiningMode(const std::string& mode) { miningMode = mode; }
    void SetAutoSwitchTools(bool enabled) { autoSwitchTools = enabled; }
    void SetAvoidBedrock(bool enabled) { avoidBedrock = enabled; }
    void SetPauseOnPlayer(bool enabled) { pauseOnPlayer = enabled; }
    void SetActionDelay(int delayMs) { actionDelayMs = delayMs; }
    
    GameState GetCurrentState() const { return currentState; }
    
protected: // Made protected for inheritance
    void SendMouseMove(cv::Point2f delta);
    void SendClick(bool leftClick = true);
    void SendKeyPress(int keyCode);
    cv::Mat CaptureScreen();
    std::vector<cv::Rect> DetectBlocks(const cv::Mat& image);
    std::string IdentifyBlockType(const cv::Rect& blockRegion, const cv::Mat& image);
    double CalculateMiningTime(const std::string& blockType);
};

// Enhanced MinecraftBot with performance optimizations
class OptimizedMinecraftBot : public MinecraftBot {
private:
    cv::Mat lastScreenshot;
    std::chrono::steady_clock::time_point lastCaptureTime;
    static const int CAPTURE_INTERVAL_MS = 100; // Limit to 10 FPS
    
    // ROI (Region of Interest) optimization
    cv::Rect miningROI;
    cv::Rect playerDetectionROI;
    cv::Rect chatROI;
    
    // Image processing cache
    cv::Mat processedImage;
    cv::Mat grayImage;
    std::vector<cv::Rect> cachedBlocks;
    std::chrono::steady_clock::time_point lastBlockDetection;
    
public:
    OptimizedMinecraftBot(HumanizationEngine* h, SkyblockStats* s, PlayerDetector* pd, ChatHandler* ch);
    
    void CaptureGameState() override;
    
private:
    cv::Mat CaptureOptimizedScreen();
    void UpdateROIs();
    void ProcessRelevantRegions();
    std::vector<cv::Rect> DetectBlocksOptimized(const cv::Mat& roi, cv::Point offset);
};

// Video learning system
class VideoLearner {
private:
    cv::VideoCapture videoCapture;
    std::vector<HumanizationEngine::MovementPattern> extractedPatterns;
    
public:
    bool LoadVideo(const std::string& videoPath);
    void ExtractMovementPatterns();
    void AnalyzeHumanBehavior();
    std::vector<HumanizationEngine::MovementPattern> GetLearnedPatterns() const;
    
private:
    cv::Point2f TrackMouseMovement(const cv::Mat& frame1, const cv::Mat& frame2);
    bool DetectMiningAction(const cv::Mat& frame);
    void SavePatternsToFile(const std::string& filename);
    void LoadPatternsFromFile(const std::string& filename);
    double CalculateNaturalness(const HumanizationEngine::MovementPattern& pattern);
};

// Web GUI integration
class WebGUI {
private:
    MinecraftAI* aiInstance;
    bool running = false;
    std::thread serverThread;
    
public:
    WebGUI(MinecraftAI* ai);
    ~WebGUI();
    
    void Start();
    void Stop();
    void ProcessCommand(const std::string& command, const Json::Value& data);
    
private:
    void RunServer();
    void HandleClient(int clientSocket);
    std::string HandleRequest(const std::string& request);
    std::string HandleGetRequest(const std::string& path);
    std::string HandlePostRequest(const std::string& path, const std::string& body);
    void UpdateSingleSetting(const std::string& setting, const Json::Value& value);
    std::string ServeStaticFile(const std::string& filename);
    std::string CreateHttpResponse(int statusCode, const std::string& contentType, const std::string& body);
    std::string CreateJsonResponse(const Json::Value& json);
    Json::Value GetStatusJson();
    Json::Value ConfigToJson(const AIConfig& config);
    AIConfig JsonToConfig(const Json::Value& json);
    Json::Value StatsToJson(const AIStats& stats);
};

// Template implementations for ObjectPool
template<typename T>
ObjectPool<T>::ObjectPool(std::function<std::unique_ptr<T>()> factoryFunc, size_t initialSize) 
    : factory(factoryFunc) {
    for (size_t i = 0; i < initialSize; i++) {
        pool.push(factory());
    }
}

template<typename T>
std::unique_ptr<T> ObjectPool<T>::acquire() {
    std::lock_guard<std::mutex> lock(poolMutex);
    if (pool.empty()) {
        return factory();
    }
    
    auto obj = std::move(pool.front());
    pool.pop();
    return obj;
}

template<typename T>
void ObjectPool<T>::release(std::unique_ptr<T> obj) {
    if (obj) {
        std::lock_guard<std::mutex> lock(poolMutex);
        pool.push(std::move(obj));
    }
}

template<typename T>
size_t ObjectPool<T>::size() const {
    std::lock_guard<std::mutex> lock(poolMutex);
    return pool.size();
}

// Template implementation for ThreadPool::enqueue
template<typename F>
auto ThreadPool::enqueue(F&& f) -> std::future<typename std::result_of<F()>::type> {
    using return_type = typename std::result_of<F()>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<F>(f));
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks.emplace([task]() { (*task)(); });
    }
    
    condition.notify_one();
    return result;
}
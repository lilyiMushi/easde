#include "MinecraftAI.h"

PlayerDetector::PlayerDetector() {
    playerHOG.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    
    // Load known player names from file
    std::ifstream playerFile("known_players.txt");
    std::string name;
    while (std::getline(playerFile, name)) {
        if (!name.empty()) {
            knownPlayerNames.push_back(name);
        }
    }
}

void PlayerDetector::UpdateDetection(const cv::Mat& gameFrame) {
    if (gameFrame.empty()) return;
    
    detectedPlayers.clear();
    
    std::vector<cv::Rect> playerRects = DetectPlayerSilhouettes(gameFrame);
    auto currentTime = std::chrono::steady_clock::now();
    
    for (const auto& rect : playerRects) {
        if (!IsValidPlayerDetection(rect, gameFrame)) continue;
        
        Player player;
        player.position = cv::Point2f(static_cast<float>(rect.x + rect.width/2), 
                                     static_cast<float>(rect.y + rect.height/2));
        player.name = ExtractPlayerName(rect, gameFrame);
        player.distance = CalculateDistance(player.position, 
                                          cv::Point2f(static_cast<float>(gameFrame.cols/2), 
                                                     static_cast<float>(gameFrame.rows/2)));
        player.lastSeen = currentTime;
        player.isMoving = false;
        
        // Check if this is a known player moving
        for (auto& oldPlayer : detectedPlayers) {
            if (player.name == oldPlayer.name || 
                CalculateDistance(player.position, oldPlayer.position) < 50) {
                player.isMoving = CalculateDistance(player.position, oldPlayer.lastPosition) > 5;
                oldPlayer.lastPosition = oldPlayer.position;
                break;
            }
        }
        
        detectedPlayers.push_back(player);
    }
}

std::vector<cv::Rect> PlayerDetector::DetectPlayerSilhouettes(const cv::Mat& frame) {
    std::vector<cv::Rect> foundLocations;
    
    cv::Mat grayFrame;
    cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
    
    // HOG detection for player-like shapes
    std::vector<cv::Rect> hogDetections;
    playerHOG.detectMultiScale(grayFrame, hogDetections, 0, cv::Size(8, 8), 
                              cv::Size(32, 32), 1.05, 2, false);
    
    foundLocations.insert(foundLocations.end(), hogDetections.begin(), hogDetections.end());
    
    // Color-based detection for Minecraft skins
    cv::Mat hsvFrame;
    cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);
    
    // Detect skin-like colors
    cv::Mat skinMask;
    cv::Scalar lowerSkin(0, 20, 70);
    cv::Scalar upperSkin(20, 255, 255);
    cv::inRange(hsvFrame, lowerSkin, upperSkin, skinMask);
    
    // Find contours in skin mask
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    for (const auto& contour : contours) {
        cv::Rect boundingRect = cv::boundingRect(contour);
        
        // Filter by size (typical player size in Minecraft)
        if (boundingRect.width > 20 && boundingRect.width < 100 &&
            boundingRect.height > 30 && boundingRect.height < 150) {
            
            double aspectRatio = static_cast<double>(boundingRect.height) / boundingRect.width;
            // Minecraft players have roughly 2:1 height to width ratio
            if (aspectRatio > 1.5 && aspectRatio < 3.0) {
                foundLocations.push_back(boundingRect);
            }
        }
    }
    
    return foundLocations;
}

std::string PlayerDetector::ExtractPlayerName(const cv::Rect& playerRegion, const cv::Mat& frame) {
    // Look for nameplate above player
    cv::Rect nameplateRegion(playerRegion.x - 20, 
                            std::max(0, playerRegion.y - 40),
                            playerRegion.width + 40, 
                            30);
    
    if (nameplateRegion.y + nameplateRegion.height >= frame.rows ||
        nameplateRegion.x + nameplateRegion.width >= frame.cols ||
        nameplateRegion.x < 0 || nameplateRegion.y < 0) {
        return "Unknown";
    }
    
    // Simple pattern matching with known players
    for (const auto& knownName : knownPlayerNames) {
        // In a full implementation, would use OCR here
        return knownName; // Simplified - return first known player
    }
    
    return "Player";
}

std::vector<PlayerDetector::Player> PlayerDetector::GetNearbyPlayers(double maxDistance) {
    if (maxDistance < 0) maxDistance = detectionRadius;
    
    std::vector<Player> nearbyPlayers;
    for (const auto& player : detectedPlayers) {
        if (player.distance <= maxDistance) {
            nearbyPlayers.push_back(player);
        }
    }
    
    return nearbyPlayers;
}

bool PlayerDetector::IsPlayerNearby(const std::string& playerName, double radius) {
    if (radius < 0) radius = responseRadius;
    
    for (const auto& player : detectedPlayers) {
        if (player.distance <= radius) {
            if (playerName.empty() || player.name == playerName) {
                return true;
            }
        }
    }
    
    return false;
}

void PlayerDetector::AddKnownPlayer(const std::string& name) {
    auto it = std::find(knownPlayerNames.begin(), knownPlayerNames.end(), name);
    if (it == knownPlayerNames.end()) {
        knownPlayerNames.push_back(name);
        
        // Save to file
        std::ofstream playerFile("known_players.txt", std::ios::app);
        playerFile << name << std::endl;
        playerFile.close();
    }
}

void PlayerDetector::RemoveKnownPlayer(const std::string& name) {
    auto it = std::find(knownPlayerNames.begin(), knownPlayerNames.end(), name);
    if (it != knownPlayerNames.end()) {
        knownPlayerNames.erase(it);
        
        // Rewrite the file
        std::ofstream playerFile("known_players.txt");
        for (const auto& player : knownPlayerNames) {
            playerFile << player << std::endl;
        }
        playerFile.close();
    }
}

PlayerDetector::Player* PlayerDetector::FindPlayerByName(const std::string& name) {
    for (auto& player : detectedPlayers) {
        if (player.name == name) {
            return &player;
        }
    }
    return nullptr;
}

double PlayerDetector::CalculateDistance(const cv::Point2f& pos1, const cv::Point2f& pos2) {
    return cv::norm(pos1 - pos2);
}

bool PlayerDetector::IsValidPlayerDetection(const cv::Rect& region, const cv::Mat& frame) {
    if (region.x + region.width >= frame.cols || region.y + region.height >= frame.rows ||
        region.x < 0 || region.y < 0) {
        return false;
    }
    
    cv::Mat playerRegion = frame(region);
    
    cv::Scalar mean, stddev;
    cv::meanStdDev(playerRegion, mean, stddev);
    
    double totalStdDev = stddev[0] + stddev[1] + stddev[2];
    return totalStdDev > 30.0; // Minimum variance threshold
}
#include "MinecraftAI.h"

HumanizationEngine::HumanizationEngine() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
    timing.lastAction = std::chrono::steady_clock::now();
}

void HumanizationEngine::LearnFromPattern(const MovementPattern& pattern) {
    if (pattern.naturalness_score > 0.7) { // Only learn from good patterns
        learnedPatterns.push_back(pattern);
        
        // Keep only top 100 patterns to prevent memory bloat
        if (learnedPatterns.size() > 100) {
            std::sort(learnedPatterns.begin(), learnedPatterns.end(),
                [](const MovementPattern& a, const MovementPattern& b) {
                    return a.naturalness_score > b.naturalness_score;
                });
            learnedPatterns.resize(100);
        }
    }
}

cv::Point2f HumanizationEngine::GenerateHumanMouseMovement(cv::Point2f start, cv::Point2f target) {
    cv::Point2f delta = target - start;
    double distance = cv::norm(delta);
    
    // Scale by rotation speed and mouse sensitivity
    double speedFactor = (rotationSpeed / 100.0) * mouseSensitivity;
    double humanFactor = (humanizationLevel / 100.0);
    
    // Generate curved movement using Bezier curves
    std::uniform_real_distribution<float> curveDist(-0.3f * humanFactor, 0.3f * humanFactor);
    
    cv::Point2f control1 = start + cv::Point2f(delta.x * 0.3f + curveDist(rng) * distance * 0.2f,
                                               delta.y * 0.3f + curveDist(rng) * distance * 0.2f);
    cv::Point2f control2 = start + cv::Point2f(delta.x * 0.7f + curveDist(rng) * distance * 0.15f,
                                               delta.y * 0.7f + curveDist(rng) * distance * 0.15f);
    
    // Apply speed scaling with human-like acceleration
    double t = 0.8; // Use point along curve for natural movement
    float invT = 1.0f - t;
    cv::Point2f point = invT * invT * invT * start +
                       3 * invT * invT * t * control1 +
                       3 * invT * t * t * control2 +
                       t * t * t * target;
    
    return point;
}

int HumanizationEngine::GetHumanizedDelay(const std::string& actionType) {
    std::uniform_int_distribution<int> variance(-timing.reactionVariance, timing.reactionVariance);
    
    int baseDelay = timing.baseReactionTime;
    double humanFactor = (humanizationLevel / 100.0);
    
    if (actionType == "mining") {
        baseDelay = static_cast<int>(100 / (rotationSpeed / 100.0));
    } else if (actionType == "movement") {
        baseDelay = 50;
    } else if (actionType == "tool_switch") {
        baseDelay = 300;
    } else if (actionType == "player_interaction") {
        baseDelay = 500; // Longer delay when interacting with players
    }
    
    // Add fatigue over time
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastAction = std::chrono::duration_cast<std::chrono::minutes>(now - timing.lastAction);
    
    if (timeSinceLastAction.count() > 30) {
        timing.fatigueMultiplier = std::min(1.5, timing.fatigueMultiplier + 0.1);
    } else {
        timing.fatigueMultiplier = std::max(1.0, timing.fatigueMultiplier - 0.05);
    }
    
    timing.lastAction = now;
    
    return static_cast<int>(baseDelay * timing.fatigueMultiplier * (1.0 + humanFactor * 0.5)) + variance(rng);
}

void HumanizationEngine::AddNaturalJitter(cv::Point2f& movement) {
    double humanFactor = (humanizationLevel / 100.0);
    std::uniform_real_distribution<float> jitter(-1.0f * humanFactor, 1.0f * humanFactor);
    movement.x += jitter(rng);
    movement.y += jitter(rng);
}

double HumanizationEngine::CalculateNaturalness(const MovementPattern& pattern) {
    if (pattern.mouseMovements.size() < 2) return 0.0;
    
    double smoothness = 0.0;
    double timingVariance = 0.0;
    
    // Calculate smoothness (lower acceleration changes = more natural)
    for (size_t i = 2; i < pattern.mouseMovements.size(); i++) {
        cv::Point2f v1 = pattern.mouseMovements[i-1] - pattern.mouseMovements[i-2];
        cv::Point2f v2 = pattern.mouseMovements[i] - pattern.mouseMovements[i-1];
        
        double acceleration = cv::norm(v2 - v1);
        smoothness += acceleration;
    }
    
    // Calculate timing variance (humans have natural rhythm variations)
    if (pattern.timings.size() > 1) {
        double meanTiming = 0.0;
        for (int timing : pattern.timings) {
            meanTiming += timing;
        }
        meanTiming /= pattern.timings.size();
        
        for (int timing : pattern.timings) {
            timingVariance += std::pow(timing - meanTiming, 2);
        }
        timingVariance = std::sqrt(timingVariance / pattern.timings.size());
    }
    
    // Normalize and combine metrics
    double normalizedSmoothness = 1.0 / (1.0 + smoothness / 100.0);
    double normalizedVariance = std::min(1.0, timingVariance / 50.0);
    
    return (normalizedSmoothness * 0.7 + normalizedVariance * 0.3);
}
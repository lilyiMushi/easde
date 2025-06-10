#include "MinecraftAI.h"

bool VideoLearner::LoadVideo(const std::string& videoPath) {
    videoCapture.release();
    
    if (!videoCapture.open(videoPath)) {
        std::cout << "Failed to open video: " << videoPath << std::endl;
        return false;
    }
    
    if (!videoCapture.isOpened()) {
        std::cout << "Video not opened properly: " << videoPath << std::endl;
        return false;
    }
    
    // Get video properties
    double fps = videoCapture.get(cv::CAP_PROP_FPS);
    int frameCount = static_cast<int>(videoCapture.get(cv::CAP_PROP_FRAME_COUNT));
    
    std::cout << "Video loaded: " << videoPath << std::endl;
    std::cout << "FPS: " << fps << ", Frames: " << frameCount << std::endl;
    
    return true;
}

void VideoLearner::ExtractMovementPatterns() {
    if (!videoCapture.isOpened()) {
        std::cout << "No video loaded for pattern extraction" << std::endl;
        return;
    }
    
    extractedPatterns.clear();
    
    cv::Mat prevFrame, currentFrame;
    cv::Mat prevGray, currentGray;
    
    // Read first frame
    if (!videoCapture.read(prevFrame)) {
        std::cout << "Failed to read first frame" << std::endl;
        return;
    }
    
    cv::cvtColor(prevFrame, prevGray, cv::COLOR_BGR2GRAY);
    
    HumanizationEngine::MovementPattern currentPattern;
    std::vector<cv::Point2f> mouseTrail;
    std::vector<int> timings;
    
    auto lastFrameTime = std::chrono::steady_clock::now();
    int frameIndex = 0;
    
    while (videoCapture.read(currentFrame)) {
        cv::cvtColor(currentFrame, currentGray, cv::COLOR_BGR2GRAY);
        
        // Track mouse movement between frames
        cv::Point2f mouseMovement = TrackMouseMovement(prevGray, currentGray);
        
        auto currentTime = std::chrono::steady_clock::now();
        int timeDelta = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFrameTime).count());
        
        // Only add significant movements
        if (cv::norm(mouseMovement) > 1.0) {
            mouseTrail.push_back(mouseMovement);
            timings.push_back(timeDelta);
        }
        
        // Detect mining actions to segment patterns
        bool isMining = DetectMiningAction(currentFrame);
        
        // If we detect a complete mining sequence or pattern gets too long
        if ((isMining && mouseTrail.size() > 5) || mouseTrail.size() > 50) {
            if (mouseTrail.size() >= 3) {
                HumanizationEngine::MovementPattern pattern;
                pattern.mouseMovements = mouseTrail;
                pattern.timings = timings;
                pattern.naturalness_score = CalculateNaturalness(pattern);
                
                if (pattern.naturalness_score > 0.3) { // Only keep decent patterns
                    extractedPatterns.push_back(pattern);
                }
            }
            
            // Reset for next pattern
            mouseTrail.clear();
            timings.clear();
        }
        
        prevGray = currentGray.clone();
        lastFrameTime = currentTime;
        frameIndex++;
        
        // Progress feedback
        if (frameIndex % 100 == 0) {
            std::cout << "Processed " << frameIndex << " frames, found " 
                     << extractedPatterns.size() << " patterns" << std::endl;
        }
    }
    
    // Add final pattern if exists
    if (mouseTrail.size() >= 3) {
        HumanizationEngine::MovementPattern pattern;
        pattern.mouseMovements = mouseTrail;
        pattern.timings = timings;
        pattern.naturalness_score = CalculateNaturalness(pattern);
        
        if (pattern.naturalness_score > 0.3) {
            extractedPatterns.push_back(pattern);
        }
    }
    
    std::cout << "Pattern extraction complete. Found " << extractedPatterns.size() 
             << " movement patterns" << std::endl;
}

cv::Point2f VideoLearner::TrackMouseMovement(const cv::Mat& frame1, const cv::Mat& frame2) {
    // Use optical flow to detect mouse cursor movement
    std::vector<cv::Point2f> corners1, corners2;
    
    // Detect good features to track (including potential cursor)
    cv::goodFeaturesToTrack(frame1, corners1, 100, 0.01, 10);
    
    if (corners1.empty()) {
        return cv::Point2f(0, 0);
    }
    
    // Calculate optical flow
    std::vector<cv::Point2f> corners2_flow;
    std::vector<uchar> status;
    std::vector<float> errors;
    
    cv::calcOpticalFlowPyrLK(frame1, frame2, corners1, corners2_flow, status, errors);
    
    // Find the most consistent movement (likely cursor)
    cv::Point2f avgMovement(0, 0);
    int validPoints = 0;
    
    for (size_t i = 0; i < corners1.size(); i++) {
        if (status[i] && errors[i] < 50.0) {
            cv::Point2f movement = corners2_flow[i] - corners1[i];
            
            // Filter out small movements (noise) and very large movements (background)
            float magnitude = cv::norm(movement);
            if (magnitude > 2.0 && magnitude < 100.0) {
                avgMovement += movement;
                validPoints++;
            }
        }
    }
    
    if (validPoints > 0) {
        avgMovement /= static_cast<float>(validPoints);
    }
    
    return avgMovement;
}

bool VideoLearner::DetectMiningAction(const cv::Mat& frame) {
    // Simple mining detection based on screen changes
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    // Look for mining progress bar or crosshair changes
    cv::Rect centerRegion(frame.cols/2 - 50, frame.rows/2 - 50, 100, 100);
    
    if (centerRegion.x >= 0 && centerRegion.y >= 0 && 
        centerRegion.x + centerRegion.width < frame.cols &&
        centerRegion.y + centerRegion.height < frame.rows) {
        
        cv::Mat centerArea = gray(centerRegion);
        cv::Scalar meanIntensity = cv::mean(centerArea);
        
        // Mining often causes rapid brightness changes in center
        static double lastIntensity = 0;
        double intensityChange = std::abs(meanIntensity[0] - lastIntensity);
        lastIntensity = meanIntensity[0];
        
        return intensityChange > 10.0; // Threshold for mining detection
    }
    
    return false;
}

void VideoLearner::AnalyzeHumanBehavior() {
    if (extractedPatterns.empty()) {
        std::cout << "No patterns to analyze" << std::endl;
        return;
    }
    
    std::cout << "Analyzing human behavior patterns..." << std::endl;
    
    // Analyze timing patterns
    std::vector<double> reactionTimes;
    std::vector<double> movementSpeeds;
    std::vector<double> accelerations;
    
    for (const auto& pattern : extractedPatterns) {
        if (pattern.timings.size() >= 2) {
            // Calculate average reaction time
            double avgTiming = 0;
            for (int timing : pattern.timings) {
                avgTiming += timing;
            }
            avgTiming /= pattern.timings.size();
            reactionTimes.push_back(avgTiming);
            
            // Calculate movement characteristics
            if (pattern.mouseMovements.size() >= 2) {
                double totalDistance = 0;
                double totalTime = 0;
                
                for (size_t i = 1; i < pattern.mouseMovements.size(); i++) {
                    double distance = cv::norm(pattern.mouseMovements[i] - pattern.mouseMovements[i-1]);
                    totalDistance += distance;
                    
                    if (i-1 < pattern.timings.size()) {
                        totalTime += pattern.timings[i-1];
                    }
                }
                
                if (totalTime > 0) {
                    double speed = totalDistance / totalTime;
                    movementSpeeds.push_back(speed);
                }
                
                // Calculate acceleration changes (jerkiness)
                if (pattern.mouseMovements.size() >= 3) {
                    double totalAcceleration = 0;
                    for (size_t i = 2; i < pattern.mouseMovements.size(); i++) {
                        cv::Point2f v1 = pattern.mouseMovements[i-1] - pattern.mouseMovements[i-2];
                        cv::Point2f v2 = pattern.mouseMovements[i] - pattern.mouseMovements[i-1];
                        double acceleration = cv::norm(v2 - v1);
                        totalAcceleration += acceleration;
                    }
                    accelerations.push_back(totalAcceleration / (pattern.mouseMovements.size() - 2));
                }
            }
        }
    }
    
    // Calculate statistics
    if (!reactionTimes.empty()) {
        double avgReaction = std::accumulate(reactionTimes.begin(), reactionTimes.end(), 0.0) / reactionTimes.size();
        std::cout << "Average reaction time: " << avgReaction << "ms" << std::endl;
    }
    
    if (!movementSpeeds.empty()) {
        double avgSpeed = std::accumulate(movementSpeeds.begin(), movementSpeeds.end(), 0.0) / movementSpeeds.size();
        std::cout << "Average movement speed: " << avgSpeed << " pixels/ms" << std::endl;
    }
    
    if (!accelerations.empty()) {
        double avgAcceleration = std::accumulate(accelerations.begin(), accelerations.end(), 0.0) / accelerations.size();
        std::cout << "Average acceleration: " << avgAcceleration << std::endl;
    }
    
    // Save analysis results
    SavePatternsToFile("learned_patterns.json");
}

double VideoLearner::CalculateNaturalness(const HumanizationEngine::MovementPattern& pattern) {
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

std::vector<HumanizationEngine::MovementPattern> VideoLearner::GetLearnedPatterns() const {
    return extractedPatterns;
}

void VideoLearner::SavePatternsToFile(const std::string& filename) {
    Json::Value root;
    Json::Value patterns(Json::arrayValue);
    
    for (const auto& pattern : extractedPatterns) {
        Json::Value patternJson;
        
        // Save mouse movements
        Json::Value movements(Json::arrayValue);
        for (const auto& movement : pattern.mouseMovements) {
            Json::Value point;
            point["x"] = movement.x;
            point["y"] = movement.y;
            movements.append(point);
        }
        patternJson["movements"] = movements;
        
        // Save timings
        Json::Value timings(Json::arrayValue);
        for (int timing : pattern.timings) {
            timings.append(timing);
        }
        patternJson["timings"] = timings;
        patternJson["naturalness_score"] = pattern.naturalness_score;
        
        patterns.append(patternJson);
    }
    
    root["patterns"] = patterns;
    root["pattern_count"] = static_cast<int>(extractedPatterns.size());
    root["extraction_date"] = std::time(nullptr);
    
    std::ofstream file(filename);
    if (file.is_open()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &file);
        file.close();
        std::cout << "Patterns saved to " << filename << std::endl;
    } else {
        std::cout << "Failed to save patterns to " << filename << std::endl;
    }
}

void VideoLearner::LoadPatternsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "No existing patterns file found: " << filename << std::endl;
        return;
    }
    
    Json::Value root;
    file >> root;
    file.close();
    
    extractedPatterns.clear();
    
    if (root.isMember("patterns") && root["patterns"].isArray()) {
        for (const auto& patternJson : root["patterns"]) {
            HumanizationEngine::MovementPattern pattern;
            
            // Load mouse movements
            if (patternJson.isMember("movements") && patternJson["movements"].isArray()) {
                for (const auto& movement : patternJson["movements"]) {
                    cv::Point2f point(movement["x"].asFloat(), movement["y"].asFloat());
                    pattern.mouseMovements.push_back(point);
                }
            }
            
            // Load timings
            if (patternJson.isMember("timings") && patternJson["timings"].isArray()) {
                for (const auto& timing : patternJson["timings"]) {
                    pattern.timings.push_back(timing.asInt());
                }
            }
            
            pattern.naturalness_score = patternJson.get("naturalness_score", 0.0).asDouble();
            extractedPatterns.push_back(pattern);
        }
        
        std::cout << "Loaded " << extractedPatterns.size() << " patterns from " << filename << std::endl;
    }
}
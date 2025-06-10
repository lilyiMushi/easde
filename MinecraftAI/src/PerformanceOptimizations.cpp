#include "MinecraftAI.h"

// PerformanceMonitor Implementation
PerformanceMonitor::PerformanceMonitor() {
    frameTimes.resize(FRAME_HISTORY_SIZE, 0.0);
    lastFrameTime = std::chrono::steady_clock::now();
}

void PerformanceMonitor::FrameStart() {
    auto now = std::chrono::steady_clock::now();
    double frameTime = std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
    
    frameTimes[frameIndex % FRAME_HISTORY_SIZE] = frameTime;
    frameIndex++;
    lastFrameTime = now;
}

double PerformanceMonitor::GetAverageFrameTime() const {
    double sum = 0.0;
    size_t count = std::min(frameIndex, FRAME_HISTORY_SIZE);
    for (size_t i = 0; i < count; i++) {
        sum += frameTimes[i];
    }
    return count > 0 ? sum / count : 0.0;
}

double PerformanceMonitor::GetFPS() const {
    double avgFrameTime = GetAverageFrameTime();
    return avgFrameTime > 0 ? 1000.0 / avgFrameTime : 0.0;
}

// ThreadPool Implementation
ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    
                    if (stop && tasks.empty()) return;
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        worker.join();
    }
}

// ImageProcessingCache Implementation
cv::Mat ImageProcessingCache::getOrProcess(const cv::Mat& input, 
                                          std::function<cv::Mat(const cv::Mat&)> processor) {
    size_t hash = computeHash(input);
    
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    auto it = processedImages.find(hash);
    if (it != processedImages.end()) {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - cacheTimestamps[hash]).count();
        
        if (age < CACHE_TIMEOUT_MS) {
            return it->second;
        }
    }
    
    // Process and cache
    cv::Mat result = processor(input);
    processedImages[hash] = result.clone();
    cacheTimestamps[hash] = std::chrono::steady_clock::now();
    
    // Clean old entries
    cleanOldEntries();
    
    return result;
}

size_t ImageProcessingCache::computeHash(const cv::Mat& mat) {
    if (mat.empty()) return 0;
    
    // Simple hash based on image properties and sample pixels
    size_t hash = std::hash<int>{}(mat.rows) ^ 
                 (std::hash<int>{}(mat.cols) << 1) ^
                 (std::hash<int>{}(mat.type()) << 2);
    
    // Sample a few pixels for content-based hashing
    if (mat.rows > 10 && mat.cols > 10) {
        cv::Vec3b pixel1 = mat.at<cv::Vec3b>(mat.rows/4, mat.cols/4);
        cv::Vec3b pixel2 = mat.at<cv::Vec3b>(mat.rows/2, mat.cols/2);
        cv::Vec3b pixel3 = mat.at<cv::Vec3b>(3*mat.rows/4, 3*mat.cols/4);
        
        hash ^= (std::hash<int>{}(pixel1[0] + pixel1[1] + pixel1[2]) << 3);
        hash ^= (std::hash<int>{}(pixel2[0] + pixel2[1] + pixel2[2]) << 4);
        hash ^= (std::hash<int>{}(pixel3[0] + pixel3[1] + pixel3[2]) << 5);
    }
    
    return hash;
}

void ImageProcessingCache::cleanOldEntries() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = cacheTimestamps.begin(); it != cacheTimestamps.end();) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second).count();
        
        if (age > CACHE_TIMEOUT_MS) {
            processedImages.erase(it->first);
            it = cacheTimestamps.erase(it);
        } else {
            ++it;
        }
    }
}

// OptimizedMinecraftBot Implementation
OptimizedMinecraftBot::OptimizedMinecraftBot(HumanizationEngine* h, SkyblockStats* s, 
                                            PlayerDetector* pd, ChatHandler* ch)
    : MinecraftBot(h, s, pd, ch) {
    lastCaptureTime = std::chrono::steady_clock::now();
    lastBlockDetection = std::chrono::steady_clock::now();
    
    // Initialize ROIs with default values
    UpdateROIs();
}

void OptimizedMinecraftBot::CaptureGameState() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCapture = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastCaptureTime).count();
    
    // Only capture if enough time has passed
    if (timeSinceLastCapture < CAPTURE_INTERVAL_MS && !lastScreenshot.empty()) {
        currentState.screenshot = lastScreenshot;
        return; // Use cached screenshot
    }
    
    // Capture only the necessary region instead of full screen
    cv::Mat newScreenshot = CaptureOptimizedScreen();
    
    if (!newScreenshot.empty()) {
        lastScreenshot = newScreenshot;
        currentState.screenshot = lastScreenshot;
        lastCaptureTime = now;
        
        // Update ROIs based on current state
        UpdateROIs();
        
        // Process only relevant regions
        ProcessRelevantRegions();
    }
}

cv::Mat OptimizedMinecraftBot::CaptureOptimizedScreen() {
    if (!minecraftWindow) return cv::Mat();
    
    RECT windowRect;
    GetWindowRect(minecraftWindow, &windowRect);
    
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    // Only capture the game area, skip title bar and borders
    int gameAreaTop = 30; // Skip title bar
    int gameAreaHeight = height - 60; // Skip title bar and bottom border
    int gameAreaLeft = 8; // Skip left border
    int gameAreaWidth = width - 16; // Skip left and right borders
    
    // Ensure we don't go out of bounds
    if (gameAreaTop >= height || gameAreaLeft >= width || 
        gameAreaHeight <= 0 || gameAreaWidth <= 0) {
        // Fallback to full window capture
        return CaptureScreen();
    }
    
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, gameAreaWidth, gameAreaHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Capture only the game area
    BitBlt(hdcMem, 0, 0, gameAreaWidth, gameAreaHeight, hdcScreen, 
           windowRect.left + gameAreaLeft, windowRect.top + gameAreaTop, SRCCOPY);
    
    // Create OpenCV Mat
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = gameAreaWidth;
    bi.biHeight = -gameAreaHeight; // Negative for top-down DIB
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    
    cv::Mat screenshot(gameAreaHeight, gameAreaWidth, CV_8UC3);
    GetDIBits(hdcMem, hBitmap, 0, gameAreaHeight, screenshot.data, 
              (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    // Convert BGR to RGB (OpenCV uses BGR by default)
    cv::cvtColor(screenshot, screenshot, cv::COLOR_BGR2RGB);
    
    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    
    return screenshot;
}

void OptimizedMinecraftBot::UpdateROIs() {
    if (lastScreenshot.empty()) return;
    
    int width = lastScreenshot.cols;
    int height = lastScreenshot.rows;
    
    // Define regions of interest based on typical Minecraft layout
    
    // Mining ROI: Central area where blocks are typically visible
    miningROI = cv::Rect(width/4, height/4, width/2, height/2);
    
    // Player detection ROI: Full screen but we'll process at lower resolution
    playerDetectionROI = cv::Rect(0, 0, width, height);
    
    // Chat ROI: Top-left corner where chat typically appears
    chatROI = cv::Rect(0, 0, std::min(400, width/3), std::min(200, height/4));
    
    // Ensure ROIs are within image bounds
    auto clampROI = [&](cv::Rect& roi) {
        roi.x = std::max(0, std::min(roi.x, width - 1));
        roi.y = std::max(0, std::min(roi.y, height - 1));
        roi.width = std::max(1, std::min(roi.width, width - roi.x));
        roi.height = std::max(1, std::min(roi.height, height - roi.y));
    };
    
    clampROI(miningROI);
    clampROI(playerDetectionROI);
    clampROI(chatROI);
}

void OptimizedMinecraftBot::ProcessRelevantRegions() {
    if (lastScreenshot.empty()) return;
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastBlockDetection = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastBlockDetection).count();
    
    // Only update block detection every 200ms to save performance
    if (timeSinceLastBlockDetection > 200) {
        // Process mining region for block detection
        cv::Mat miningRegion = lastScreenshot(miningROI);
        currentState.detectedBlocks = DetectBlocksOptimized(miningRegion, cv::Point(miningROI.x, miningROI.y));
        lastBlockDetection = now;
    } else {
        // Use cached block detection
        currentState.detectedBlocks = cachedBlocks;
    }
    
    // Process chat region (only if chat responses are enabled)
    if (chatHandler) {
        cv::Mat chatRegion = lastScreenshot(chatROI);
        chatHandler->ProcessChatRegion(chatRegion);
    }
    
    // Process player detection region (downsampled for performance)
    if (playerDetector) {
        cv::Mat playerRegion = lastScreenshot(playerDetectionROI);
        
        // Downsample to 1/2 resolution for faster processing
        cv::Mat downsampledRegion;
        cv::resize(playerRegion, downsampledRegion, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);
        
        playerDetector->UpdateDetection(downsampledRegion);
        currentState.nearbyPlayers = playerDetector->GetNearbyPlayers();
    }
    
    // Check for responses needed
    currentState.shouldRespondToPlayer = (chatHandler && chatHandler->WasMentioned()) || 
                                        (playerDetector && playerDetector->IsPlayerNearby("", 5.0));
}

std::vector<cv::Rect> OptimizedMinecraftBot::DetectBlocksOptimized(const cv::Mat& roi, cv::Point offset) {
    std::vector<cv::Rect> blocks;
    if (roi.empty()) return blocks;
    
    // Use cached processed image if available
    cv::Mat processedROI;
    
    // Convert to grayscale for edge detection
    if (roi.channels() == 3) {
        cv::cvtColor(roi, grayImage, cv::COLOR_BGR2GRAY);
    } else {
        grayImage = roi.clone();
    }
    
    // Apply Gaussian blur to reduce noise
    cv::GaussianBlur(grayImage, processedROI, cv::Size(3, 3), 0);
    
    // Edge detection with optimized parameters
    cv::Mat edges;
    cv::Canny(processedROI, edges, 30, 90);
    
    // Morphological operations to connect nearby edges
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Filter contours for block-like shapes
    for (const auto& contour : contours) {
        cv::Rect boundingRect = cv::boundingRect(contour);
        
        // Apply offset to convert back to full image coordinates
        boundingRect.x += offset.x;
        boundingRect.y += offset.y;
        
        // Filter by size for block-like objects (optimized thresholds)
        if (boundingRect.width >= 15 && boundingRect.width <= 80 &&
            boundingRect.height >= 15 && boundingRect.height <= 80) {
            
            double aspectRatio = static_cast<double>(boundingRect.height) / boundingRect.width;
            double area = cv::contourArea(contour);
            double rectArea = boundingRect.width * boundingRect.height;
            double fillRatio = area / rectArea;
            
            // Minecraft blocks are roughly square with good fill ratio
            if (aspectRatio > 0.7 && aspectRatio < 1.4 && fillRatio > 0.4) {
                blocks.push_back(boundingRect);
            }
        }
    }
    
    // Cache the results
    cachedBlocks = blocks;
    
    // Sort blocks by proximity to center (prioritize central blocks)
    cv::Point2f center(lastScreenshot.cols / 2.0f, lastScreenshot.rows / 2.0f);
    std::sort(blocks.begin(), blocks.end(), [&center](const cv::Rect& a, const cv::Rect& b) {
        cv::Point2f centerA(a.x + a.width/2.0f, a.y + a.height/2.0f);
        cv::Point2f centerB(b.x + b.width/2.0f, b.y + b.height/2.0f);
        
        double distA = cv::norm(centerA - center);
        double distB = cv::norm(centerB - center);
        
        return distA < distB;
    });
    
    // Limit number of blocks to process (performance optimization)
    if (blocks.size() > 10) {
        blocks.resize(10);
    }
    
    return blocks;
}
#include "MinecraftAI.h"

MinecraftBot::MinecraftBot(HumanizationEngine* h, SkyblockStats* s, PlayerDetector* pd, ChatHandler* ch) 
    : humanizer(h), stats(s), playerDetector(pd), chatHandler(ch), minecraftWindow(nullptr) {}

bool MinecraftBot::FindMinecraftWindow() {
    minecraftWindow = FindWindowA(nullptr, "Minecraft");
    if (!minecraftWindow) {
        minecraftWindow = FindWindowA(nullptr, "Minecraft 1.8.9");
    }
    if (!minecraftWindow) {
        minecraftWindow = FindWindowA(nullptr, "Lunar Client");
    }
    if (!minecraftWindow) {
        minecraftWindow = FindWindowA(nullptr, "Badlion Client");
    }
    
    return minecraftWindow != nullptr;
}

void MinecraftBot::CaptureGameState() {
    currentState.screenshot = CaptureScreen();
    currentState.detectedBlocks = DetectBlocks(currentState.screenshot);
    currentState.nearbyPlayers = playerDetector->GetNearbyPlayers();
    currentState.shouldRespondToPlayer = chatHandler->WasMentioned() || 
                                        playerDetector->IsPlayerNearby("", 5.0);
    
    if (isMining) {
        currentState.isBlockBroken = IsBlockBroken();
        if (!currentState.detectedBlocks.empty()) {
            cv::Rect miningRegion(static_cast<int>(currentMiningTarget.x - 20), 
                                 static_cast<int>(currentMiningTarget.y - 20), 40, 40);
            currentState.currentBlockType = IdentifyBlockType(miningRegion, currentState.screenshot);
        }
    }
}

void MinecraftBot::StartMining(cv::Point2f blockPosition) {
    currentMiningTarget = blockPosition;
    isMining = true;
    miningStartTime = std::chrono::steady_clock::now();
    
    // Move mouse to block with human-like movement
    POINT currentCursor;
    GetCursorPos(&currentCursor);
    cv::Point2f currentPos(static_cast<float>(currentCursor.x), static_cast<float>(currentCursor.y));
    
    cv::Point2f humanizedTarget = humanizer->GenerateHumanMouseMovement(currentPos, blockPosition);
    SendMouseMove(humanizedTarget - currentPos);
    
    // Start mining with human delay
    std::this_thread::sleep_for(std::chrono::milliseconds(
        humanizer->GetHumanizedDelay("mining")));
    SendClick(true);
}

void MinecraftBot::StopMining() {
    isMining = false;
}

bool MinecraftBot::IsBlockBroken() {
    if (!isMining) return false;
    
    auto now = std::chrono::steady_clock::now();
    auto miningDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - miningStartTime);
    
    double expectedMiningTime = CalculateMiningTime(currentState.currentBlockType);
    
    return miningDuration.count() >= expectedMiningTime;
}

double MinecraftBot::CalculateMiningTime(const std::string& blockType) {
    double miningSpeed = stats->GetMiningSpeed(currentState.currentTool, blockType);
    return 1000.0 / miningSpeed; // Convert to milliseconds
}

void MinecraftBot::MoveToNextBlock() {
    for (const auto& block : currentState.detectedBlocks) {
        cv::Point2f blockCenter(static_cast<float>(block.x + block.width/2), 
                               static_cast<float>(block.y + block.height/2));
        if (cv::norm(blockCenter - currentMiningTarget) > 10) {
            StartMining(blockCenter);
            return;
        }
    }
}

cv::Mat MinecraftBot::CaptureScreen() {
    if (!minecraftWindow) return cv::Mat();
    
    RECT windowRect;
    GetWindowRect(minecraftWindow, &windowRect);
    
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);
    
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, windowRect.left, windowRect.top, SRCCOPY);
    
    cv::Mat screenshot(height, width, CV_8UC3);
    GetBitmapBits(hBitmap, width * height * 3, screenshot.data);
    
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    
    return screenshot;
}

std::vector<cv::Rect> MinecraftBot::DetectBlocks(const cv::Mat& image) {
    std::vector<cv::Rect> blocks;
    if (image.empty()) return blocks;
    
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    for (const auto& contour : contours) {
        cv::Rect boundingRect = cv::boundingRect(contour);
        
        // Filter by size for block-like objects
        if (boundingRect.width > 20 && boundingRect.width < 100 &&
            boundingRect.height > 20 && boundingRect.height < 100) {
            
            double aspectRatio = static_cast<double>(boundingRect.height) / boundingRect.width;
            // Minecraft blocks are roughly square
            if (aspectRatio > 0.8 && aspectRatio < 1.2) {
                blocks.push_back(boundingRect);
            }
        }
    }
    
    return blocks;
}

std::string MinecraftBot::IdentifyBlockType(const cv::Rect& blockRegion, const cv::Mat& image) {
    if (blockRegion.x + blockRegion.width >= image.cols || 
        blockRegion.y + blockRegion.height >= image.rows ||
        blockRegion.x < 0 || blockRegion.y < 0) {
        return "unknown";
    }
    
    cv::Mat blockImage = image(blockRegion);
    cv::Scalar meanColor = cv::mean(blockImage);
    
    // Simple color-based block identification
    if (meanColor[0] < 50 && meanColor[1] < 50 && meanColor[2] < 50) {
        return "bedrock";
    } else if (meanColor[2] > 200 && meanColor[0] < 100 && meanColor[1] < 100) {
        return "redstone_ore";
    } else if (meanColor[0] > 150 && meanColor[1] > 150 && meanColor[2] > 150) {
        return "stone";
    } else if (meanColor[1] > 180 && meanColor[0] < 100 && meanColor[2] < 100) {
        return "emerald_ore";
    } else if (meanColor[0] > 200 && meanColor[1] > 200 && meanColor[2] < 100) {
        return "gold_ore";
    }
    
    return "unknown";
}

void MinecraftBot::SendMouseMove(cv::Point2f delta) {
    humanizer->AddNaturalJitter(delta);
    
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = static_cast<LONG>(delta.x);
    input.mi.dy = static_cast<LONG>(delta.y);
    
    SendInput(1, &input, sizeof(INPUT));
}

void MinecraftBot::SendClick(bool leftClick) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = leftClick ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    
    SendInput(1, &input, sizeof(INPUT));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(actionDelayMs));
    
    input.mi.dwFlags = leftClick ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void MinecraftBot::SendKeyPress(int keyCode) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void MinecraftBot::ExecuteAction(ActionType action) {
    switch (action) {
        case ActionType::MINE_BLOCK:
            if (!currentState.detectedBlocks.empty()) {
                cv::Point2f target(static_cast<float>(currentState.detectedBlocks[0].x + currentState.detectedBlocks[0].width/2),
                                 static_cast<float>(currentState.detectedBlocks[0].y + currentState.detectedBlocks[0].height/2));
                StartMining(target);
            }
            break;
        case ActionType::SWITCH_TOOL:
            if (autoSwitchTools) {
                SendKeyPress('1'); // Switch to slot 1 (pickaxe)
            }
            break;
        case ActionType::LOOK_AROUND:
            // Implement looking around behavior
            break;
        case ActionType::MOVE_TO_POSITION:
            // Implement movement
            break;
        case ActionType::IDLE:
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            break;
    }
}
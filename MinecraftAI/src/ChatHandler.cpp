#include "MinecraftAI.h"

ChatHandler::ChatHandler(const std::string& botPlayerName) : botName(botPlayerName) {
    // Initialize chat region (typical Minecraft chat area)
    chatRegion = cv::Rect(2, 2, 400, 200); // Top-left area where chat appears
    
    // Initialize response templates
    responseTemplates = {
        "Hello {player}!",
        "Hi there {player}!",
        "Hey {player}, how's it going?",
        "Nice to see you {player}!",
        "What's up {player}?",
        "Greetings {player}!",
        "Good to see you here {player}!",
        "Hello {player}, having fun mining?"
    };
}

void ChatHandler::ProcessChatRegion(const cv::Mat& gameFrame) {
    if (gameFrame.empty() || !enabledResponses) return;
    
    // Ensure chat region is within frame bounds
    if (chatRegion.x + chatRegion.width > gameFrame.cols ||
        chatRegion.y + chatRegion.height > gameFrame.rows) {
        chatRegion = cv::Rect(2, 2, std::min(400, gameFrame.cols - 4), 
                             std::min(200, gameFrame.rows - 4));
    }
    
    cv::Mat chatArea = gameFrame(chatRegion);
    std::string chatText = ExtractChatText(chatArea);
    
    if (!chatText.empty()) {
        ParseChatMessage(chatText);
    }
}

std::string ChatHandler::ExtractChatText(const cv::Mat& chatRegion) {
    if (chatRegion.empty()) return "";
    
    // Convert to grayscale for OCR processing
    cv::Mat gray;
    cv::cvtColor(chatRegion, gray, cv::COLOR_BGR2GRAY);
    
    // Apply threshold to make text more readable
    cv::Mat thresh;
    cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
    
    // Simple character recognition for common chat patterns
    // In a full implementation, you'd use a proper OCR library like Tesseract
    
    // For now, we'll do pattern matching on known chat formats
    std::string extractedText = "";
    
    // Look for white/bright text areas (typical chat text)
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    for (const auto& contour : contours) {
        cv::Rect boundingRect = cv::boundingRect(contour);
        
        // Filter for text-like shapes
        if (boundingRect.width > 5 && boundingRect.height > 8 && 
            boundingRect.height < 30 && boundingRect.width < 200) {
            
            cv::Mat charRegion = thresh(boundingRect);
            char recognizedChar = RecognizeCharacter(charRegion);
            
            if (recognizedChar != '\0') {
                extractedText += recognizedChar;
            }
        }
    }
    
    return extractedText;
}

char ChatHandler::RecognizeCharacter(const cv::Mat& charRegion) {
    // Very simplified character recognition
    // In practice, you'd use machine learning or a proper OCR library
    
    if (charRegion.empty()) return '\0';
    
    cv::Scalar meanValue = cv::mean(charRegion);
    double whitePixelRatio = meanValue[0] / 255.0;
    
    // Basic pattern matching based on aspect ratio and pixel density
    double aspectRatio = static_cast<double>(charRegion.rows) / charRegion.cols;
    
    // This is extremely simplified - in reality you'd need much more sophisticated recognition
    if (whitePixelRatio > 0.3 && aspectRatio > 1.0) {
        return 'I'; // Tall, narrow character
    } else if (whitePixelRatio > 0.2 && aspectRatio < 0.8) {
        return '_'; // Wide, short character  
    } else if (whitePixelRatio > 0.25) {
        return 'O'; // Medium character
    }
    
    return '\0'; // Unrecognized
}

void ChatHandler::ParseChatMessage(const std::string& rawText) {
    if (rawText.empty()) return;
    
    // Look for common Minecraft chat patterns
    // Format: <PlayerName> message
    // Format: PlayerName: message
    // Format: [PlayerName] message
    
    std::regex chatPatterns[] = {
        std::regex(R"(<([^>]+)>\s*(.+))"),           // <PlayerName> message
        std::regex(R"(([^:]+):\s*(.+))"),            // PlayerName: message  
        std::regex(R"(\[([^\]]+)\]\s*(.+))"),        // [PlayerName] message
        std::regex(R"((\w+)\s+(.+))")                // PlayerName message
    };
    
    std::smatch matches;
    
    for (const auto& pattern : chatPatterns) {
        if (std::regex_search(rawText, matches, pattern)) {
            std::string playerName = matches[1].str();
            std::string message = matches[2].str();
            
            // Clean up the extracted strings
            playerName = std::regex_replace(playerName, std::regex(R"([^\w])"), "");
            message = std::regex_replace(message, std::regex(R"(^\s+|\s+$)"), "");
            
            if (!playerName.empty() && !message.empty() && playerName != botName) {
                ChatMessage chatMsg;
                chatMsg.playerName = playerName;
                chatMsg.message = message;
                chatMsg.timestamp = std::chrono::steady_clock::now();
                chatMsg.isWhisper = false;
                chatMsg.mentionsSelf = CheckIfMentioned(message);
                
                recentMessages.push_back(chatMsg);
                
                // Keep only recent messages (last 100)
                if (recentMessages.size() > 100) {
                    recentMessages.erase(recentMessages.begin());
                }
                
                break; // Found a match, stop checking other patterns
            }
        }
    }
}

bool ChatHandler::CheckIfMentioned(const std::string& message) {
    // Convert to lowercase for case-insensitive matching
    std::string lowerMessage = message;
    std::string lowerBotName = botName;
    
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
    std::transform(lowerBotName.begin(), lowerBotName.end(), lowerBotName.begin(), ::tolower);
    
    // Check for direct mentions
    if (lowerMessage.find(lowerBotName) != std::string::npos) {
        return true;
    }
    
    // Check for common addressing patterns
    std::vector<std::string> addressingPatterns = {
        "bot", "ai", "hey", "hello", "hi"
    };
    
    for (const auto& pattern : addressingPatterns) {
        if (lowerMessage.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool ChatHandler::WasMentioned() const {
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& message : recentMessages) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - message.timestamp);
        
        // Check messages from last 30 seconds
        if (timeDiff.count() <= 30 && message.mentionsSelf) {
            return true;
        }
    }
    
    return false;
}

std::vector<ChatHandler::ChatMessage> ChatHandler::GetRecentMentions(int seconds) {
    std::vector<ChatMessage> mentions;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& message : recentMessages) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - message.timestamp);
        
        if (timeDiff.count() <= seconds && message.mentionsSelf) {
            mentions.push_back(message);
        }
    }
    
    return mentions;
}

std::string ChatHandler::GenerateResponse(const ChatMessage& message) {
    if (responseTemplates.empty()) return "Hello!";
    
    // Select a random response template
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(responseTemplates.size()) - 1);
    
    std::string response = responseTemplates[dis(gen)];
    
    // Replace {player} placeholder with actual player name
    size_t pos = response.find("{player}");
    if (pos != std::string::npos) {
        response.replace(pos, 8, message.playerName);
    }
    
    // Add some context-based responses
    std::string lowerMessage = message.message;
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
    
    if (lowerMessage.find("mining") != std::string::npos) {
        response += " I'm just mining here.";
    } else if (lowerMessage.find("bot") != std::string::npos || lowerMessage.find("ai") != std::string::npos) {
        response += " Yes, I'm an AI assistant.";
    } else if (lowerMessage.find("help") != std::string::npos) {
        response += " I'm just mining, nothing special.";
    }
    
    return response;
}

void ChatHandler::SendChatMessage(const std::string& message) {
    if (message.empty()) return;
    
    // Simulate pressing T to open chat
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 'T';
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Type the message
    for (char c : message) {
        if (c == ' ') {
            input.ki.wVk = VK_SPACE;
        } else if (c >= 'A' && c <= 'Z') {
            input.ki.wVk = c;
        } else if (c >= 'a' && c <= 'z') {
            input.ki.wVk = c - 'a' + 'A'; // Convert to uppercase VK code
        } else {
            continue; // Skip special characters for simplicity
        }
        
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    
    // Press Enter to send
    input.ki.wVk = VK_RETURN;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void ChatHandler::SendWhisper(const std::string& playerName, const std::string& message) {
    if (playerName.empty() || message.empty()) return;
    
    std::string whisperCommand = "/msg " + playerName + " " + message;
    SendChatMessage(whisperCommand);
}
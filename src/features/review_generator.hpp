#pragma once
#include "../ai/ai_client.hpp"
#include <string>
#include <vector>
#include <memory>
#include <set>

namespace vit::features {

class ReviewGenerator {
public:
    explicit ReviewGenerator(std::unique_ptr<vit::ai::AIClient> aiClient);

    struct ReviewResult {
        bool success;
        std::string reviewContent;
        std::string error;
        
        static ReviewResult Success(const std::string& content) {
            return ReviewResult{true, content, ""};
        }
        
        static ReviewResult Error(const std::string& error) {
            return ReviewResult{false, "", error};
        }
    };

    struct FileChange {
        std::string filePath;
        std::string content;
        size_t fileSize;
        std::string changeDescription;
    };

    /**
     * Generate review for specific files
     */
    ReviewResult generateReviewForFiles(const std::vector<std::string>& filePaths);

private:
    std::unique_ptr<vit::ai::AIClient> aiClient_;
    
    // DEFINE THE MISSING CONSTANTS
    static constexpr size_t MAX_FILE_SIZE = 8000;       // 8KB per file
    static constexpr size_t MAX_TOTAL_SIZE = 5600;      // 5.6KB total
    static constexpr size_t MAX_FILES = 3;              // Max 3 files
    
    // DECLARE THE MISSING FUNCTIONS
    std::vector<FileChange> analyzeWorkingDirectory();
    std::vector<FileChange> analyzeSpecificFiles(const std::vector<std::string>& filePaths);
    bool shouldProcessFile(const std::string& filePath);
    bool isSourceFile(const std::string& filePath);
    bool validateTokenLimits(const std::vector<FileChange>& changes);
    size_t estimateTokens(const std::string& text);
    std::vector<vit::ai::AIClient::Message> createReviewPrompt(const std::vector<FileChange>& changes);
    std::string formatReviewContent(const std::string& aiResponse);
    std::string getCurrentTimestamp();
};

} 
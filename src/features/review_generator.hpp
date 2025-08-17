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


    ReviewResult generateReviewForFiles(const std::vector<std::string>& filePaths);

private:
    std::unique_ptr<vit::ai::AIClient> aiClient_;
    
    std::vector<FileChange> analyzeSpecificFiles(const std::vector<std::string>& filePaths);
    bool shouldProcessFile(const std::string& filePath);
    std::vector<vit::ai::AIClient::Message> createReviewPrompt(const std::vector<FileChange>& changes);
    std::string formatReviewContent(const std::string& aiResponse);
    std::string getCurrentTimestamp();
};

} 
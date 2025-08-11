#pragma once
#include "../ai/ai_client.hpp"
#include "../ai/openai_client.hpp"
#include <string>
#include <vector>
#include <memory>

namespace vit::features {

class CommentGenerator {
public:
    struct CommentResult {
        bool success;
        std::string originalContent;
        std::string modifiedContent;
        std::string error;
        std::string fileName;
        
        CommentResult(bool s, const std::string& orig, const std::string& mod, 
                     const std::string& err, const std::string& file)
            : success(s), originalContent(orig), modifiedContent(mod), error(err), fileName(file) {}
        
        static CommentResult Success(const std::string& original, const std::string& modified, const std::string& fileName) {
            return CommentResult(true, original, modified, "", fileName);
        }
        
        static CommentResult Error(const std::string& error, const std::string& fileName) {
            return CommentResult(false, "", "", error, fileName);
        }
    };

    explicit CommentGenerator(std::unique_ptr<vit::ai::AIClient> aiClient);
    
    CommentResult generateCommentsForFile(const std::string& filePath);
    
    std::vector<CommentResult> generateCommentsForFiles(const std::vector<std::string>& filePaths);
    
    static bool shouldProcessFile(const std::string& filePath);

    std::string stripMarkdownFormatting(const std::string& response);

private:
    std::unique_ptr<vit::ai::AIClient> aiClient_;
    
    std::vector<vit::ai::AIClient::Message> createCommentPrompt(const std::string& fileContent, const std::string& fileName);
    
    std::string validateAIResponse(const std::string& original, const std::string& modified);
};

}
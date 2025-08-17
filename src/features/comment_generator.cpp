#include "comment_generator.hpp"
#include "../utils/file_utils.hpp"
#include <iostream>
#include <algorithm>
#include <set>
#include <filesystem>
#include <sstream>

namespace vit::features {

CommentGenerator::CommentGenerator(std::unique_ptr<vit::ai::AIClient> aiClient) 
    : aiClient_(std::move(aiClient)) {
}

CommentGenerator::CommentResult CommentGenerator::generateCommentsForFile(const std::string& filePath) {
    std::string fileName = filePath;  // Keep full path for writing
    std::string displayName = filePath.substr(filePath.find_last_of("/\\") + 1);  // Just for display
    
    if (!vit::utils::FileUtils::isSourceFile(filePath)) {
        return CommentResult::Error("File type not suitable for comment generation", fileName);
    }
    
    std::string originalContent;
    try {
        originalContent = vit::utils::FileUtils::readFile(filePath);
    } catch (const std::exception& e) {
        return CommentResult::Error("Failed to read file: " + std::string(e.what()), fileName);
    }
    
    if (originalContent.empty()) {
        return CommentResult::Error("File is empty", fileName);
    }
    
    std::cout << "Generating comments for " << fileName << " using " << aiClient_->getProviderName() << "..." << std::endl;
    
    auto messages = createCommentPrompt(originalContent, fileName);
    
    auto future = aiClient_->generateResponse(messages);
    auto result = future.get();
    
    if (!result.success) {
        return CommentResult::Error("AI request failed: " + result.error, fileName);
    }

    // Strip markdown formatting before validation
    std::string cleanedContent = stripMarkdownFormatting(result.content);

    std::string validationError = validateAIResponse(originalContent, cleanedContent);
    if (!validationError.empty()) {
        return CommentResult::Error(validationError, fileName);
    }
    
    std::cout << "✓ Comments generated for " << fileName << std::endl;
    return CommentResult::Success(originalContent, cleanedContent, fileName);  // Use full path
}

std::vector<CommentGenerator::CommentResult> CommentGenerator::generateCommentsForFiles(const std::vector<std::string>& filePaths) {
    std::vector<CommentResult> results;
    
    for (const auto& filePath : filePaths) {
        std::cout << "\nProcessing file " << (results.size() + 1) << "/" << filePaths.size() << ": " << filePath << std::endl;
        results.push_back(generateCommentsForFile(filePath));
        
        // avoid rate limits
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return results;
}

std::vector<vit::ai::AIClient::Message> CommentGenerator::createCommentPrompt(const std::string& fileContent, const std::string& fileName) {
    std::string systemPrompt = 
        "You are an expert code documentation assistant. Your task is to add helpful, concise comments to code files.\n\n"
        "Guidelines:\n"
        "- Add comments only to functions, classes, and complex code blocks that don't already have adequate comments\n"
        "- Use the appropriate comment style for the programming language\n"
        "- Keep comments concise but informative\n"
        "- Focus on explaining WHAT the code does and WHY, not HOW (unless the HOW is particularly complex)\n"
        "- Preserve ALL existing code and comments exactly as they are\n"
        "- Only ADD comments, never modify existing code\n"
        "- Return ONLY the raw code with your added comments - no markdown formatting, no code blocks, no ```\n"
        "- If the code is already well-commented, return it unchanged\n";
    
    std::string userPrompt = 
        "Please add appropriate comments to this " + fileName + " file:\n\n" + fileContent + "\n\n"
        "Remember: Only add comments where they would be genuinely helpful. "
        "Return the complete file with your improvements.";
    
    return {
        vit::ai::AIClient::createSystemMessage(systemPrompt),
        vit::ai::AIClient::createUserMessage(userPrompt)
    };
}

std::string CommentGenerator::validateAIResponse(const std::string& original, const std::string& modified) {
    if (modified.empty()) {
        return "AI returned empty response";
    }
    
    double sizeRatio = static_cast<double>(modified.size()) / original.size();
    
    if (sizeRatio < 0.8) {
        return "AI response too short (" + std::to_string(modified.size()) + " chars vs " + 
               std::to_string(original.size()) + " original, " + 
               std::to_string(static_cast<int>(sizeRatio * 100)) + "% of original) - likely truncated due to token limits";
    }
    
    if (sizeRatio > 3.0) {
        return "AI response too long (" + std::to_string(modified.size()) + " chars vs " + 
               std::to_string(original.size()) + " original, " + 
               std::to_string(static_cast<int>(sizeRatio * 100)) + "% of original) - likely hallucinated content";
    }
    
    // Should contain some of the original content (basic sanity check)
    std::istringstream originalStream(original);
    std::string line;
    int linesChecked = 0;
    int linesFound = 0;
    
    while (std::getline(originalStream, line) && linesChecked < 5) {
        if (!line.empty() && line.find("//") != 0 && line.find("/*") != 0 && line.find("#") != 0) {
            if (modified.find(line) != std::string::npos) {
                linesFound++;
            }
            linesChecked++;
        }
    }
    
    // At least half of the checked lines should be found
    if (linesChecked > 0) {
        double contentRatio = static_cast<double>(linesFound) / linesChecked;
        if (contentRatio < 0.5) {
            return "AI response missing original content (" + std::to_string(linesFound) + "/" + 
                   std::to_string(linesChecked) + " lines found, " + 
                   std::to_string(static_cast<int>(contentRatio * 100)) + "%) - may have rewritten instead of adding comments";
        }
    }
    
    return ""; // Valid - return empty string
}

std::string CommentGenerator::stripMarkdownFormatting(const std::string& response) {
    std::string cleaned = response;
    
    size_t start = cleaned.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = cleaned.find_last_not_of(" \t\r\n");
    cleaned = cleaned.substr(start, end - start + 1);
    
    if (cleaned.length() > 6 && cleaned.substr(0, 3) == "```") {
        size_t firstNewline = cleaned.find('\n');
        if (firstNewline != std::string::npos) {
            size_t lastTripleBacktick = cleaned.rfind("```");
            if (lastTripleBacktick != std::string::npos && lastTripleBacktick > firstNewline) {
                cleaned = cleaned.substr(firstNewline + 1, lastTripleBacktick - firstNewline - 1);
                
                end = cleaned.find_last_not_of(" \t\r\n");
                if (end != std::string::npos) {
                    cleaned = cleaned.substr(0, end + 1);
                }
            }
        }
    }
    
    return cleaned;
}

}   
#include "review_generator.hpp"
#include "../utils/file_utils.hpp"
#include "../commit.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace vit::features {

ReviewGenerator::ReviewGenerator(std::unique_ptr<vit::ai::AIClient> aiClient) 
    : aiClient_(std::move(aiClient)) {}

ReviewGenerator::ReviewResult ReviewGenerator::generateReviewForFiles(const std::vector<std::string>& filePaths) {
    try {
        std::vector<FileChange> changes = analyzeSpecificFiles(filePaths);
        
        if (changes.empty()) {
            return ReviewResult::Error("No suitable files found for review");
        }
        
        // Token validation
        if (!validateTokenLimits(changes)) {
            return ReviewResult::Error("Files too large for AI context window. Try fewer/smaller files.");
        }
        
        // Size validation
        size_t totalSize = 0;
        for (const auto& change : changes) {
            totalSize += change.fileSize;
        }
        
        if (totalSize > MAX_TOTAL_SIZE) {
            return ReviewResult::Error("Total file size too large (" + 
                                     std::to_string(totalSize) + " bytes, max " + 
                                     std::to_string(MAX_TOTAL_SIZE) + ")");
        }
        
        if (changes.size() > MAX_FILES) {
            return ReviewResult::Error("Too many files for review (" + 
                                     std::to_string(changes.size()) + " files, max " + 
                                     std::to_string(MAX_FILES) + ")");
        }
        
        std::cout << "Generating AI review for " << changes.size() << " file(s)...\n";
        
        // Create AI prompt and get response
        auto messages = createReviewPrompt(changes);
        auto future = aiClient_->generateResponse(messages);
        auto result = future.get();

        if (!result.success) {
            return ReviewResult::Error("AI request failed: " + result.error);
        }

        // Format the review content
        std::string formattedReview = formatReviewContent(result.content);
        
        return ReviewResult::Success(formattedReview);

    } catch (const std::exception& e) {
        return ReviewResult::Error("Exception during review generation: " + std::string(e.what()));
    }
}

// IMPLEMENT THE MISSING FUNCTIONS

std::vector<ReviewGenerator::FileChange> ReviewGenerator::analyzeWorkingDirectory() {
    std::vector<FileChange> changes;
    
    // Get all files in working directory (using existing vit function)
    auto workingFiles = getWorkingDirectoryFiles(".");
    
    for (const auto& filePath : workingFiles) {
        if (!shouldProcessFile(filePath)) {
            continue;
        }
        
        try {
            std::string content = vit::utils::FileUtils::readFile(filePath);
            size_t fileSize = content.size();
            
            // Skip empty files or files that are too large
            if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
                continue;
            }
            
            FileChange change;
            change.filePath = filePath;
            change.content = content;
            change.fileSize = fileSize;
            change.changeDescription = "Modified file";
            
            changes.push_back(change);
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not read file " << filePath << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    return changes;
}

std::vector<ReviewGenerator::FileChange> ReviewGenerator::analyzeSpecificFiles(const std::vector<std::string>& filePaths) {
    std::vector<FileChange> changes;
    
    for (const auto& filePath : filePaths) {
        if (!shouldProcessFile(filePath)) {
            std::cout << "Skipping " << filePath << " (not a source file or too large)\n";
            continue;
        }
        
        try {
            std::string content = vit::utils::FileUtils::readFile(filePath);
            size_t fileSize = content.size();
            
            if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
                std::cout << "Skipping " << filePath << " (empty or too large: " << fileSize << " bytes)\n";
                continue;
            }
            
            FileChange change;
            change.filePath = filePath;
            change.content = content;
            change.fileSize = fileSize;
            change.changeDescription = "Modified file";
            
            changes.push_back(change);
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not read file " << filePath << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    return changes;
}

bool ReviewGenerator::shouldProcessFile(const std::string& filePath) {
    // Skip backup files and review files
    if (filePath.find(".backup") != std::string::npos || 
        filePath.find("review.md") != std::string::npos) {
        return false;
    }
    
    return isSourceFile(filePath);
}

bool ReviewGenerator::isSourceFile(const std::string& filePath) {
    if (!vit::utils::FileUtils::fileExists(filePath)) {
        return false;
    }
    
    std::string ext = vit::utils::FileUtils::getFileExtension(filePath);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Same source extensions as comment generator
    static const std::set<std::string> sourceExtensions = {
        ".cpp", ".cxx", ".cc", ".c",           // C/C++
        ".hpp", ".hxx", ".h", ".hh",           // C/C++ headers
        ".py", ".pyx",                         // Python
        ".js", ".jsx", ".mjs",                 // JavaScript
        ".ts", ".tsx",                         // TypeScript
        ".java",                               // Java
        ".cs",                                 // C#
        ".go",                                 // Go
        ".rs",                                 // Rust
        ".php",                                // PHP
        ".rb",                                 // Ruby
        ".swift",                              // Swift
        ".kt", ".kts",                         // Kotlin
        ".scala",                              // Scala
        ".m", ".mm",                           // Objective-C
        ".dart",                               // Dart
        ".lua",                                // Lua
        ".r", ".R",                            // R
        ".jl",                                 // Julia
        ".hs",                                 // Haskell
        ".ml", ".mli",                         // OCaml
        ".fs", ".fsx",                         // F#
        ".clj", ".cljs", ".cljc",             // Clojure
        ".ex", ".exs",                         // Elixir
        ".erl", ".hrl",                        // Erlang
        ".vim",                                // Vim script
        ".sh", ".bash", ".zsh",                // Shell scripts
        ".ps1",                                // PowerShell
        ".sql"                                 // SQL
    };
    
    return sourceExtensions.count(ext) > 0;
}

size_t ReviewGenerator::estimateTokens(const std::string& text) {
    // Conservative estimate: 1 token ≈ 3.5 characters for code
    return text.size() / 3.5;
}

bool ReviewGenerator::validateTokenLimits(const std::vector<FileChange>& changes) {
    size_t totalTokens = 0;
    size_t promptOverhead = 700; // System + user prompt formatting
    
    for (const auto& change : changes) {
        totalTokens += estimateTokens(change.content);
        totalTokens += 50; // File header formatting overhead
    }
    
    // Check against available input tokens (accounting for response space)
    size_t maxInputTokens = 2096 - promptOverhead; // ~1400 tokens
    
    if (totalTokens > maxInputTokens) {
        std::cerr << "Token limit exceeded: " << totalTokens << " > " << maxInputTokens << std::endl;
        return false;
    }
    
    return true;
}

std::vector<vit::ai::AIClient::Message> ReviewGenerator::createReviewPrompt(const std::vector<FileChange>& changes) {
    std::string systemPrompt = R"(You are an expert code reviewer with deep knowledge of software engineering best practices, security, performance, and maintainability.

Your task is to provide a comprehensive code review for all the files provided. Focus on:

1. **Code Quality Issues**: Logic errors, potential bugs, edge cases
2. **Security Concerns**: Injection vulnerabilities, input validation, authentication issues  
3. **Performance Issues**: Inefficient algorithms, memory leaks, unnecessary operations
4. **Best Practices**: Code style, naming conventions, design patterns
5. **Maintainability**: Code clarity, documentation, modularity

Format your response as follows:

# Code Review

## Summary
Brief overview of the codebase and overall assessment.

## Issues Found
### 🔴 Critical Issues
- List any critical problems that could cause crashes, security vulnerabilities, or data loss

### 🟡 Warnings  
- List moderate issues that should be addressed but aren't critical

### 🔵 Suggestions
- List minor improvements and style suggestions

## File-by-File Analysis
For each file, provide specific observations and recommendations.

## Next Steps
Recommendations for follow-up work, additional testing, or improvements to consider.

## Overall Assessment
Rate the code quality and provide final recommendations.)";

    std::stringstream userPrompt;
    userPrompt << "Please review this codebase. Here are the source files:\n\n";
    
    for (const auto& change : changes) {
        userPrompt << "**File: " << change.filePath << "**\n";
        userPrompt << "```\n" << change.content << "\n```\n\n";
    }
    
    userPrompt << "Please provide a comprehensive review focusing on code quality, security, performance, and maintainability.";

    return {
        vit::ai::AIClient::createSystemMessage(systemPrompt),
        vit::ai::AIClient::createUserMessage(userPrompt.str())
    };
}

std::string ReviewGenerator::formatReviewContent(const std::string& aiResponse) {
    std::stringstream formatted;
    
    formatted << "# AI Code Review\n";
    formatted << "Generated on: " << getCurrentTimestamp() << "\n";
    formatted << "Review Tool: vit --review\n";
    formatted << "AI Provider: " << aiClient_->getProviderName() << "\n\n";
    formatted << "---\n\n";
    formatted << aiResponse << "\n\n";
    formatted << "---\n";
    formatted << "This review was automatically generated by AI. Please use human judgment for final decisions.\n";
    
    return formatted.str();
}

std::string ReviewGenerator::getCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace vit::features 
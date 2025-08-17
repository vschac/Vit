#include "commit_splitter.hpp"
#include "../utils/file_utils.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vit::features {

CommitSplitter::CommitSplitter(std::shared_ptr<vit::ai::AIClient> aiClient, std::string userName, std::string userEmail) 
    : aiClient_(aiClient), changeAnalyzer_(aiClient), userName(userName), userEmail(userEmail) {}

CommitSplitter::SplitResult CommitSplitter::analyzeAndSuggestSplits(const std::string& commitHash, 
                                                                   const std::string& fallbackMessage) {
    try {
        auto analysisResult = changeAnalyzer_.analyzeChanges(commitHash, true);  // Source files only
        
        if (!analysisResult.hasChanges()) {
            return SplitResult::Error("No changes detected");
        }
        
        std::cout << "Analyzing " << analysisResult.sourceFilesChanged << " changed file(s)...\n";
        
        auto messages = createAnalysisPrompt(analysisResult.changes);
        auto future = aiClient_->generateResponse(messages);
        auto aiResult = future.get();
        
        if (!aiResult.success) {
            std::cerr << "AI analysis failed: " << aiResult.error << std::endl;
            
            // Create fallback single commit
            CommitGroup fallbackGroup(fallbackMessage);
            for (const auto& change : analysisResult.changes) {
                fallbackGroup.filePaths.push_back(change.filePath);
            }
            fallbackGroup.description = "Multiple file changes (AI analysis failed)";
            fallbackGroup.category = "feat";
            
            return SplitResult::Success({fallbackGroup}, analysisResult.totalFilesAnalyzed, 
                                      analysisResult.sourceFilesChanged);
        }
        
        return parseAIResponse(aiResult.content, analysisResult.changes);
        
    } catch (const std::exception& e) {
        return SplitResult::Error("Analysis failed: " + std::string(e.what()));
    }
}

std::vector<vit::ai::AIClient::Message> CommitSplitter::createAnalysisPrompt(
    const std::vector<utils::ChangeAnalyzer::FileChange>& changes) {
    
    std::string systemPrompt = R"(You are an expert Git commit analyzer. Your task is to analyze file changes and group them into logical, atomic commits.

Guidelines:
- Group related changes together (e.g., all authentication changes, all UI changes)
- Each commit should be atomic and functional
- Use conventional commit format: type(scope): description
- Prefer fewer, meaningful commits over many tiny ones
- Consider dependencies between files

Respond in JSON format:
{
  "should_split": true/false,
  "reasoning": "Brief explanation of grouping decision",
  "commits": [
    {
      "message": "feat(auth): add user authentication system",
      "description": "Implements login, logout, and session management", 
      "files": ["auth.cpp", "user.hpp"],
      "category": "feat",
      "confidence": 8
    }
  ]
}

If should_split is false, provide a single commit with all files.)";

    std::string userPrompt = "Analyze these file changes and suggest commit groupings:\n\n";
    userPrompt += formatFileChangesForAI(changes);
    
    return {
        vit::ai::AIClient::createSystemMessage(systemPrompt),
        vit::ai::AIClient::createUserMessage(userPrompt)
    };
}

std::string CommitSplitter::formatFileChangesForAI(const std::vector<utils::ChangeAnalyzer::FileChange>& changes) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < changes.size(); ++i) {
        const auto& change = changes[i];
        
        oss << "**File " << (i + 1) << ": " << change.filePath << "**\n";
        oss << "Change type: " << getChangeTypeString(change.changeType) << "\n";
        
        if (change.changeType == utils::ChangeAnalyzer::ChangeType::ADDED) {
            oss << "New file content:\n```\n" 
                << change.newContent << "\n```\n\n";
        } else if (change.changeType == utils::ChangeAnalyzer::ChangeType::DELETED) {
            oss << "Deleted file content:\n```\n" 
                << change.oldContent << "\n```\n\n";
        } else if (change.changeType == utils::ChangeAnalyzer::ChangeType::MODIFIED) {
            oss << "Before:\n```\n" << change.oldContent << "\n```\n";
            oss << "After:\n```\n" << change.newContent << "\n```\n\n";
        }
    }
    
    return oss.str();
}

CommitSplitter::SplitResult CommitSplitter::parseAIResponse(const std::string& aiResponse, 
                                                           const std::vector<utils::ChangeAnalyzer::FileChange>& changes) {
    try {
        auto jsonResponse = json::parse(aiResponse);
        
        bool shouldSplit = jsonResponse.value("should_split", false);
        std::string reasoning = jsonResponse.value("reasoning", "");
        
        std::vector<CommitGroup> groups;
        
        if (jsonResponse.contains("commits") && jsonResponse["commits"].is_array()) {
            for (const auto& commitJson : jsonResponse["commits"]) {
                CommitGroup group(commitJson.value("message", "Update files"));
                group.description = commitJson.value("description", "");
                group.category = commitJson.value("category", "feat");
                group.confidence = commitJson.value("confidence", 5);
                
                if (commitJson.contains("files") && commitJson["files"].is_array()) {
                    for (const auto& fileName : commitJson["files"]) {
                        if (fileName.is_string()) {
                            group.filePaths.push_back(fileName.get<std::string>());
                        }
                    }
                }
                
                if (validateCommitGroup(group)) {
                    groups.push_back(std::move(group));
                }
            }
        }
        
        // Fallback if parsing failed
        if (groups.empty()) {
            CommitGroup fallbackGroup("Update multiple files");
            for (const auto& change : changes) {
                fallbackGroup.filePaths.push_back(change.filePath);
            }
            fallbackGroup.description = "Multiple file changes";
            fallbackGroup.category = "feat";
            groups.push_back(std::move(fallbackGroup));
        }
        
        std::cout << "AI reasoning: " << reasoning << std::endl;
        
        return SplitResult::Success(groups, changes.size(), changes.size());
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse AI response: " << e.what() << std::endl;
        
        // Create fallback single commit
        CommitGroup fallbackGroup("Update multiple files");
        for (const auto& change : changes) {
            fallbackGroup.filePaths.push_back(change.filePath);
        }
        fallbackGroup.description = "Multiple file changes (AI response parsing failed)";
        fallbackGroup.category = "feat";
        
        return SplitResult::Success({fallbackGroup}, changes.size(), changes.size());
    }
}

bool CommitSplitter::executeSplits(const SplitResult& splits, bool dryRun) {
    if (!splits.success) {
        std::cerr << "Cannot execute splits: " << splits.error << std::endl;
        return false;
    }
    
    if (dryRun) {
        std::cout << "\n=== DRY RUN: Proposed Commits ===\n";
        for (size_t i = 0; i < splits.groups.size(); ++i) {
            const auto& group = splits.groups[i];
            std::cout << "Commit " << (i + 1) << ": " << group.commitMessage << std::endl;
            std::cout << "  Description: " << group.description << std::endl;
            std::cout << "  Files (" << group.filePaths.size() << "):" << std::endl;
            for (const auto& file : group.filePaths) {
                std::cout << "    - " << file << std::endl;
            }
            std::cout << std::endl;
        }
        return true;
    }
    
    // Execute actual commits
    std::cout << "Executing " << splits.groups.size() << " commit(s)...\n";
    
    for (size_t i = 0; i < splits.groups.size(); ++i) {
        const auto& group = splits.groups[i];
        std::cout << "Creating commit " << (i + 1) << "/" << splits.groups.size() 
                  << ": " << group.commitMessage << std::endl;
        
        if (!createCommitFromGroup(group)) {
            std::cerr << "Failed to create commit for group: " << group.commitMessage << std::endl;
            return false;
        }
    }
    
    std::cout << "✓ Successfully created " << splits.groups.size() << " commit(s)" << std::endl;
    return true;
}

bool CommitSplitter::createCommitFromGroup(const CommitGroup& group) {
    try {
        std::string treeHash = writeTree(".");
        if (treeHash.empty()) {
            std::cerr << "Failed to create tree for commit group" << std::endl;
            return false;
        }
        
        std::string parentHash = readHead();
        
        std::string author = userName;
        std::string email = userEmail;
        
        std::string commitHash = writeCommit(treeHash, parentHash, group.commitMessage, author, email);
        if (commitHash.empty()) {
            std::cerr << "Failed to create commit object" << std::endl;
            return false;
        }
        
        // Update HEAD and branch
        std::string currentBranch = getCurrentBranch();
        if (!currentBranch.empty()) {
            if (!updateBranch(currentBranch, commitHash)) {
                std::cerr << "Failed to update branch" << std::endl;
                return false;
            }
        } else {
            if (!writeHead(commitHash)) {
                std::cerr << "Failed to update HEAD" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ " << commitHash.substr(0, 8) << " " << group.commitMessage << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception creating commit: " << e.what() << std::endl;
        return false;
    }
}

bool CommitSplitter::validateCommitGroup(const CommitGroup& group) {
    if (group.commitMessage.empty()) {
        return false;
    }
    
    if (group.filePaths.empty()) {
        return false;
    }
    
    // Validate that all files exist
    for (const auto& filePath : group.filePaths) {
        if (!vit::utils::FileUtils::fileExists(filePath)) {
            std::cerr << "Warning: File " << filePath << " does not exist" << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string CommitSplitter::getChangeTypeString(utils::ChangeAnalyzer::ChangeType type) {
    switch (type) {
        case utils::ChangeAnalyzer::ChangeType::ADDED: return "Added";
        case utils::ChangeAnalyzer::ChangeType::MODIFIED: return "Modified";
        case utils::ChangeAnalyzer::ChangeType::DELETED: return "Deleted";
        default: return "Unknown";
    }
}

}
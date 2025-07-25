#pragma once
#include "../utils/change_analyzer.hpp"
#include "../ai/ai_client.hpp"
#include <string>
#include <vector>
#include <memory>

namespace vit::features {

class CommitSplitter {
public:
    struct CommitGroup {
        std::string commitMessage;          // AI-generated commit message
        std::string description;            // Brief description of changes
        std::vector<std::string> filePaths; // Files to include in this commit
        std::string category;               // e.g., "feat", "fix", "docs", "refactor"
        int confidence;                     // AI confidence (1-10)
        
        CommitGroup(const std::string& message) : commitMessage(message), confidence(5) {}
    };

    struct SplitResult {
        bool success;
        std::vector<CommitGroup> groups;
        std::string error;
        size_t totalFiles;
        size_t analyzedFiles;
        
        static SplitResult Success(const std::vector<CommitGroup>& groups, size_t total, size_t analyzed) {
            return SplitResult{true, groups, "", total, analyzed};
        }
        
        static SplitResult Error(const std::string& error) {
            return SplitResult{false, {}, error, 0, 0};
        }
        
        bool shouldSplit() const { return success && groups.size() > 1; }
    };

    explicit CommitSplitter(std::unique_ptr<vit::ai::AIClient> aiClient);
    
    /**
     * Analyze changes and suggest commit splits
     * @param commitHash Commit to compare against (empty = HEAD)
     * @param fallbackMessage Message to use if no split is suggested
     */
    SplitResult analyzeAndSuggestSplits(const std::string& commitHash = "", 
                                       const std::string& fallbackMessage = "Update multiple files");

    /**
     * Execute the suggested commit splits
     * @param splits Result from analyzeAndSuggestSplits
     * @param dryRun If true, only show what would be committed
     */
    bool executeSplits(const SplitResult& splits, bool dryRun = false);

private:
    std::unique_ptr<vit::ai::AIClient> aiClient_;
    utils::ChangeAnalyzer changeAnalyzer_;
    
    // AI analysis
    std::vector<vit::ai::AIClient::Message> createAnalysisPrompt(const std::vector<utils::ChangeAnalyzer::FileChange>& changes);
    SplitResult parseAIResponse(const std::string& aiResponse, const std::vector<utils::ChangeAnalyzer::FileChange>& changes);
    
    // Commit execution
    bool createCommitFromGroup(const CommitGroup& group);
    std::string createTreeFromFiles(const std::vector<std::string>& filePaths);
    bool validateCommitGroup(const CommitGroup& group);
    
    // Helper functions
    std::string formatFileChangesForAI(const std::vector<utils::ChangeAnalyzer::FileChange>& changes);
    std::string truncateContent(const std::string& content, size_t maxLength = 2000);
    std::string getChangeTypeString(utils::ChangeAnalyzer::ChangeType type);
};

}
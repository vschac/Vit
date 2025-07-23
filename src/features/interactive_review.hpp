
#pragma once
#include "comment_generator.hpp"
#include <vector>

namespace vit::features {

class InteractiveReview {
public:
    enum class ReviewAction {
        ACCEPT,
        REJECT,
        SHOW_DIFF,
        SKIP,
        QUIT
    };
    
    struct ReviewResult {
        bool shouldProceed;
        std::vector<CommentGenerator::CommentResult> accepted;
        std::vector<std::string> rejected;
    };
    
    ReviewResult reviewComments(std::vector<CommentGenerator::CommentResult>& results);

private:
    ReviewAction reviewSingleFile(const CommentGenerator::CommentResult& result);
    
    void showDiff(const std::string& original, const std::string& modified, const std::string& fileName);
    
    ReviewAction promptUserAction();
    
    void showSummary(const std::string& original, const std::string& modified);
};

}
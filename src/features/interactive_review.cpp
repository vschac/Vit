#include "interactive_review.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace vit::features {

InteractiveReview::ReviewResult InteractiveReview::reviewComments(std::vector<CommentGenerator::CommentResult>& results) {
    ReviewResult reviewResult;
    reviewResult.shouldProceed = true;
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "REVIEWING AI-GENERATED COMMENTS" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Filter out failed results
    auto successfulResults = std::vector<CommentGenerator::CommentResult>{};
    for (const auto& result : results) {
        if (result.success) {
            successfulResults.push_back(result);
        } else {
            std::cout << "!!  Skipped " << result.fileName << ": " << result.error << std::endl;
        }
    }
    
    if (successfulResults.empty()) {
        std::cout << "No files were successfully processed for comment generation." << std::endl;
        reviewResult.shouldProceed = false;
        return reviewResult;
    }
    
    std::cout << "\nFound " << successfulResults.size() << " file(s) with AI-generated comments to review.\n" << std::endl;
    
    for (size_t i = 0; i < successfulResults.size(); ++i) {
        auto& result = successfulResults[i];
        
        std::cout << "\n" << std::string(50, '-') << std::endl;
        std::cout << "File " << (i + 1) << "/" << successfulResults.size() << ": " << result.fileName << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        bool fileDecided = false;
        while (!fileDecided) {
            ReviewAction action = reviewSingleFile(result);
            
            switch (action) {
                case ReviewAction::ACCEPT:
                    reviewResult.accepted.push_back(result);
                    std::cout << "✓ Accepted changes for " << result.fileName << std::endl;
                    fileDecided = true;
                    break;
                    
                case ReviewAction::REJECT:
                    reviewResult.rejected.push_back(result.fileName);
                    std::cout << "✗ Rejected changes for " << result.fileName << std::endl;
                    fileDecided = true;
                    break;
                    
                case ReviewAction::SHOW_DIFF:
                    showDiff(result.originalContent, result.modifiedContent, result.fileName);
                    // Don't set fileDecided - let user choose again
                    break;
                    
                case ReviewAction::SKIP:
                    reviewResult.rejected.push_back(result.fileName);
                    std::cout << "⏭️  Skipped " << result.fileName << std::endl;
                    fileDecided = true;
                    break;
                    
                case ReviewAction::QUIT:
                    std::cout << "Review cancelled." << std::endl;
                    reviewResult.shouldProceed = false;
                    return reviewResult;
            }
        }
    }
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "REVIEW SUMMARY" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "✓ Accepted: " << reviewResult.accepted.size() << " file(s)" << std::endl;
    std::cout << "✗ Rejected: " << reviewResult.rejected.size() << " file(s)" << std::endl;
    
    if (!reviewResult.accepted.empty()) {
        std::cout << "\nFiles to be modified:" << std::endl;
        for (const auto& accepted : reviewResult.accepted) {
            std::cout << "  - " << accepted.fileName << std::endl;
        }
    }
    
    return reviewResult;
}

InteractiveReview::ReviewAction InteractiveReview::reviewSingleFile(const CommentGenerator::CommentResult& result) {
    showSummary(result.originalContent, result.modifiedContent);
    return promptUserAction();
}

void InteractiveReview::showSummary(const std::string& original, const std::string& modified) {
    // Count lines and estimate added comments
    auto originalLines = std::count(original.begin(), original.end(), '\n');
    auto modifiedLines = std::count(modified.begin(), modified.end(), '\n');
    auto addedLines = modifiedLines - originalLines;
    
    std::cout << "Summary of changes:" << std::endl;
    std::cout << "  Original lines: " << originalLines << std::endl;
    std::cout << "  Modified lines: " << modifiedLines << std::endl;
    std::cout << "  Added lines: " << addedLines << " (estimated comments)" << std::endl;
    
    // Show a preview of the beginning of the modified file
    std::cout << "\nPreview of modified file (first 10 lines):" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    std::istringstream stream(modified);
    std::string line;
    int lineCount = 0;
    while (std::getline(stream, line) && lineCount < 10) {
        std::cout << std::setw(3) << (lineCount + 1) << " | " << line << std::endl;
        lineCount++;
    }
    
    if (lineCount == 10) {
        std::cout << "    | ..." << std::endl;
    }
    std::cout << std::string(40, '-') << std::endl;
}

void InteractiveReview::showDiff(const std::string& original, const std::string& modified, const std::string& fileName) {
    std::cout << "\nDetailed diff for " << fileName << ":" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::istringstream originalStream(original);
    std::istringstream modifiedStream(modified);
    
    std::string origLine, modLine;
    int lineNum = 1;
    
    while (std::getline(originalStream, origLine) || std::getline(modifiedStream, modLine)) {
        if (origLine != modLine) {
            if (!origLine.empty()) {
                std::cout << "- " << std::setw(3) << lineNum << " | " << origLine << std::endl;
            }
            if (!modLine.empty()) {
                std::cout << "+ " << std::setw(3) << lineNum << " | " << modLine << std::endl;
            }
        }
        lineNum++;
        
        if (originalStream.eof()) origLine.clear();
        if (modifiedStream.eof()) modLine.clear();
    }
    
    std::cout << std::string(60, '=') << std::endl;
}

InteractiveReview::ReviewAction InteractiveReview::promptUserAction() {
    std::cout << "\nWhat would you like to do?" << std::endl;
    std::cout << "  [a]ccept  [r]eject  [d]iff  [s]kip  [q]uit" << std::endl;
    std::cout << "Choice: ";
    
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        input = "a";
    }
    
    char choice = std::tolower(input[0]);
    
    switch (choice) {
        case 'a': return ReviewAction::ACCEPT;
        case 'r': return ReviewAction::REJECT;
        case 'd': return ReviewAction::SHOW_DIFF;
        case 's': return ReviewAction::SKIP;
        case 'q': return ReviewAction::QUIT;
        default:
            std::cout << "Invalid choice. Please enter a, r, d, s, or q." << std::endl;
            return promptUserAction();
    }
}

} 
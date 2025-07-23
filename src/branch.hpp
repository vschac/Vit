#pragma once
#include <string>

std::string getCurrentBranch();

bool updateBranch(const std::string& branchName,
                  const std::string& commitHash);

bool switchToBranch(const std::string& branchName);

// Convenience: write commit hash to <branch> and switch HEAD.
bool writeHeadAsBranch(const std::string& commitHash,
                       const std::string& branchName = "main");

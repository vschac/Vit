#pragma once
#include <string>

/*  -------- Branch-level helpers --------  */

// Returns the currently-checked-out branch name,
// or an empty string when HEAD is detached.
std::string getCurrentBranch();

// Persist <commitHash> to refs/heads/<branchName>.
bool updateBranch(const std::string& branchName,
                  const std::string& commitHash);

// Make HEAD point at refs/heads/<branchName>.
bool switchToBranch(const std::string& branchName);

// Convenience: write commit hash to <branch> and switch HEAD.
bool writeHeadAsBranch(const std::string& commitHash,
                       const std::string& branchName = "main");

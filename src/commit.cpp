#include "commit.hpp"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <ctime>
#include <set>
#include <unordered_set>
#include <queue>

#include <zlib.h>
#include <openssl/sha.h>


// ──────────────────────────  zlib helpers  ──────────────────────────
std::string decompressObject(const std::string& compressed)
{
    z_stream strm{};
    strm.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    strm.avail_in  = compressed.size();

    if (inflateInit(&strm) != Z_OK) {
        std::cerr << "inflateInit failed\n";
        return {};
    }

    std::string out;
    char        buf[4096];

    int ret;
    do {
        strm.next_out  = reinterpret_cast<Bytef*>(buf);
        strm.avail_out = sizeof(buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            std::cerr << "inflate error: " << ret << '\n';
            inflateEnd(&strm);
            return {};
        }
        out.append(buf, sizeof(buf) - strm.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return out;
}



/* ──────────────────────────  Low-level helpers  ────────────────────────── */

std::string hashToHexString(const unsigned char* hash)
{
    std::ostringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    return ss.str();
}

std::string hexStringToBinary(const std::string& hex)
{
    std::string bin;
    for (int i = 0; i < 20; ++i) {
        std::string byteStr = hex.substr(i * 2, 2);
        char byte          = static_cast<char>(std::stoi(byteStr, nullptr, 16));
        bin += byte;
    }
    return bin;
}

std::string binaryToHexString(const std::string& bin)
{
    std::ostringstream ss;
    for (unsigned char c : bin)
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(c);
    return ss.str();
}

std::string getObjectPath(const std::string& hash)
{
    return ".git/objects/" + hash.substr(0, 2) + '/' + hash.substr(2);
}


/* ──────────────────────────  object writers / readers  ────────────────────────── */

std::string writeObject(const std::string& type, const std::string& content)
{
    const std::string header = type + ' ' + std::to_string(content.size()) + '\0';
    const std::string full   = header + content;

    unsigned char sha[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(full.data()), full.size(), sha);
    const std::string hash = hashToHexString(sha);

    // compress
    uLong dstSize = compressBound(full.size());
    std::string compressed(dstSize, '\0');
    if (compress(reinterpret_cast<Bytef*>(&compressed[0]), &dstSize,
                 reinterpret_cast<const Bytef*>(full.data()), full.size()) != Z_OK) {
        std::cerr << "Compression failed\n";
        return {};
    }
    compressed.resize(dstSize);

    // write to disk
    const std::string dir  = ".git/objects/" + hash.substr(0, 2);
    std::filesystem::create_directories(dir);
    std::ofstream(getObjectPath(hash), std::ios::binary)
        .write(compressed.data(), compressed.size());

    return hash;
}

std::string writeBlob(const std::string& content)         { return writeObject("blob",  content); }
std::string writeTree(const std::string& dirPath);


/* ──────────────────────────  read helpers  ────────────────────────── */

std::string readObject(const std::string& hash)
{
    std::ifstream in(getObjectPath(hash), std::ios::binary);
    if (!in) {
        std::cerr << "Object not found: " << hash << '\n';
        return {};
    }
    std::ostringstream ss; ss << in.rdbuf();
    return decompressObject(ss.str());
}

std::string readObjectContent(const std::string& hash)
{
    const std::string decompressed = readObject(hash);
    const auto nullPos             = decompressed.find('\0');
    return nullPos == std::string::npos ? decompressed
                                        : decompressed.substr(nullPos + 1);
}


/* ──────────────────────────  tree handling  ────────────────────────── */

std::string writeTree(const std::string& dirPath)
{
    std::vector<TreeEntry> entries;

    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        const std::string name = entry.path().filename().string();
        if (name == ".git") continue;

        TreeEntry e;
        e.filename = name;

        if (entry.is_regular_file()) {
            std::ifstream f(entry.path(), std::ios::binary);
            std::ostringstream ss; ss << f.rdbuf();
            e.hash = writeBlob(ss.str());
            e.mode = "100644";
        }
        else if (entry.is_directory()) {
            e.hash = writeTree(entry.path().string());
            e.mode = "40000";
        }
        if (e.hash.empty()) return {};
        entries.push_back(e);
    }

    std::sort(entries.begin(), entries.end(),
              [](const TreeEntry& a, const TreeEntry& b){ return a.filename < b.filename; });

    std::string treeContent;
    for (const auto& e : entries) {
        treeContent += e.mode + ' ' + e.filename + '\0';
        treeContent += hexStringToBinary(e.hash);
    }
    return writeObject("tree", treeContent);
}

std::vector<FileInfo> parseTree(const std::string& treeHash)
{
    std::vector<FileInfo> files;
    const std::string content = readObjectContent(treeHash);
    if (content.empty()) return files;

    size_t pos = 0;
    while (pos < content.size()) {
        const size_t nullPos = content.find('\0', pos);
        if (nullPos == std::string::npos) break;

        const std::string header = content.substr(pos, nullPos - pos);
        const size_t      spPos  = header.find_last_of(' ');
        if (spPos != std::string::npos) {
            FileInfo f;
            f.mode = header.substr(0, spPos);
            f.name = header.substr(spPos + 1);
            const std::string binHash = content.substr(nullPos + 1, 20);
            f.hash        = binaryToHexString(binHash);
            f.isDirectory = f.mode.substr(0,3) == "400";
            files.push_back(f);
        }
        pos = nullPos + 1 + 20;   // skip hash
    }
    return files;
}

bool restoreTree(const std::string& treeHash, const std::string& base)
{
    for (const auto& f : parseTree(treeHash)) {
        const std::string path = base.empty() ? f.name : base + '/' + f.name;
        if (f.isDirectory) {
            std::filesystem::create_directories(path);
            if (!restoreTree(f.hash, path)) return false;
        } else {
            const std::string blob = readObjectContent(f.hash);
            if (blob.empty()) return false;

            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            std::ofstream out(path, std::ios::binary);
            if (!out) return false;
            out << blob;
        }
    }
    return true;
}

void collectTreeFiles(const std::string& treeHash,
                      const std::string& base,
                      std::set<std::string>& out)
{
    for (const auto& f : parseTree(treeHash)) {
        const std::string path = base.empty() ? f.name : base + '/' + f.name;
        if (f.isDirectory)
            collectTreeFiles(f.hash, path, out);
        else
            out.insert(path);
    }
}

bool restoreTreeOverwrite(const std::string& treeHash, const std::string& base)
{
    for (const auto& f : parseTree(treeHash)) {
        const std::string path = base.empty() ? f.name : base + '/' + f.name;
        if (f.isDirectory) {
            std::filesystem::create_directories(path);
            if (!restoreTreeOverwrite(f.hash, path)) return false;
        } else {
            const std::string blob = readObjectContent(f.hash);
            if (blob.empty()) return false;

            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            std::ofstream out(path, std::ios::binary);
            if (!out) return false;
            out << blob;
        }
    }
    return true;
}


/* ──────────────────────────  commit / HEAD primitives  ────────────────────────── */

std::string writeCommit(const std::string& tree,
                        const std::string& parent,
                        const std::string& message,
                        const std::string& author,
                        const std::string& email)
{
    std::string c =
        "tree "      + tree   + '\n' +
        (parent.empty() ? "" : "parent " + parent + '\n') +
        "author "    + author + " <" + email + "> " + std::to_string(std::time(nullptr)) + '\n' +
        "committer " + author + " <" + email + "> " + std::to_string(std::time(nullptr)) + "\n\n" +
        message + '\n';
    return writeObject("commit", c);
}

std::string readHead()
{
    std::ifstream in(".git/HEAD");
    if (!in) return {};
    std::string line; std::getline(in, line);

    if (line.rfind("ref: ", 0) == 0) {          // symbolic ref
        std::ifstream ref(".git/" + line.substr(5));
        std::string   hash; std::getline(ref, hash);
        return hash;
    }
    return line;                                // detached
}

bool writeHead(const std::string& hash)
{
    std::ofstream(".git/HEAD") << hash << '\n';
    return true;
}


/* ──────────────────────────  commit parsing / pretty helpers  ────────────────────────── */

CommitInfo parseCommit(const std::string& commitHash)
{
    CommitInfo info;
    std::string content = readObjectContent(commitHash);
    if (content.empty()) return info;

    info.hash = commitHash;
    std::istringstream in(content);
    std::string line;
    while (std::getline(in, line)) {
        if      (line.rfind("tree ",   0) == 0) info.treeHash   = line.substr(5);
        else if (line.rfind("parent ", 0) == 0) info.parentHash = line.substr(7);
        else if (line.rfind("author ", 0) == 0) {
            size_t lt = line.find('<'), gt = line.find('>');
            if (lt != std::string::npos && gt != std::string::npos)
                info.author = line.substr(7, lt - 8);
            if (gt + 2 < line.size()) info.timestamp = line.substr(gt + 2);
        }
        else if (line.empty()) {                 // message starts
            std::ostringstream msg;
            while (std::getline(in, line)) { msg << line << '\n'; }
            info.message = msg.str();
            if (!info.message.empty() && info.message.back() == '\n')
                info.message.pop_back();
            break;
        }
    }
    return info;
}

std::string formatTimestamp(const std::string& ts)
{
    if (ts.empty()) return {};
    try {
        time_t t = std::stol(ts);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return buf;
    } catch (...) { return ts; }
}


/* ──────────────────────────  working-dir helpers  ────────────────────────── */

std::set<std::string> getWorkingDirectoryFiles(const std::string& path)
{
    std::set<std::string> out;
    for (const auto& e : std::filesystem::recursive_directory_iterator(path)) {
        if (!e.is_regular_file()) continue;
        std::string p = e.path().string();
        if (p.find("/.git") != std::string::npos) continue;
        if (p.rfind("./", 0) == 0) p.erase(0, 2);
        out.insert(p);
    }
    return out;
}


/* ──────────────────────────  high-level ops  ────────────────────────── */

bool safeCheckout(const std::string& commitHash)
{
    const CommitInfo ci = parseCommit(commitHash);
    if (ci.hash.empty()) {
        std::cerr << "Invalid commit: " << commitHash << '\n';
        return false;
    }

    std::cout << "Checking out " << commitHash << " – " << ci.message << '\n';

    // restore files
    if (!restoreTreeOverwrite(ci.treeHash)) return false;

    // clean untracked
    std::set<std::string> expected;
    collectTreeFiles(ci.treeHash, "", expected);

    for (const auto& f : getWorkingDirectoryFiles()) {
        if (!expected.count(f)) {
            std::filesystem::remove(f);
            std::cout << "Removed " << f << '\n';
        }
    }

    writeHead(commitHash);
    std::cout << "Checkout complete\n";
    return true;
}


/* ──────────────────────────  reachability / refs  ────────────────────────── */

std::vector<std::string> findAllCommitHashes()
{
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;

    for (const auto& dir : std::filesystem::directory_iterator(".git/objects")) {
        if (!dir.is_directory() || dir.path().filename().string().size() != 2) continue;
        for (const auto& file : std::filesystem::directory_iterator(dir.path())) {
            const std::string hash = dir.path().filename().string() + file.path().filename().string();
            if (hash.size() != 40) continue;
            const std::string obj = readObject(hash);
            if (!obj.empty() && obj.rfind("commit ", 0) == 0 && !seen.count(hash)) {
                out.push_back(hash); seen.insert(hash);
            }
        }
    }
    return out;
}

std::vector<std::string> getReachableCommits(const std::vector<std::string>& start)
{
    std::vector<std::string> order;
    std::unordered_set<std::string> vis;
    std::queue<std::string> q;

    for (const auto& h : start) if (!h.empty() && vis.insert(h).second) q.push(h);

    while (!q.empty()) {
        const std::string cur = q.front(); q.pop();
        order.push_back(cur);
        const auto parent = parseCommit(cur).parentHash;
        if (!parent.empty() && vis.insert(parent).second)
            q.push(parent);
    }
    return order;
}

std::vector<std::string> collectReferenceCommits()
{
    std::vector<std::string> refs;
    if (const std::string h = readHead(); !h.empty()) refs.push_back(h);

    const std::string refsDir = ".git/refs/heads";
    if (std::filesystem::exists(refsDir)) {
        for (const auto& e : std::filesystem::directory_iterator(refsDir)) {
            std::ifstream rf(e.path());
            std::string   hash; std::getline(rf, hash);
            if (!hash.empty()) refs.push_back(hash);
        }
    }
    return refs;
}

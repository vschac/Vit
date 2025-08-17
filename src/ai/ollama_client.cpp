#include "ollama_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace vit::ai {

// PIMPL implementation to hide curl details
struct OllamaClient::Impl {
    Config config;
    
    Impl(const Config& cfg) : config(cfg) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~Impl() {
        curl_global_cleanup();
    }
};

// Callback function for curl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

OllamaClient::OllamaClient(const Config& config) : pImpl_(std::make_unique<Impl>(config)) {}

OllamaClient::~OllamaClient() = default;

// future enables async generation
std::future<AIClient::GenerationResult> OllamaClient::generateResponse(const std::vector<Message>& messages) {
    // async enables it to run in a separate thread, 
    return std::async(std::launch::async, [this, messages]() {
        return makeRequest(messages);
    });
}

bool OllamaClient::isAvailable() const {
    return !pImpl_->config.baseUrl.empty() && validateConfig();
}

std::string OllamaClient::getProviderName() const {
    return "Ollama";
}

std::string OllamaClient::getModelName() const {
    return pImpl_->config.model;
}

AIClient::GenerationResult OllamaClient::makeRequest(const std::vector<Message>& messages) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return GenerationResult::Error("Failed to initialize CURL");
    }

    std::string response;
    CURLcode res;

    try {
        std::string jsonPayload = createJsonPayload(messages);
    
        // Headers (Ollama doesn't need authorization)
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        std::string url = pImpl_->config.baseUrl + "/api/chat";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // Longer timeout for local models
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return GenerationResult::Error("CURL error: " + std::string(curl_easy_strerror(res)));
        }

        if (httpCode != 200) {
            return GenerationResult::Error("HTTP error " + std::to_string(httpCode) + ": " + response);
        }

        return parseResponse(response);

    } catch (const std::exception& e) {
        curl_easy_cleanup(curl);
        return GenerationResult::Error("Request failed: " + std::string(e.what()));
    }
}

std::string OllamaClient::createJsonPayload(const std::vector<Message>& messages) {
    json payload = {
        {"model", pImpl_->config.model},
        {"stream", false}  // Get complete response, not streaming
    };

    json messagesJson = json::array();
    for (const auto& msg : messages) {
        messagesJson.push_back({
            {"role", msg.role},
            {"content", msg.content}
        });
    }
    payload["messages"] = messagesJson;

    return payload.dump();
}

AIClient::GenerationResult OllamaClient::parseResponse(const std::string& jsonResponse) {
    try {
        json parsed = json::parse(jsonResponse);
        
        if (parsed.contains("error")) {
            std::string errorMsg = "Ollama API error";
            if (parsed["error"].is_string()) {
                errorMsg = parsed["error"].get<std::string>();
            }
            return GenerationResult::Error(errorMsg);
        }

        // Extract the response content from Ollama format
        if (parsed.contains("message") && parsed["message"].contains("content")) {
            std::string content = parsed["message"]["content"].get<std::string>();
            return GenerationResult::Success(content);
        }

        return GenerationResult::Error("Invalid response format from Ollama API");

    } catch (const std::exception& e) {
        return GenerationResult::Error("JSON parsing error: " + std::string(e.what()));
    }
}

bool OllamaClient::validateConfig() const {
    return !pImpl_->config.baseUrl.empty() && 
           !pImpl_->config.model.empty();
}

}
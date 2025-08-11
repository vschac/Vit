#include "openai_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace vit::ai {

// PIMPL implementation to hide curl details
struct OpenAIClient::Impl {
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

OpenAIClient::OpenAIClient(const std::string& apiKey) : pImpl_(std::make_unique<Impl>(Config{apiKey})) {}

OpenAIClient::~OpenAIClient() = default;

// future enables async generation
std::future<AIClient::GenerationResult> OpenAIClient::generateResponse(const std::vector<Message>& messages) {
    // async enables it to run in a separate thread, 
    return std::async(std::launch::async, [this, messages]() {
        return makeRequest(messages);
    });
}

bool OpenAIClient::isAvailable() const {
    return !pImpl_->config.apiKey.empty() && validateConfig();
}

std::string OpenAIClient::getProviderName() const {
    return "OpenAI";
}

std::string OpenAIClient::getModelName() const {
    return pImpl_->config.model;
}

AIClient::GenerationResult OpenAIClient::makeRequest(const std::vector<Message>& messages) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return GenerationResult::Error("Failed to initialize CURL");
    }

    std::string response;
    CURLcode res;

    try {
        std::string jsonPayload = createJsonPayload(messages);
        
        // headers
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + pImpl_->config.apiKey;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, authHeader.c_str());

        // CURL options
        std::string url = pImpl_->config.baseUrl + "/chat/completions";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, pImpl_->config.timeoutSeconds);
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

std::string OpenAIClient::createJsonPayload(const std::vector<Message>& messages) {
    json payload = {
        {"model", pImpl_->config.model},
        {"temperature", pImpl_->config.temperature}
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

AIClient::GenerationResult OpenAIClient::parseResponse(const std::string& jsonResponse) {
    try {
        json parsed = json::parse(jsonResponse);
        
        if (parsed.contains("error")) {
            std::string errorMsg = "OpenAI API error";
            if (parsed["error"].contains("message")) {
                errorMsg = parsed["error"]["message"].get<std::string>();
            }
            return GenerationResult::Error(errorMsg);
        }

        // Extract the response content
        if (parsed.contains("choices") && !parsed["choices"].empty()) {
            const auto& choice = parsed["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string content = choice["message"]["content"].get<std::string>();
                return GenerationResult::Success(content);
            }
        }

        return GenerationResult::Error("Invalid response format from OpenAI API");

    } catch (const std::exception& e) {
        return GenerationResult::Error("JSON parsing error: " + std::string(e.what()));
    }
}

bool OpenAIClient::validateConfig() const {
    return !pImpl_->config.apiKey.empty() && 
           !pImpl_->config.model.empty() && 
           pImpl_->config.temperature >= 0.0 && 
           pImpl_->config.temperature <= 1.0;
}

}
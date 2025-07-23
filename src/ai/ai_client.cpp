#include "ai_client.hpp"
#include "openai_client.hpp"
// #include "ollama_client.hpp"
#include <cstdlib>

namespace vit::ai {

std::unique_ptr<AIClient> AI::createOpenAI(const std::string& apiKey) {
    if (apiKey.empty()) {
        return nullptr;
    }
    
    try {
        OpenAIClient::Config config{apiKey};
        return std::make_unique<OpenAIClient>(config.apiKey);
    } catch (const std::exception&) {
        return nullptr;
    }
}

std::unique_ptr<AIClient> AI::createOllama(const std::string& baseUrl, const std::string& model) {
    return nullptr;
}

std::string AI::getEnvVar(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string();
}

} 
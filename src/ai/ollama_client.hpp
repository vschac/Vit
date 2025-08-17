#pragma once
#include "ai_client.hpp"
#include <string>
#include <memory>

namespace vit::ai {

class OllamaClient : public AIClient {
public:

    struct Config {
        std::string baseUrl;
        std::string model;

        Config(const std::string& baseUrl, const std::string& model) : baseUrl(baseUrl), model(model) {}
    };

    explicit OllamaClient(const Config& config);
    ~OllamaClient() override;

    std::future<GenerationResult> generateResponse(const std::vector<Message>& messages) override;

    bool isAvailable() const override;
    std::string getProviderName() const override;
    std::string getModelName() const override;


private:
    struct Impl; // forward declaration
    std::unique_ptr<Impl> pImpl_; // pointer to the implementation to hide curl details

    GenerationResult makeRequest(const std::vector<Message>& messages);
    std::string createJsonPayload(const std::vector<Message>& messages);
    GenerationResult parseResponse(const std::string& jsonResponse);
    bool validateConfig() const;
};

}
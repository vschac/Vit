#pragma once
#include <string>
#include <vector>
#include <future>
#include <memory>

namespace vit::ai {

class AIClient {
public:
    struct Message {
        std::string role;
        std::string content;
        
        Message(const std::string& r, const std::string& c) : role(r), content(c) {}
    };

    struct GenerationResult {
        bool success;
        std::string content;
        std::string error;
        
        GenerationResult(bool s, const std::string& c, const std::string& e = "") 
            : success(s), content(c), error(e) {}
        
        static GenerationResult Success(const std::string& content) {
            return GenerationResult(true, content);
        }
        
        static GenerationResult Error(const std::string& error) {
            return GenerationResult(false, "", error);
        }
    };

    virtual ~AIClient() = default;

    virtual std::future<GenerationResult> generateResponse(
        const std::vector<Message>& messages) = 0;

    virtual bool isAvailable() const = 0;

    virtual std::string getProviderName() const = 0;

    virtual std::string getModelName() const = 0;

    static Message createUserMessage(const std::string& content) {
        return Message("user", content);
    }

    static Message createSystemMessage(const std::string& content) {
        return Message("system", content);
    }
};

class AI {
public:
    static std::unique_ptr<AIClient> createOpenAI(const std::string& apiKey);

    static std::unique_ptr<AIClient> createOllama(const std::string& baseUrl = "http://localhost:11434",
                                                  const std::string& model = "llama3.2");

    static std::string getEnvVar(const std::string& name);
};

} 
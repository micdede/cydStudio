import Foundation

/// OpenAI Chat Completions wire format. Works against the real OpenAI API,
/// but also against any compatible endpoint: OpenRouter, Groq, Together,
/// LM Studio's local server, etc. The user supplies the base URL.
///
/// We request response_format: json_object — supported by OpenAI's gpt-4o
/// family and most compatible servers; harmless when ignored.
struct OpenAICompatibleProvider: AIProvider {
    let displayName: String

    let baseURL: URL                // e.g. https://api.openai.com or https://openrouter.ai/api
    let model: String               // e.g. "gpt-4o", "google/gemini-2.5-pro"
    let apiKey: String

    init(displayName: String = "OpenAI-compatible",
         baseURL: URL = URL(string: "https://api.openai.com")!,
         model: String,
         apiKey: String) {
        self.displayName = displayName
        self.baseURL = baseURL
        self.model = model
        self.apiKey = apiKey
    }

    func generateLayout(systemPrompt: String, userPrompt: String) async throws -> String {
        guard !apiKey.isEmpty else { throw AIError.missingAPIKey }

        var req = URLRequest(url: baseURL.appendingPathComponent("v1/chat/completions"))
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
        req.timeoutInterval = 60

        let body: [String: Any] = [
            "model": model,
            "messages": [
                ["role": "system", "content": systemPrompt],
                ["role": "user",   "content": userPrompt]
            ],
            "response_format": ["type": "json_object"],
            "temperature": 0.4,
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (data, response) = try await URLSession.shared.data(for: req)
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            let code = (response as? HTTPURLResponse)?.statusCode ?? 0
            throw AIError.http(status: code, body: String(data: data, encoding: .utf8) ?? "")
        }

        let parsed = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        guard let choices = parsed?["choices"] as? [[String: Any]],
              let first = choices.first,
              let message = first["message"] as? [String: Any],
              let text = message["content"] as? String else {
            throw AIError.decoding("missing choices[0].message.content in OpenAI-compat response")
        }
        let cleaned = ResponseCleaner.unwrap(text)
        if cleaned.isEmpty { throw AIError.empty }
        return cleaned
    }
}

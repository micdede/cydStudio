import Foundation

/// Local Ollama server (https://ollama.com), default at http://localhost:11434.
/// No API key. Format: "json" hint asks the model to bias toward JSON.
struct OllamaProvider: AIProvider {
    let displayName = "Ollama (local)"

    let baseURL: URL                // default http://localhost:11434
    let model: String               // e.g. "llama3.2", "qwen2.5-coder"

    init(baseURL: URL = URL(string: "http://localhost:11434")!,
         model: String) {
        self.baseURL = baseURL
        self.model = model
    }

    func generateLayout(systemPrompt: String, userPrompt: String) async throws -> String {
        var req = URLRequest(url: baseURL.appendingPathComponent("api/chat"))
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.timeoutInterval = 120  // local models are often slow

        let body: [String: Any] = [
            "model": model,
            "stream": false,
            "format": "json",
            "messages": [
                ["role": "system", "content": systemPrompt],
                ["role": "user",   "content": userPrompt]
            ],
            "options": ["temperature": 0.4]
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (data, response) = try await URLSession.shared.data(for: req)
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            let code = (response as? HTTPURLResponse)?.statusCode ?? 0
            throw AIError.http(status: code, body: String(data: data, encoding: .utf8) ?? "")
        }

        let parsed = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        guard let message = parsed?["message"] as? [String: Any],
              let text = message["content"] as? String else {
            throw AIError.decoding("missing message.content in Ollama response")
        }
        let cleaned = ResponseCleaner.unwrap(text)
        if cleaned.isEmpty { throw AIError.empty }
        return cleaned
    }
}

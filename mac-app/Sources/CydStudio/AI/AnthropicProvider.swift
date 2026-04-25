import Foundation

/// Anthropic Messages API (https://docs.anthropic.com/en/api/messages).
///
/// We don't use tool_use for now — instructing the model to "respond with
/// raw JSON only" works reliably for Claude 3.5+ and keeps the wire format
/// simple. The defensive ResponseCleaner handles the rare fenced response.
struct AnthropicProvider: AIProvider {
    let displayName = "Anthropic"

    let baseURL: URL                // default: https://api.anthropic.com
    let model: String               // e.g. "claude-opus-4-7", "claude-sonnet-4-6"
    let apiKey: String

    init(baseURL: URL = URL(string: "https://api.anthropic.com")!,
         model: String,
         apiKey: String) {
        self.baseURL = baseURL
        self.model = model
        self.apiKey = apiKey
    }

    func generateLayout(systemPrompt: String, userPrompt: String) async throws -> String {
        guard !apiKey.isEmpty else { throw AIError.missingAPIKey }

        var req = URLRequest(url: baseURL.appendingPathComponent("v1/messages"))
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue(apiKey, forHTTPHeaderField: "x-api-key")
        req.setValue("2023-06-01", forHTTPHeaderField: "anthropic-version")
        req.timeoutInterval = 60

        let body: [String: Any] = [
            "model": model,
            "max_tokens": 4096,
            "system": systemPrompt,
            "messages": [
                ["role": "user", "content": userPrompt]
            ]
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (data, response) = try await URLSession.shared.data(for: req)
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            let code = (response as? HTTPURLResponse)?.statusCode ?? 0
            throw AIError.http(status: code, body: String(data: data, encoding: .utf8) ?? "")
        }

        let parsed = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        guard let content = parsed?["content"] as? [[String: Any]],
              let first = content.first,
              let text = first["text"] as? String else {
            throw AIError.decoding("missing content[0].text in Anthropic response")
        }
        let cleaned = ResponseCleaner.unwrap(text)
        if cleaned.isEmpty { throw AIError.empty }
        return cleaned
    }
}

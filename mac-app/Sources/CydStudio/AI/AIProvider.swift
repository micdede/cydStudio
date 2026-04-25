import Foundation

/// Generic interface every AI backend implements. The job is narrow:
/// take a system prompt + a user prompt and return a JSON layout string.
///
/// Implementations are responsible for any provider-specific quirks
/// (response-format hints, code-fence stripping, error mapping).
protocol AIProvider {
    var displayName: String { get }
    func generateLayout(systemPrompt: String, userPrompt: String) async throws -> String
}

enum AIError: LocalizedError {
    case missingAPIKey
    case http(status: Int, body: String)
    case decoding(String)
    case empty

    var errorDescription: String? {
        switch self {
        case .missingAPIKey:           "API key not set. Open AI settings to add one."
        case .http(let s, let body):   "HTTP \(s): \(body.prefix(300))"
        case .decoding(let m):         "Decoding error: \(m)"
        case .empty:                   "Provider returned an empty response."
        }
    }
}

/// Strip ```json … ``` (or plain ```) fences if the model decided to wrap
/// its JSON anyway. Defensive — providers told to return raw JSON usually do,
/// but local Ollama models are less reliable.
enum ResponseCleaner {
    static func unwrap(_ raw: String) -> String {
        var s = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if s.hasPrefix("```") {
            // strip first fence + optional language
            if let nl = s.firstIndex(of: "\n") {
                s = String(s[s.index(after: nl)...])
            }
            // strip closing fence
            if let endRange = s.range(of: "```", options: .backwards) {
                s = String(s[..<endRange.lowerBound])
            }
            s = s.trimmingCharacters(in: .whitespacesAndNewlines)
        }
        return s
    }
}

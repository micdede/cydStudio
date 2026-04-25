import Foundation
import Observation

/// Active AI provider configuration. Persisted across launches: non-secret
/// fields go in UserDefaults, API keys go in the macOS Keychain (see Keychain.swift).
@Observable
@MainActor
final class AISettings {
    enum Kind: String, CaseIterable, Identifiable {
        case anthropic = "anthropic"
        case openai    = "openai"
        case ollama    = "ollama"
        var id: String { rawValue }
        var displayName: String {
            switch self {
            case .anthropic: "Anthropic (Claude)"
            case .openai:    "OpenAI-compatible"
            case .ollama:    "Ollama (local)"
            }
        }
    }

    var kind: Kind {
        didSet { UserDefaults.standard.set(kind.rawValue, forKey: "ai.kind") }
    }
    var baseURL: String {
        didSet { UserDefaults.standard.set(baseURL, forKey: "ai.baseURL.\(kind.rawValue)") }
    }
    var model: String {
        didSet { UserDefaults.standard.set(model, forKey: "ai.model.\(kind.rawValue)") }
    }
    var apiKey: String {
        didSet {
            // Pasted keys frequently arrive with a trailing newline or leading
            // whitespace from the clipboard — Anthropic/OpenAI then reject them
            // with a generic "invalid x-api-key" 401. Trim before persisting.
            let trimmed = apiKey.trimmingCharacters(in: .whitespacesAndNewlines)
            if trimmed != apiKey {
                apiKey = trimmed   // re-enters didSet, this time as no-op
                return
            }
            Keychain.set(trimmed, account: kind.rawValue)
        }
    }

    init() {
        let raw = UserDefaults.standard.string(forKey: "ai.kind") ?? Kind.anthropic.rawValue
        let kind = Kind(rawValue: raw) ?? .anthropic
        self.kind = kind
        self.baseURL = UserDefaults.standard.string(forKey: "ai.baseURL.\(kind.rawValue)") ?? Self.defaultBaseURL(kind)
        self.model   = UserDefaults.standard.string(forKey: "ai.model.\(kind.rawValue)")   ?? Self.defaultModel(kind)
        self.apiKey  = Keychain.get(account: kind.rawValue)
    }

    /// Switch to a new provider, loading its persisted base URL/model/key.
    func switchTo(_ newKind: Kind) {
        guard newKind != kind else { return }
        kind = newKind
        baseURL = UserDefaults.standard.string(forKey: "ai.baseURL.\(newKind.rawValue)") ?? Self.defaultBaseURL(newKind)
        model   = UserDefaults.standard.string(forKey: "ai.model.\(newKind.rawValue)")   ?? Self.defaultModel(newKind)
        apiKey  = Keychain.get(account: newKind.rawValue)
    }

    /// Build a usable AIProvider from current settings. Throws if config is bad.
    func makeProvider() throws -> AIProvider {
        guard let url = URL(string: baseURL) else {
            throw AIError.decoding("invalid base URL: \(baseURL)")
        }
        switch kind {
        case .anthropic:
            return AnthropicProvider(baseURL: url, model: model, apiKey: apiKey)
        case .openai:
            return OpenAICompatibleProvider(baseURL: url, model: model, apiKey: apiKey)
        case .ollama:
            return OllamaProvider(baseURL: url, model: model)
        }
    }

    static func defaultBaseURL(_ k: Kind) -> String {
        switch k {
        case .anthropic: "https://api.anthropic.com"
        case .openai:    "https://api.openai.com"
        case .ollama:    "http://localhost:11434"
        }
    }
    static func defaultModel(_ k: Kind) -> String {
        switch k {
        case .anthropic: "claude-opus-4-7"
        case .openai:    "gpt-4o"
        case .ollama:    "llama3.2"
        }
    }
}

import SwiftUI

/// Compact prompt bar that lives at the top of the Designer. Takes a natural
/// language prompt, calls the active AI provider, and replaces the editor
/// content with the returned JSON layout.
struct AIGenerateBar: View {
    @Environment(AISettings.self) private var settings
    @Binding var editorText: String
    let boardWidth: Int
    let boardHeight: Int

    @State private var prompt: String = ""
    @State private var isGenerating = false
    @State private var status: String?
    @State private var statusIsError = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Image(systemName: "sparkles").foregroundStyle(.purple)
                TextField("Describe what to show — e.g. \"Bitcoin price + RSI gauge\"", text: $prompt, axis: .vertical)
                    .lineLimit(1...3)
                    .textFieldStyle(.roundedBorder)
                    .onSubmit { Task { await generate() } }
                Button {
                    Task { await generate() }
                } label: {
                    if isGenerating {
                        ProgressView().controlSize(.small)
                    } else {
                        Label("Generate", systemImage: "sparkles")
                    }
                }
                .buttonStyle(.bordered)
                .disabled(prompt.isEmpty || isGenerating)
            }
            if let s = status {
                Text(s)
                    .font(.caption)
                    .foregroundStyle(statusIsError ? .red : .secondary)
            }
        }
    }

    private func generate() async {
        let trimmed = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        isGenerating = true
        status = nil
        defer { isGenerating = false }

        do {
            let provider = try settings.makeProvider()
            let sys  = PromptBuilder.systemPrompt(boardWidth: boardWidth, boardHeight: boardHeight)
            let user = PromptBuilder.userPrompt(natural: trimmed, currentLayout: editorText.isEmpty ? nil : editorText)
            let raw  = try await provider.generateLayout(systemPrompt: sys, userPrompt: user)

            // Validate it parses as JSON before replacing editor content.
            guard let data = raw.data(using: .utf8),
                  let _ = try? JSONSerialization.jsonObject(with: data) else {
                statusIsError = true
                status = "AI returned non-JSON. Raw response left in editor for inspection."
                editorText = raw
                return
            }
            editorText = raw
            statusIsError = false
            status = "Generated \(raw.count) bytes via \(provider.displayName)."
        } catch {
            statusIsError = true
            status = (error as? LocalizedError)?.errorDescription ?? "\(error)"
        }
    }
}

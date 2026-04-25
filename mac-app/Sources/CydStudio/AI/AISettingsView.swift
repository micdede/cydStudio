import SwiftUI

struct AISettingsView: View {
    @Environment(AISettings.self) private var settings
    @Environment(\.dismiss) private var dismiss

    @State private var testStatus: TestStatus = .idle
    enum TestStatus { case idle, probing, ok(String), failed(String) }

    var body: some View {
        @Bindable var settings = settings

        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Image(systemName: "sparkles").foregroundStyle(.purple)
                Text("AI Provider").font(.title3).bold()
                Spacer()
            }
            Divider()

            Form {
                Picker("Provider", selection: Binding(
                    get: { settings.kind },
                    set: { settings.switchTo($0) }
                )) {
                    ForEach(AISettings.Kind.allCases) { k in
                        Text(k.displayName).tag(k)
                    }
                }

                TextField("Base URL", text: $settings.baseURL)
                    .textFieldStyle(.roundedBorder)

                TextField("Model", text: $settings.model)
                    .textFieldStyle(.roundedBorder)

                if settings.kind != .ollama {
                    SecureField("API key", text: $settings.apiKey)
                        .textFieldStyle(.roundedBorder)
                }
            }

            HStack {
                Button("Test connection") { Task { await runTest() } }
                    .disabled({ if case .probing = testStatus { true } else { false } }())
                statusBadge
                Spacer()
                Button("Done") { dismiss() }
                    .buttonStyle(.borderedProminent)
                    .keyboardShortcut(.defaultAction)
            }
        }
        .padding(20)
        .frame(width: 480)
    }

    @ViewBuilder
    private var statusBadge: some View {
        switch testStatus {
        case .idle:
            EmptyView()
        case .probing:
            ProgressView().controlSize(.small)
        case .ok(let msg):
            Label(msg, systemImage: "checkmark.circle.fill").foregroundStyle(.green).font(.caption)
        case .failed(let msg):
            Label(msg, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.red).font(.caption)
        }
    }

    private func runTest() async {
        testStatus = .probing
        do {
            let provider = try settings.makeProvider()
            // A throwaway short prompt; we only care that the round-trip works.
            let _ = try await provider.generateLayout(
                systemPrompt: "Reply with a JSON object containing one field 'ok' set to true.",
                userPrompt:   "Say ok."
            )
            testStatus = .ok("OK")
        } catch {
            testStatus = .failed((error as? LocalizedError)?.errorDescription ?? "\(error)")
        }
    }
}

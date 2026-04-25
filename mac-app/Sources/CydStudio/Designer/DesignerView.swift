import SwiftUI

struct DesignerView: View {
    @Bindable var device: Device

    @State private var jsonText: String = ""
    @State private var lastPushOutcome: PushOutcome?
    @State private var isPushing = false
    @State private var examples: [Example] = Example.discover()
    @State private var selectedExample: Example.ID?

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            editor
            Divider()
            statusBar
        }
        .task { await refreshHealth() }
    }

    // MARK: - Subviews

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(device.id).font(.title2).fontWeight(.semibold)
                Text("\(device.baseURL.absoluteString) · \(device.lastHealth?.board ?? "—")")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer()

            Picker("Example", selection: $selectedExample) {
                Text("Choose example…").tag(Optional<Example.ID>.none)
                ForEach(examples) { ex in
                    Text(ex.name).tag(Optional(ex.id))
                }
            }
            .frame(maxWidth: 220)
            .onChange(of: selectedExample) { _, new in
                if let id = new, let ex = examples.first(where: { $0.id == id }) {
                    jsonText = ex.contents
                }
            }

            Button {
                Task { await push() }
            } label: {
                Label("Push layout", systemImage: "arrow.up.circle.fill")
            }
            .buttonStyle(.borderedProminent)
            .disabled(isPushing || jsonText.isEmpty)
        }
        .padding(12)
    }

    private var editor: some View {
        TextEditor(text: $jsonText)
            .font(.system(.body, design: .monospaced))
            .padding(8)
    }

    private var statusBar: some View {
        HStack {
            if isPushing {
                ProgressView().controlSize(.small)
                Text("Pushing…").foregroundStyle(.secondary)
            } else if let outcome = lastPushOutcome {
                Image(systemName: outcome.success ? "checkmark.circle.fill" : "exclamationmark.triangle.fill")
                    .foregroundStyle(outcome.success ? .green : .red)
                Text(outcome.message)
                    .foregroundStyle(.secondary)
            } else {
                Text("Idle").foregroundStyle(.secondary)
            }
            Spacer()
        }
        .font(.caption)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
    }

    // MARK: - Actions

    private func push() async {
        guard let data = jsonText.data(using: .utf8) else { return }
        // Validate JSON syntax client-side before round-tripping to the device.
        do {
            _ = try JSONSerialization.jsonObject(with: data)
        } catch {
            lastPushOutcome = PushOutcome(success: false, message: "Invalid JSON: \(error.localizedDescription)")
            return
        }
        isPushing = true; defer { isPushing = false }
        do {
            try await DeviceClient(device: device).pushLayout(data)
            lastPushOutcome = PushOutcome(success: true, message: "Pushed \(data.count) bytes at \(Date().formatted(date: .omitted, time: .standard))")
            device.lastError = nil
        } catch {
            let msg = (error as? LocalizedError)?.errorDescription ?? "\(error)"
            lastPushOutcome = PushOutcome(success: false, message: msg)
            device.lastError = msg
        }
    }

    private func refreshHealth() async {
        do {
            let h = try await DeviceClient(device: device).health()
            device.lastHealth = h
            device.lastError = nil
        } catch {
            device.lastError = (error as? LocalizedError)?.errorDescription ?? "\(error)"
        }
    }
}

struct PushOutcome {
    let success: Bool
    let message: String
}

struct Example: Identifiable {
    let id: String
    let name: String
    let contents: String

    static func discover() -> [Example] {
        guard let urls = Bundle.module.urls(forResourcesWithExtension: "json", subdirectory: "examples") else {
            return []
        }
        return urls.compactMap { url -> Example? in
            guard let data = try? Data(contentsOf: url),
                  let text = String(data: data, encoding: .utf8) else { return nil }
            return Example(id: url.lastPathComponent, name: url.deletingPathExtension().lastPathComponent, contents: text)
        }
        .sorted { $0.name < $1.name }
    }
}

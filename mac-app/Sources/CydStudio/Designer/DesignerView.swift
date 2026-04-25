import SwiftUI
import AppKit
import UniformTypeIdentifiers

struct DesignerView: View {
    @Environment(DeviceRegistry.self) private var registry
    @Bindable var device: Device

    @State private var jsonText: String = ""
    @State private var lastPushOutcome: PushOutcome?
    @State private var isPushing = false
    @State private var isPushingAll = false
    @State private var examples: [Example] = Example.discover()
    @State private var selectedExample: Example.ID?

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            AIGenerateBar(editorText: $jsonText,
                          boardWidth: 320, boardHeight: 240)
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
            Divider()
            editor
            Divider()
            statusBar
        }
        .task { await refreshHealth() }
    }

    // MARK: - Subviews

    private var header: some View {
        HStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text(device.id).font(.title2).fontWeight(.semibold)
                Text("\(device.baseURL.absoluteString) · \(device.lastHealth?.board ?? "—")")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer()

            // File operations
            Button { openJSON() } label: {
                Label("Open…", systemImage: "folder")
            }
            .help("Open a JSON layout file")

            Button { saveJSON() } label: {
                Label("Save…", systemImage: "square.and.arrow.down")
            }
            .disabled(jsonText.isEmpty)
            .help("Save current layout to a JSON file")

            Divider().frame(height: 20)

            Picker("Example", selection: $selectedExample) {
                Text("Choose example…").tag(Optional<Example.ID>.none)
                ForEach(examples) { ex in
                    Text(ex.name).tag(Optional(ex.id))
                }
            }
            .frame(maxWidth: 200)
            .onChange(of: selectedExample) { _, new in
                if let id = new, let ex = examples.first(where: { $0.id == id }) {
                    jsonText = ex.contents
                }
            }

            Divider().frame(height: 20)

            Button {
                Task { await push() }
            } label: {
                Label("Push", systemImage: "arrow.up.circle.fill")
            }
            .buttonStyle(.borderedProminent)
            .disabled(isPushing || jsonText.isEmpty)
            .help("Push layout to \(device.id)")

            if registry.devices.count > 1 {
                Button {
                    Task { await pushToAll() }
                } label: {
                    Label("Push to all", systemImage: "arrow.up.circle")
                }
                .buttonStyle(.bordered)
                .disabled(isPushingAll || jsonText.isEmpty)
                .help("Push layout to all \(registry.devices.count) discovered devices")
            }
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
            if isPushing || isPushingAll {
                ProgressView().controlSize(.small)
                Text(isPushingAll ? "Pushing to all devices…" : "Pushing…")
                    .foregroundStyle(.secondary)
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

    // MARK: - File I/O

    private func openJSON() {
        let panel = NSOpenPanel()
        panel.title = "Open Layout"
        panel.allowedContentTypes = [UTType.json]
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK,
              let url = panel.url,
              let text = try? String(contentsOf: url, encoding: .utf8) else { return }
        jsonText = text
        selectedExample = nil
    }

    private func saveJSON() {
        let panel = NSSavePanel()
        panel.title = "Save Layout"
        panel.allowedContentTypes = [UTType.json]
        panel.nameFieldStringValue = "\(device.id)-layout.json"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        try? jsonText.data(using: .utf8)?.write(to: url)
    }

    // MARK: - Push actions

    private func push() async {
        guard let data = validatedData() else { return }
        isPushing = true; defer { isPushing = false }
        do {
            try await DeviceClient(device: device).pushLayout(data)
            lastPushOutcome = PushOutcome(success: true, message: "Pushed \(data.count) bytes to \(device.id) at \(Date().formatted(date: .omitted, time: .standard))")
            device.lastError = nil
        } catch {
            let msg = (error as? LocalizedError)?.errorDescription ?? "\(error)"
            lastPushOutcome = PushOutcome(success: false, message: msg)
            device.lastError = msg
        }
    }

    private func pushToAll() async {
        guard let data = validatedData() else { return }
        isPushingAll = true; defer { isPushingAll = false }
        let targets = registry.devices
        var succeeded = 0
        await withTaskGroup(of: Bool.self) { group in
            for dev in targets {
                group.addTask {
                    do {
                        try await DeviceClient(device: dev).pushLayout(data)
                        return true
                    } catch {
                        return false
                    }
                }
            }
            for await ok in group { if ok { succeeded += 1 } }
        }
        let ok = succeeded == targets.count
        lastPushOutcome = PushOutcome(
            success: ok,
            message: "Pushed to \(succeeded)/\(targets.count) devices at \(Date().formatted(date: .omitted, time: .standard))"
        )
    }

    private func validatedData() -> Data? {
        guard let data = jsonText.data(using: .utf8) else { return nil }
        do {
            _ = try JSONSerialization.jsonObject(with: data)
            return data
        } catch {
            lastPushOutcome = PushOutcome(success: false, message: "Invalid JSON: \(error.localizedDescription)")
            return nil
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

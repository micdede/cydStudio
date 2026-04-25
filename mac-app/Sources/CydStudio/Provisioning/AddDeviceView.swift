import SwiftUI

/// Multi-step sheet that walks the user through:
///   1) pick USB-Serial port + board
///   2) flash bundled firmware image via esptool
///   3) enter WiFi credentials + hostname + OTA password
///   4) push credentials over serial; wait for the device to appear on mDNS
struct AddDeviceView: View {
    @Environment(DeviceRegistry.self) private var registry
    @Environment(\.dismiss) private var dismiss

    enum Step { case pickPort, flashing, credentials, provisioning, waitingForDevice, done }

    @State private var step: Step = .pickPort
    @State private var ports: [SerialPort] = []
    @State private var selectedPortPath: String?
    @State private var selectedBoardID: String = BoardCatalog.all.first?.id ?? ""

    @State private var ssid: String = ""
    @State private var psk: String = ""
    @State private var hostname: String = ""
    @State private var otaPassword: String = ""

    @State private var flashProgress: Double = 0
    @State private var lastLog: String = ""
    @State private var errorMessage: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            header
            Divider()
            content
            Spacer(minLength: 0)
            footer
        }
        .padding(20)
        .frame(width: 540, height: 380)
        .onAppear { ports = SerialPortDetector.scan() }
    }

    // MARK: - Sections

    private var header: some View {
        HStack {
            Image(systemName: "bolt.fill").foregroundStyle(.orange)
            VStack(alignment: .leading) {
                Text("Add a cydStudio device").font(.title3).bold()
                Text(stepLabel).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
        }
    }

    @ViewBuilder
    private var content: some View {
        switch step {
        case .pickPort:        portPicker
        case .flashing:        progressView(title: "Flashing firmware…", progress: flashProgress)
        case .credentials:     credentialsForm
        case .provisioning:    progressView(title: "Sending credentials over serial…", progress: nil)
        case .waitingForDevice:progressView(title: "Waiting for device on mDNS…", progress: nil)
        case .done:            doneView
        }
    }

    private var portPicker: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Plug your board in via USB, then pick its serial port.")
                .foregroundStyle(.secondary).font(.callout)
            HStack {
                Picker("Port", selection: $selectedPortPath) {
                    Text("Select…").tag(Optional<String>.none)
                    ForEach(ports) { port in
                        Text(port.displayName).tag(Optional(port.path))
                    }
                }
                Button {
                    ports = SerialPortDetector.scan()
                } label: { Image(systemName: "arrow.clockwise") }
            }
            Picker("Board", selection: $selectedBoardID) {
                ForEach(BoardCatalog.all) { board in
                    Text(board.displayName).tag(board.id)
                }
            }
            if errorMessage != nil { errorBanner }
        }
    }

    private var credentialsForm: some View {
        VStack(alignment: .leading, spacing: 8) {
            TextField("WiFi SSID", text: $ssid)
            SecureField("WiFi password", text: $psk)
            TextField("Device hostname (e.g. claude-monitor)", text: $hostname)
            SecureField("OTA password (optional)", text: $otaPassword)
            Text("Hostname becomes \(hostname.isEmpty ? "<hostname>" : hostname).local on your network.")
                .font(.caption).foregroundStyle(.secondary)
            if errorMessage != nil { errorBanner }
        }
        .textFieldStyle(.roundedBorder)
    }

    private var doneView: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Device is online", systemImage: "checkmark.seal.fill")
                .font(.title3).foregroundStyle(.green)
            Text("\(hostname).local appeared on mDNS. You can close this window and push a layout from the sidebar.")
                .foregroundStyle(.secondary)
        }
    }

    private var errorBanner: some View {
        Label(errorMessage ?? "", systemImage: "exclamationmark.triangle.fill")
            .foregroundStyle(.red).font(.caption)
            .padding(8)
            .background(.red.opacity(0.1), in: RoundedRectangle(cornerRadius: 6))
    }

    private func progressView(title: String, progress: Double?) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.headline)
            if let p = progress {
                ProgressView(value: p) { EmptyView() }
                Text("\(Int(p * 100))%").font(.caption).foregroundStyle(.secondary)
            } else {
                ProgressView().controlSize(.small)
            }
            if !lastLog.isEmpty {
                Text(lastLog).font(.system(.caption, design: .monospaced))
                    .foregroundStyle(.secondary).lineLimit(2, reservesSpace: true)
            }
            if errorMessage != nil { errorBanner }
        }
    }

    private var footer: some View {
        HStack {
            Button("Cancel") { dismiss() }
            Spacer()
            primaryButton
        }
    }

    @ViewBuilder
    private var primaryButton: some View {
        switch step {
        case .pickPort:
            Button("Flash") { Task { await beginFlash() } }
                .buttonStyle(.borderedProminent)
                .disabled(selectedPortPath == nil)
        case .credentials:
            Button("Send") { Task { await sendCredentials() } }
                .buttonStyle(.borderedProminent)
                .disabled(ssid.isEmpty || hostname.isEmpty)
        case .done:
            Button("Done") { dismiss() }.buttonStyle(.borderedProminent)
        default:
            EmptyView()
        }
    }

    private var stepLabel: String {
        switch step {
        case .pickPort:         "Step 1 of 3 — Pick port and board"
        case .flashing:         "Step 1 of 3 — Flashing"
        case .credentials:      "Step 2 of 3 — WiFi & hostname"
        case .provisioning:     "Step 2 of 3 — Sending"
        case .waitingForDevice: "Step 3 of 3 — Joining network"
        case .done:             "All done"
        }
    }

    // MARK: - Actions

    private func beginFlash() async {
        errorMessage = nil
        guard let portPath = selectedPortPath,
              let board = BoardCatalog.profile(id: selectedBoardID) else { return }
        guard let image = board.imageURL else {
            errorMessage = "No bundled image for \(board.id) — run scripts/build_firmware_image.sh"
            return
        }
        step = .flashing
        flashProgress = 0
        do {
            for try await event in EsptoolRunner.flash(image: image, port: portPath, chip: board.chip, baud: board.flashBaud) {
                switch event {
                case .log(let l):       lastLog = l
                case .progress(let p):  flashProgress = p
                case .done:             break
                }
            }
            // Pre-fill hostname suggestion based on board id if user left it empty.
            if hostname.isEmpty { hostname = board.id }
            step = .credentials
        } catch {
            errorMessage = (error as? LocalizedError)?.errorDescription ?? "\(error)"
            step = .pickPort
        }
    }

    private func sendCredentials() async {
        errorMessage = nil
        guard let portPath = selectedPortPath else { return }
        step = .provisioning
        let payload = SerialProvisioner.Payload(ssid: ssid, psk: psk, hostname: hostname, ota_pw: otaPassword)
        do {
            try await SerialProvisioner.provision(port: portPath, payload: payload)
        } catch {
            errorMessage = (error as? LocalizedError)?.errorDescription ?? "\(error)"
            step = .credentials
            return
        }
        step = .waitingForDevice
        await waitForDeviceOnMDNS(name: hostname)
        step = .done
    }

    private func waitForDeviceOnMDNS(name: String, timeout: TimeInterval = 30) async {
        let deadline = Date().addingTimeInterval(timeout)
        while Date() < deadline {
            if registry.devices.contains(where: { $0.id == name }) { return }
            try? await Task.sleep(nanoseconds: 500_000_000)
        }
        errorMessage = "Device didn't appear on mDNS within \(Int(timeout))s — it may still be joining WiFi."
    }
}

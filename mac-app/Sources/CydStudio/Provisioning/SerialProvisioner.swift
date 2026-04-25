import Darwin
import Foundation

/// One-shot helper that pushes WiFi credentials to a freshly-flashed device
/// over the same USB-Serial cable used to flash it. Avoids the SoftAP dance.
///
/// Wire protocol (must match firmware/src/serial_provision.cpp):
///   firmware emits: "CYDSTUDIO_PROVISION_READY\n"
///   we send:        "PROVISION {json}\n"
///   firmware emits: "PROVISION_OK\n" then reboots
struct SerialProvisioner {
    enum Error: LocalizedError {
        case openFailed(errno: Int32)
        case configureFailed(errno: Int32)
        case timeout(message: String)
        case deviceError(String)

        var errorDescription: String? {
            switch self {
            case .openFailed(let e):     "Open serial failed (errno \(e))"
            case .configureFailed(let e):"Configure serial failed (errno \(e))"
            case .timeout(let m):        "Timeout: \(m)"
            case .deviceError(let m):    "Device: \(m)"
            }
        }
    }

    struct Payload: Encodable {
        let ssid: String
        let psk: String
        let hostname: String
        let ota_pw: String
    }

    /// Open `path` at 115200, wait up to `readyTimeout` seconds for the
    /// PROVISION_READY marker, send the payload, wait for PROVISION_OK.
    static func provision(port path: String,
                          payload: Payload,
                          readyTimeout: TimeInterval = 8,
                          ackTimeout: TimeInterval = 5) async throws {
        let fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)
        guard fd >= 0 else { throw Error.openFailed(errno: errno) }
        defer { close(fd) }

        try configure(fd: fd, baud: 115200)

        let json = try JSONEncoder().encode(payload)
        let line = "PROVISION " + (String(data: json, encoding: .utf8) ?? "{}") + "\n"

        // Wait for the firmware's ready marker.
        let readyDeadline = Date().addingTimeInterval(readyTimeout)
        try await waitFor(needle: "CYDSTUDIO_PROVISION_READY", on: fd, until: readyDeadline)

        // Send the credentials line.
        guard let lineData = line.data(using: .utf8) else {
            throw Error.deviceError("encoding payload")
        }
        _ = lineData.withUnsafeBytes { write(fd, $0.baseAddress, lineData.count) }

        // Wait for PROVISION_OK or PROVISION_ERR.
        let ackDeadline = Date().addingTimeInterval(ackTimeout)
        let ack = try await waitForLine(matching: { $0.hasPrefix("PROVISION_OK") || $0.hasPrefix("PROVISION_ERR") },
                                        on: fd, until: ackDeadline)
        if ack.hasPrefix("PROVISION_ERR") {
            throw Error.deviceError(String(ack.dropFirst("PROVISION_ERR ".count)))
        }
    }

    // MARK: - termios + read helpers

    private static func configure(fd: Int32, baud: Int) throws {
        var t = termios()
        if tcgetattr(fd, &t) != 0 { throw Error.configureFailed(errno: errno) }
        cfmakeraw(&t)
        cfsetispeed(&t, speed_t(baud))
        cfsetospeed(&t, speed_t(baud))
        t.c_cflag |= UInt(CLOCAL | CREAD | CS8)
        if tcsetattr(fd, TCSANOW, &t) != 0 { throw Error.configureFailed(errno: errno) }
    }

    private static func waitFor(needle: String, on fd: Int32, until deadline: Date) async throws {
        var buffer = ""
        while Date() < deadline {
            try await readChunk(into: &buffer, fd: fd)
            if buffer.contains(needle) { return }
        }
        throw Error.timeout(message: "did not see '\(needle)' on serial")
    }

    private static func waitForLine(matching predicate: (String) -> Bool,
                                    on fd: Int32,
                                    until deadline: Date) async throws -> String {
        var buffer = ""
        while Date() < deadline {
            try await readChunk(into: &buffer, fd: fd)
            while let nl = buffer.firstIndex(of: "\n") {
                let line = String(buffer[..<nl]).trimmingCharacters(in: .whitespacesAndNewlines)
                buffer = String(buffer[buffer.index(after: nl)...])
                if predicate(line) { return line }
            }
        }
        throw Error.timeout(message: "no matching reply on serial")
    }

    private static func readChunk(into buffer: inout String, fd: Int32) async throws {
        var bytes = [UInt8](repeating: 0, count: 256)
        let n = bytes.withUnsafeMutableBufferPointer { ptr in
            read(fd, ptr.baseAddress, 256)
        }
        if n > 0, let chunk = String(bytes: bytes.prefix(n), encoding: .utf8) {
            buffer.append(chunk)
        } else {
            try await Task.sleep(nanoseconds: 50_000_000)  // 50ms
        }
    }
}

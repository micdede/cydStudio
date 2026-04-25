import Foundation

/// A cydStudio device discovered on the local network via Bonjour
/// (`_cydstudio._tcp`). Identity is the Bonjour service name; the resolved
/// host/port are filled in once the browser hands us an endpoint.
@Observable
final class Device: Identifiable, Hashable {
    let id: String        // Bonjour service name, e.g. "claude-monitor"
    var hostname: String  // mDNS hostname, e.g. "claude-monitor.local"
    var port: Int
    var lastHealth: HealthSnapshot?
    var lastError: String?

    init(id: String, hostname: String, port: Int = 80) {
        self.id = id
        self.hostname = hostname
        self.port = port
    }

    /// Base URL we send HTTP requests to.
    var baseURL: URL {
        URL(string: "http://\(hostname):\(port)")!
    }

    static func == (lhs: Device, rhs: Device) -> Bool { lhs.id == rhs.id }
    func hash(into hasher: inout Hasher) { hasher.combine(id) }
}

struct HealthSnapshot: Codable, Equatable {
    let ok: Bool
    let board: String
}

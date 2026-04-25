import Foundation
import Network
import Observation

/// Discovers cydStudio devices on the local network via Bonjour.
///
/// The firmware advertises `_cydstudio._tcp` (see ESPmDNS.addService in
/// firmware/src/main.cpp). We browse continuously and surface results as an
/// observable list of `Device` objects.
@Observable
@MainActor
final class DeviceRegistry {
    private(set) var devices: [Device] = []
    private var browser: NWBrowser?

    init() {
        startBrowsing()
    }

    func device(id: Device.ID) -> Device? {
        devices.first { $0.id == id }
    }

    func startBrowsing() {
        guard browser == nil else { return }
        let descriptor = NWBrowser.Descriptor.bonjour(type: "_cydstudio._tcp", domain: "local.")
        let params = NWParameters()
        params.includePeerToPeer = false
        let browser = NWBrowser(for: descriptor, using: params)
        self.browser = browser

        browser.stateUpdateHandler = { state in
            // State is logged for diagnostics — actual device discovery flows through
            // browseResultsChangedHandler below.
            #if DEBUG
            print("[mDNS] state = \(state)")
            #endif
        }

        browser.browseResultsChangedHandler = { [weak self] results, _ in
            Task { @MainActor in
                self?.applyBrowseResults(results)
            }
        }

        browser.start(queue: .main)
    }

    func stop() {
        browser?.cancel()
        browser = nil
    }

    private func applyBrowseResults(_ results: Set<NWBrowser.Result>) {
        var seen = Set<String>()
        var next: [Device] = []

        for result in results {
            guard case let .service(name, _, _, _) = result.endpoint else { continue }
            seen.insert(name)
            // Reuse existing Device so its observed state survives re-renders.
            if let existing = devices.first(where: { $0.id == name }) {
                next.append(existing)
            } else {
                next.append(Device(id: name, hostname: "\(name).local"))
            }
        }

        // Keep stable order by name for a non-jittery sidebar.
        next.sort { $0.id < $1.id }
        devices = next.filter { seen.contains($0.id) } + next.filter { !seen.contains($0.id) }
    }
}

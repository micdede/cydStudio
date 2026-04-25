import SwiftUI

struct DeviceListView: View {
    @Environment(DeviceRegistry.self) private var registry
    @Binding var selection: Device.ID?

    var body: some View {
        List(selection: $selection) {
            Section("Devices") {
                if registry.devices.isEmpty {
                    Label("Searching mDNS…", systemImage: "antenna.radiowaves.left.and.right")
                        .foregroundStyle(.secondary)
                        .padding(.vertical, 4)
                } else {
                    ForEach(registry.devices) { device in
                        DeviceRow(device: device)
                            .tag(device.id)
                    }
                }
            }
        }
        .listStyle(.sidebar)
    }
}

private struct DeviceRow: View {
    let device: Device

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(device.id)
                .font(.headline)
            HStack(spacing: 6) {
                Circle()
                    .fill(device.lastError == nil ? Color.green : Color.red)
                    .frame(width: 7, height: 7)
                Text(device.hostname)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 2)
    }
}

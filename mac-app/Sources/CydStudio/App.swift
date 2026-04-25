import SwiftUI

@main
struct CydStudioApp: App {
    @State private var registry = DeviceRegistry()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(registry)
                .frame(minWidth: 900, minHeight: 600)
        }
        .windowResizability(.contentSize)
        .commands {
            CommandGroup(replacing: .newItem) { }
        }
    }
}

struct ContentView: View {
    @Environment(DeviceRegistry.self) private var registry
    @State private var selection: Device.ID?

    var body: some View {
        NavigationSplitView {
            DeviceListView(selection: $selection)
                .navigationSplitViewColumnWidth(min: 220, ideal: 260)
        } detail: {
            if let id = selection, let device = registry.device(id: id) {
                DesignerView(device: device)
            } else {
                ContentUnavailableView(
                    "No device selected",
                    systemImage: "display",
                    description: Text("Pick a discovered cydStudio device from the sidebar to push a layout.")
                )
            }
        }
    }
}

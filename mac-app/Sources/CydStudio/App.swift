import AppKit
import SwiftUI

/// SPM-built SwiftUI apps default to accessory activation policy, which
/// means no Dock icon, no menu-bar focus, and — critically — TextField/
/// SecureField never receive keyboard input. Force regular foreground-app
/// behavior on launch.
final class CydStudioAppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }
}

@main
struct CydStudioApp: App {
    @NSApplicationDelegateAdaptor(CydStudioAppDelegate.self) private var appDelegate
    @State private var registry = DeviceRegistry()
    @State private var aiSettings = AISettings()
    @State private var showingAISettings = false

    var body: some Scene {
        WindowGroup {
            ContentView(showingAISettings: $showingAISettings)
                .environment(registry)
                .environment(aiSettings)
                .frame(minWidth: 900, minHeight: 600)
                .sheet(isPresented: $showingAISettings) {
                    AISettingsView().environment(aiSettings)
                }
        }
        .windowResizability(.contentSize)
        .commands {
            CommandGroup(replacing: .newItem) { }
            CommandGroup(after: .appSettings) {
                Button("AI Provider…") { showingAISettings = true }
                    .keyboardShortcut(",", modifiers: [.command, .shift])
            }
        }
    }
}

struct ContentView: View {
    @Environment(DeviceRegistry.self) private var registry
    @Binding var showingAISettings: Bool
    @State private var selection: Device.ID?
    @State private var showingAddDevice = false

    var body: some View {
        NavigationSplitView {
            DeviceListView(selection: $selection)
                .navigationSplitViewColumnWidth(min: 220, ideal: 260)
                .toolbar {
                    ToolbarItem {
                        Button { showingAddDevice = true } label: {
                            Label("Add Device", systemImage: "plus")
                        }
                    }
                    ToolbarItem {
                        Button { showingAISettings = true } label: {
                            Label("AI Provider", systemImage: "sparkles")
                        }
                    }
                }
        } detail: {
            if let id = selection, let device = registry.device(id: id) {
                DesignerView(device: device)
            } else {
                ContentUnavailableView(
                    "No device selected",
                    systemImage: "display",
                    description: Text("Pick a discovered cydStudio device from the sidebar to push a layout, or use + to flash a new one.")
                )
            }
        }
        .sheet(isPresented: $showingAddDevice) {
            AddDeviceView()
        }
    }
}

import Foundation

/// Static metadata about every board cydStudio knows how to flash. Each
/// entry corresponds to a `[env:...]` in firmware/platformio.ini and a
/// `<env>.bin` file under Resources/firmware/, produced by
/// scripts/build_firmware_image.sh.
struct BoardProfile: Identifiable, Hashable {
    let id: String                  // matches platformio.ini env name
    let displayName: String
    let chip: ChipFamily
    let imageResourceName: String   // basename in Resources/firmware/
    let usbHints: [String]          // substrings in /dev/cu.* names that suggest this board

    enum ChipFamily: String {
        case esp32   = "esp32"
        case esp32s3 = "esp32-s3"
    }

    /// Path to the bundled .bin, or nil if not yet shipped.
    var imageURL: URL? {
        Bundle.module.url(forResource: imageResourceName, withExtension: "bin", subdirectory: "firmware")
    }

    /// Suggested baud rate. CH340 (CYD) is unreliable above 460800; ESP32-S3
    /// native USB can do 921600 cleanly. Conservative defaults below.
    var flashBaud: Int {
        switch chip {
        case .esp32:   460_800
        case .esp32s3: 921_600
        }
    }
}

enum BoardCatalog {
    static let all: [BoardProfile] = [
        BoardProfile(
            id: "cyd-2432s028",
            displayName: "ESP32-2432S028 (CYD)",
            chip: .esp32,
            imageResourceName: "cyd-2432s028",
            usbHints: ["usbserial", "wchusbserial", "SLAB_USBtoUART"]
        ),
        // Future boards: add when the corresponding firmware/src/boards/<name>.h lands.
    ]

    static func profile(id: String) -> BoardProfile? {
        all.first { $0.id == id }
    }

    /// Best-effort suggestion of a board profile given the device's serial port name.
    static func suggested(for portName: String) -> BoardProfile? {
        all.first { $0.usbHints.contains(where: { portName.contains($0) }) }
    }
}

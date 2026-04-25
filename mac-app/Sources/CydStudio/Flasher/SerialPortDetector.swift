import Foundation

struct SerialPort: Identifiable, Hashable {
    var id: String { path }
    let path: String              // full /dev/cu.* path
    let displayName: String       // human-friendly basename
    let isProbablyESP: Bool       // heuristic from name
}

enum SerialPortDetector {
    /// Scan /dev for USB-Serial devices that look like an ESP-class chip.
    /// We deliberately list /dev/cu.* (call-out) not /dev/tty.* (call-in)
    /// because cu doesn't block on DCD and is the right one for esptool.
    static func scan() -> [SerialPort] {
        let fm = FileManager.default
        guard let entries = try? fm.contentsOfDirectory(atPath: "/dev") else { return [] }
        return entries
            .filter { $0.hasPrefix("cu.") }
            .filter { name in
                name.contains("usbserial")
                    || name.contains("wchusbserial")
                    || name.contains("SLAB_USBtoUART")
                    || name.contains("usbmodem")          // ESP32-S3 / native USB CDC
            }
            .map { name in
                let path = "/dev/" + name
                let display = name.replacingOccurrences(of: "cu.", with: "")
                let espHint = name.contains("usbserial")
                    || name.contains("wchusbserial")
                    || name.contains("SLAB_USBtoUART")
                    || name.contains("usbmodem")
                return SerialPort(path: path, displayName: display, isProbablyESP: espHint)
            }
            .sorted { $0.displayName < $1.displayName }
    }
}

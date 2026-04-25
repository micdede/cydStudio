import Foundation

/// Wraps the esptool.py command-line tool. We try a few well-known locations
/// before falling back to PATH so the user doesn't have to set anything up
/// in the common case where they already use PlatformIO.
struct EsptoolRunner {
    enum Error: LocalizedError {
        case notFound
        case exit(code: Int32, stderr: String)

        var errorDescription: String? {
            switch self {
            case .notFound:
                "esptool.py not found. Install with `pip3 install esptool` or install PlatformIO."
            case .exit(let code, let err):
                "esptool exited \(code): \(err)"
            }
        }
    }

    /// Stream emitted while flashing — values are user-facing log lines
    /// (mostly esptool stdout) plus periodic `progress` updates parsed from
    /// the percentage in esptool's "Writing at 0x... (47 %)" output.
    enum Event {
        case log(String)
        case progress(Double)   // 0…1
        case done
    }

    private static func resolve() -> (python: String, script: String)? {
        let home = NSHomeDirectory()
        let candidates = [
            ("/Library/Frameworks/Python.framework/Versions/3.13/bin/python3",
             "\(home)/.platformio/packages/tool-esptoolpy/esptool.py"),
            ("/usr/bin/python3",
             "\(home)/.platformio/packages/tool-esptoolpy/esptool.py"),
            ("/usr/bin/env",
             "esptool.py"),   // last resort: PATH
        ]
        for (py, script) in candidates {
            if FileManager.default.isExecutableFile(atPath: py),
               script == "esptool.py" || FileManager.default.fileExists(atPath: script) {
                return (py, script)
            }
        }
        return nil
    }

    static func flash(image: URL, port: String, chip: BoardProfile.ChipFamily, baud: Int) -> AsyncThrowingStream<Event, Swift.Error> {
        AsyncThrowingStream { continuation in
            guard let tools = resolve() else {
                continuation.finish(throwing: Error.notFound)
                return
            }
            let process = Process()
            let pipe = Pipe()
            process.executableURL = URL(fileURLWithPath: tools.python)
            process.arguments = [
                tools.script,
                "--chip", chip.rawValue,
                "--port", port,
                "--baud", "\(baud)",
                "--before", "default_reset",
                "--after",  "hard_reset",
                "write_flash", "0x0", image.path,
            ]
            process.standardOutput = pipe
            process.standardError  = pipe

            // Stream lines as they arrive.
            pipe.fileHandleForReading.readabilityHandler = { handle in
                let chunk = handle.availableData
                guard !chunk.isEmpty else { return }
                if let text = String(data: chunk, encoding: .utf8) {
                    for line in text.split(separator: "\n", omittingEmptySubsequences: false) {
                        let s = String(line)
                        continuation.yield(.log(s))
                        if let pct = parsePercent(s) {
                            continuation.yield(.progress(pct))
                        }
                    }
                }
            }

            process.terminationHandler = { proc in
                pipe.fileHandleForReading.readabilityHandler = nil
                if proc.terminationStatus == 0 {
                    continuation.yield(.progress(1.0))
                    continuation.yield(.done)
                    continuation.finish()
                } else {
                    continuation.finish(throwing: Error.exit(code: proc.terminationStatus, stderr: ""))
                }
            }

            do {
                try process.run()
            } catch {
                continuation.finish(throwing: error)
            }

            continuation.onTermination = { _ in
                if process.isRunning { process.terminate() }
            }
        }
    }

    /// Parse "Writing at 0x... (47 %)" → 0.47.
    private static func parsePercent(_ line: String) -> Double? {
        guard let openIdx = line.firstIndex(of: "("),
              let closeIdx = line.firstIndex(of: ")"),
              openIdx < closeIdx else { return nil }
        let inside = line[line.index(after: openIdx)..<closeIdx]
        let trimmed = inside.trimmingCharacters(in: .whitespaces)
        guard trimmed.hasSuffix("%"),
              let pct = Double(trimmed.dropLast().trimmingCharacters(in: .whitespaces)) else { return nil }
        return pct / 100.0
    }
}

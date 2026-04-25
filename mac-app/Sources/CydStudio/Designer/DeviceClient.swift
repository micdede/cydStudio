import Foundation

/// Thin async HTTP wrapper around a single cydStudio device.
/// All endpoints return 200 + `{"ok":true}` on success or a 4xx with a
/// plain-text body on failure.
struct DeviceClient {
    let device: Device

    enum ClientError: LocalizedError {
        case http(status: Int, body: String)
        case transport(Error)
        case decoding(Error)

        var errorDescription: String? {
            switch self {
            case .http(let status, let body): "HTTP \(status): \(body)"
            case .transport(let e):           "Network: \(e.localizedDescription)"
            case .decoding(let e):            "Decode: \(e.localizedDescription)"
            }
        }
    }

    func health() async throws -> HealthSnapshot {
        let (data, response) = try await get("/health")
        try ensureOK(data: data, response: response)
        do {
            return try JSONDecoder().decode(HealthSnapshot.self, from: data)
        } catch {
            throw ClientError.decoding(error)
        }
    }

    func pushLayout(_ json: Data) async throws {
        let (data, response) = try await post("/layout", body: json)
        try ensureOK(data: data, response: response)
    }

    func pushData(sourceID: String, payload: Data) async throws {
        let (data, response) = try await post("/data/\(sourceID)", body: payload)
        try ensureOK(data: data, response: response)
    }

    // MARK: - Internals

    private func get(_ path: String) async throws -> (Data, URLResponse) {
        let url = device.baseURL.appendingPathComponent(path)
        var req = URLRequest(url: url)
        req.timeoutInterval = 5
        return try await transport(req)
    }

    private func post(_ path: String, body: Data) async throws -> (Data, URLResponse) {
        let url = device.baseURL.appendingPathComponent(path)
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = body
        req.timeoutInterval = 10
        return try await transport(req)
    }

    private func transport(_ req: URLRequest) async throws -> (Data, URLResponse) {
        do {
            return try await URLSession.shared.data(for: req)
        } catch {
            throw ClientError.transport(error)
        }
    }

    private func ensureOK(data: Data, response: URLResponse) throws {
        guard let http = response as? HTTPURLResponse else { return }
        guard (200..<300).contains(http.statusCode) else {
            let body = String(data: data, encoding: .utf8) ?? ""
            throw ClientError.http(status: http.statusCode, body: body)
        }
    }
}

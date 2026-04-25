import Foundation

/// Builds the system prompt sent to the LLM. The prompt embeds:
///   - The full layout schema (the contract)
///   - The full datasource schema
///   - One real-world example (claude-monitor.json) to anchor the style
///   - The target board's display dimensions
///   - Hard rules: respond with raw JSON only, schema_version 0.1, no markdown
enum PromptBuilder {
    static func systemPrompt(boardWidth: Int, boardHeight: Int) -> String {
        let layoutSchema     = bundleString("layout.schema",     ext: "json", subdir: "schema") ?? "(schema missing)"
        let datasourceSchema = bundleString("datasource.schema", ext: "json", subdir: "schema") ?? "(schema missing)"
        let example          = bundleString("claude-monitor",    ext: "json", subdir: "examples") ?? "(example missing)"

        return """
        You are a UI generator for cydStudio, a system that renders LVGL widgets on small ESP32 displays. \
        You produce a single JSON layout document that the firmware can render directly.

        # Hard rules
        - Output ONE JSON object — nothing else. No markdown fences. No prose before or after.
        - schema_version MUST be "0.1".
        - Use only the widget types defined in the schema (label, bar, arc, image, button, line, sparkline).
        - Coordinates are in pixels, integers, with origin at the top-left.
        - The current target display is \(boardWidth)x\(boardHeight) pixels (after page.rotation is applied).
          Make sure all widgets fit inside that box.
        - For "label" widgets: when align is "right", x is the right-edge pixel; when align is "center", x is the center.
        - Datasources you reference in widget bindings MUST be declared in the layout's datasources array.
        - Keep it visually clean: leave ~12 px from the edges by default; pick muted backgrounds and an accent color.

        # Layout schema (JSON Schema)
        \(layoutSchema)

        # Datasource schema
        \(datasourceSchema)

        # Reference example
        \(example)
        """
    }

    /// Build the user-side message: their natural language prompt plus the
    /// editor's current JSON if they're iterating on an existing layout.
    static func userPrompt(natural: String, currentLayout: String?) -> String {
        if let cur = currentLayout, !cur.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return """
            Modify the existing layout below to match this request: "\(natural)"
            Return the COMPLETE new layout JSON.

            CURRENT LAYOUT:
            \(cur)
            """
        }
        return "Create a layout for: \(natural)"
    }

    private static func bundleString(_ name: String, ext: String, subdir: String) -> String? {
        guard let url = Bundle.module.url(forResource: name, withExtension: ext, subdirectory: subdir),
              let data = try? Data(contentsOf: url) else { return nil }
        return String(data: data, encoding: .utf8)
    }
}

.pragma library

function toInt(value, fallback) {
    const n = Math.round(Number(value))
    return Number.isFinite(n) ? n : fallback
}

function rgbArray(value) {
    const c = Qt.color(String(value))
    return [Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)]
}

function toColor(value, fallback) {
    if (!Array.isArray(value) || value.length !== 3) return fallback
    const byte = (v) => Math.max(0, Math.min(255, toInt(v, 0)))
    return String(Qt.rgba(byte(value[0]) / 255, byte(value[1]) / 255, byte(value[2]) / 255, 1))
}

function configEnvelope(panel) {
    return {
        "type": panel.patternType,
        "width": panel.resolutionW,
        "height": panel.resolutionH,
        "crossline": panel.crossLine,
        "pos_neg_color": false,
        "positive color": rgbArray(panel.positiveColor),
        "negative color": rgbArray(panel.negativeColor)
    }
}

function applyConfigEnvelope(doc, panel) {
    panel.resolutionW = Math.max(1, toInt(doc.width, panel.resolutionW))
    panel.resolutionH = Math.max(1, toInt(doc.height, panel.resolutionH))
    panel.crossLine = doc.crossline === true
    panel.positiveColor = toColor(doc["positive color"], String(panel.positiveColor))
    panel.negativeColor = toColor(doc["negative color"], String(panel.negativeColor))
}

function specEnvelope(panel) {
    return {
        "type": panel.patternType,
        "width": 0,
        "height": 0,
        "crossline": panel.crossLine
    }
}

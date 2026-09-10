.pragma library

// Rig profiles behind the Calibration System combo.

var systems = [
    {
        "name": "Yuanman - SIDE (EV2785)",
        "file": "yuanman_ev2785.json",
        "pixel_top": "0.2478",
        "pixel_side": "0.155",
        "v_gap": ["42", "42", "42", "42"],
        "h_gap": ["250", "250", "240", "240"]
    },
    {
        "name": "Yuanman - SIDE (EV2730Q)",
        "file": "yuanman_ev2730q.json",
        "pixel_top": "0.2478",
        "pixel_side": "0.2478",
        "v_gap": ["42", "42", "42", "42"],
        "h_gap": ["250", "250", "240", "240"]
    },
    {
        "name": "Yinda",
        "file": "yinda.json",
        "pixel_top": "0.293",
        "pixel_side": "0.293",
        "v_gap": ["25", "25", "25", "25"],
        "h_gap": ["215", "215", "215", "215"]
    },
    {
        // Zeros are the shipped profile, not placeholders.
        "name": "Broland C++",
        "file": "brodand_cpp.json",
        "pixel_top": "0.532",
        "pixel_side": "0",
        "v_gap": ["0", "0", "0", "0"],
        "h_gap": ["0", "0", "0", "0"]
    }
];

var directions = ["n", "s", "w", "e"];

function names() {
    var out = [];
    for (var i = 0; i < systems.length; ++i) out.push(systems[i].name);
    return out;
}

function at(index) {
    return (index >= 0 && index < systems.length) ? systems[index] : null;
}

// name -> { fieldName: text }
function fieldsFor(index) {
    var s = at(index);
    if (!s) return null;

    var out = {};
    out["lineedit_pixel_size_top"] = s.pixel_top;
    out["lineedit_pixel_size_side"] = s.pixel_side;
    for (var i = 0; i < directions.length; ++i) {
        out["lineedit_v_gap_" + directions[i]] = s.v_gap[i];
        out["lineedit_h_gap_" + directions[i]] = s.h_gap[i];
    }
    return out;
}

.pragma library

// The rig profiles behind the Calibration System combo.
//
// Selecting a system applies that hardware's pixel sizes and screen gaps to the
// table fields the pipeline reads: lineedit_pixel_size_top / _side, and
// lineedit_h_gap_<dir> / lineedit_v_gap_<dir>. Nothing else about the system is
// a computation input -- the name itself reaches no service.
//
// SOURCE OF TRUTH is Server/v2.1.0/config/cali_system/*.json. The values are
// mirrored here rather than read from there because the server runs on the rig's
// machine and this client does not share its filesystem; a client cannot open a
// path that exists on another host. They are four numbers and eight gaps per
// system and they describe physical hardware, so they change when someone builds
// a new rig, not on a release cadence -- but if those files are edited, edit
// these too.
//
// The gap arrays are in the order the v2.0 form laid its fields out: N, S, W, E.
// That is why Yuanman reads 250, 250, 240, 240 -- opposite screens share a gap,
// which is what you would expect of a rig and what makes the order checkable.

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
        // pixel_side is 0 and every gap is 0 in the source file. That is the
        // profile as shipped, not a placeholder: this system has no side screens
        // to measure, so the side pixel size and the gaps are never read.
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

// name -> { fieldName: text } for every field the pipeline reads.
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

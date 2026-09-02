#pragma once

// Internals shared by the five files that implement CaliCompute.
//
// It was one 1100-line file holding the whole calibration pipeline. It is five
// now, cut where the data actually changes hands:
//
//   CaliCompute.cpp          the table itself: cells, the side/layer metadata,
//                            and the columns derived directly from a capture
//   CaliComputePipeline.cpp  a capture becoming a filled round, and the
//                            aggregation that scores one
//   CaliComputeNoise.cpp     finding the dense bands a monitor gap leaves in the
//                            crossings, and removing nodes inside them
//   CaliComputeSearch.cpp    solving for a distance: the three searches the
//                            CaliJob action drives
//   CaliComputePlots.cpp     extracting series for the client to draw
//
// Nothing here changes a formula. Every function kept its body, its name and its
// arithmetic; only the file it lives in is different. That property is the one
// worth protecting -- see the note in packages/moil_server/CMakeLists.txt about
// why there is no second copy of this engine to drift.
//
// Private to engine/cali. CaliCompute.h is the interface.

#include <QHash>
#include <QString>
#include <QStringList>

#include "CaliCompute.h"

namespace cali_detail {

// Accessor functions rather than namespace-scope constants: a QStringList at
// namespace scope in a header would be constructed once per translation unit
// during static initialisation, in an order no standard defines. A function-local
// static is built on first use, which is always after main() here.
const QStringList &dirs8();

// The order used when building x/y point lists -- matches
// get_ict_zfl_into_xlist_ylist. Not the same order as dirs8(), and the difference
// is load-bearing: the pairs are consumed positionally.
const QStringList &dirsXY();

// _dict_column_index (controller_cali_result.py).
const QHash<QString, int> &columnMap();

// str(float)-equivalent: shortest text that round-trips the double (Python str).
QString numText(double v);

double roundTo2(double v);

}  // namespace cali_detail

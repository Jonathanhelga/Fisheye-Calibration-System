#include "PlaneFit3dViz.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "moil_3d_algorithm.h"
#include "Moildev.h"

namespace PlaneFit3dViz {

using Eigen::Matrix3d;
using Eigen::MatrixXd;

namespace {

struct Plane {
    Vec3 normal{Vec3::Zero()};
    Vec3 centroid{Vec3::Zero()};
    double d = 0;
    std::vector<double> res;
};

// fit_plane_pca: unit normal (smallest singular direction), centroid, signed
// offset d and per-point orthogonal residuals.
Plane fitPlanePCA(const std::vector<Vec3> &pts) {
    Plane pl;
    const int n = static_cast<int>(pts.size());
    if (n == 0) return pl;
    Vec3 mean = Vec3::Zero();
    for (const auto &p : pts) mean += p;
    mean /= n;
    MatrixXd C(n, 3);
    for (int i = 0; i < n; ++i) C.row(i) = (pts[i] - mean).transpose();
    Eigen::JacobiSVD<MatrixXd> svd(C, Eigen::ComputeThinV);
    Vec3 normal = svd.matrixV().col(2);
    normal.normalize();
    pl.normal = normal;
    pl.centroid = mean;
    pl.d = -normal.dot(mean);
    pl.res.resize(n);
    for (int i = 0; i < n; ++i) pl.res[i] = (pts[i] - mean).dot(normal);
    return pl;
}

void summarizeResiduals(const std::vector<double> &res, double &thickness, double &rms) {
    if (res.empty()) {
        thickness = rms = Measure3d::nan();
        return;
    }
    double lo = res[0], hi = res[0], sq = 0;
    for (double r : res) {
        lo = std::min(lo, r);
        hi = std::max(hi, r);
        sq += r * r;
    }
    thickness = hi - lo;
    rms = std::sqrt(sq / res.size());
}

std::vector<std::string> uniqueDirections(const std::vector<Point3d> &df) {
    std::vector<std::string> out;
    for (const auto &p : df)
        if (std::find(out.begin(), out.end(), p.direction) == out.end()) out.push_back(p.direction);
    return out;
}

std::vector<Vec3> pointsForDir(const std::vector<Point3d> &df, const std::string &dir) {
    std::vector<Vec3> out;
    for (const auto &p : df)
        if (p.direction == dir) out.push_back(p.mid);
    return out;
}

// Plane basis (SVD on scaled centered points), normal made to face `faceTo`.
// `mean` is the CENTRE OF THE DRAWN PATCH (a point on the plane), not the
// centroid: the patch is the tightest rectangle around the points, and its
// centre is only the centroid when the points happen to be spread symmetrically.
struct PlaneInfo {
    bool ok = false;
    Vec3 normal{Vec3::Zero()}, mean{Vec3::Zero()}, basis1{Vec3::Zero()}, basis2{Vec3::Zero()};
    double half1 = 0, half2 = 0;
};

// ---- tightest oriented rectangle around the in-plane points ----------------
// The patch used to be a PCA-axis box centred on the centroid, which drifted off
// the points whenever their spread was lopsided and sat diagonally on a nearly
// square board. A minimum-area rectangle lands its corners on the outermost
// points, whatever direction the board actually runs in.

struct Rect2 {
    Eigen::Vector2d axis1{1, 0}, axis2{0, 1}, center{Eigen::Vector2d::Zero()};
    double half1 = 0, half2 = 0;
};

// Monotone-chain convex hull; returns the input unchanged when it degenerates.
std::vector<Eigen::Vector2d> convexHull2d(std::vector<Eigen::Vector2d> p) {
    std::sort(p.begin(), p.end(), [](const Eigen::Vector2d &a, const Eigen::Vector2d &b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });
    p.erase(std::unique(p.begin(), p.end(),
                        [](const Eigen::Vector2d &a, const Eigen::Vector2d &b) {
                            return (a - b).norm() < 1e-12;
                        }),
            p.end());
    const int n = static_cast<int>(p.size());
    if (n < 3) return p;
    auto cross = [](const Eigen::Vector2d &o, const Eigen::Vector2d &a, const Eigen::Vector2d &b) {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };
    std::vector<Eigen::Vector2d> h(2 * n);
    int k = 0;
    for (int i = 0; i < n; ++i) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && cross(h[k - 2], h[k - 1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    h.resize(std::max(0, k - 1));
    return h;
}

// Rotating calipers: the minimum-area rectangle is aligned with one hull edge.
Rect2 minAreaRect2d(const std::vector<Eigen::Vector2d> &pts) {
    Rect2 best;
    if (pts.empty()) return best;
    auto boxFor = [&pts](const Eigen::Vector2d &dir) {
        const Eigen::Vector2d perp(-dir.y(), dir.x());
        double dlo = 1e300, dhi = -1e300, plo = 1e300, phi = -1e300;
        for (const auto &q : pts) {
            const double a = q.dot(dir), b = q.dot(perp);
            dlo = std::min(dlo, a);
            dhi = std::max(dhi, a);
            plo = std::min(plo, b);
            phi = std::max(phi, b);
        }
        Rect2 r;
        r.axis1 = dir;
        r.axis2 = perp;
        r.half1 = (dhi - dlo) / 2.0;
        r.half2 = (phi - plo) / 2.0;
        r.center = dir * ((dlo + dhi) / 2.0) + perp * ((plo + phi) / 2.0);
        return r;
    };

    best = boxFor(Eigen::Vector2d(1, 0));  // fallback for a degenerate hull
    double bestArea = best.half1 * best.half2;
    const std::vector<Eigen::Vector2d> hull = convexHull2d(pts);
    for (size_t i = 0; i < hull.size(); ++i) {
        const Eigen::Vector2d e = hull[(i + 1) % hull.size()] - hull[i];
        if (e.norm() < 1e-9) continue;
        const Rect2 r = boxFor(e.normalized());
        const double area = r.half1 * r.half2;
        if (area < bestArea) {
            bestArea = area;
            best = r;
        }
    }
    return best;
}

PlaneInfo planeInfoForDir(const std::vector<Vec3> &pts, const Vec3 &faceTo) {
    PlaneInfo pi;
    const int n = static_cast<int>(pts.size());
    if (n < 3) return pi;
    Vec3 mean = Vec3::Zero();
    for (const auto &p : pts) mean += p;
    mean /= n;
    MatrixXd Pc(n, 3);
    for (int i = 0; i < n; ++i) Pc.row(i) = (pts[i] - mean).transpose();
    const double scale = Pc.norm();
    if (!std::isfinite(scale) || scale == 0.0) return pi;
    Pc /= scale;
    Eigen::JacobiSVD<MatrixXd> svd(Pc, Eigen::ComputeThinV);
    Vec3 normal = svd.matrixV().col(2);
    Vec3 basis1 = svd.matrixV().col(0);
    Vec3 basis2 = svd.matrixV().col(1);
    const double nrm = normal.norm();
    if (!std::isfinite(nrm) || nrm == 0.0) return pi;
    normal /= nrm;
    if (normal.dot(faceTo - mean) < 0) normal = -normal;

    // Project onto the plane, then wrap the projections in their tightest
    // rectangle: the patch's edges follow the board's own direction and its
    // corners land on the outermost points instead of on an offset PCA box.
    std::vector<Eigen::Vector2d> proj;
    proj.reserve(pts.size());
    for (const auto &p : pts)
        proj.emplace_back((p - mean).dot(basis1), (p - mean).dot(basis2));
    const Rect2 box = minAreaRect2d(proj);

    pi.ok = true;
    pi.normal = normal;
    // Back to 3D: the rectangle's axes and centre are expressed in (basis1,
    // basis2), so they map onto the plane by the same combination.
    pi.basis1 = box.axis1.x() * basis1 + box.axis1.y() * basis2;
    pi.basis2 = box.axis2.x() * basis1 + box.axis2.y() * basis2;
    pi.mean = mean + box.center.x() * basis1 + box.center.y() * basis2;
    pi.half1 = box.half1;
    pi.half2 = box.half2;
    return pi;
}

// -------- Plotly JS trace emitters (self-contained HTML via CDN) --------

std::string jsArr(const std::vector<double> &v) {
    std::ostringstream os;
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (std::isnan(v[i]))
            os << "null";
        else
            os << v[i];
        if (i + 1 < v.size()) os << ',';
    }
    os << ']';
    return os.str();
}

const std::map<std::string, std::string> kColorMap = {{"center", "black"}, {"north", "yellow"},
                                                      {"south", "brown"},  {"west", "green"},
                                                      {"east", "purple"}};

std::string markerTrace(const std::vector<double> &x, const std::vector<double> &y,
                        const std::vector<double> &z, const std::string &color, int size,
                        const std::string &name) {
    std::ostringstream os;
    os << "{type:'scatter3d',mode:'markers',x:" << jsArr(x) << ",y:" << jsArr(y)
       << ",z:" << jsArr(z) << ",marker:{size:" << size << ",color:'" << color << "'},name:'"
       << name << "'}";
    return os.str();
}

std::string lineTrace(const std::vector<double> &x, const std::vector<double> &y,
                      const std::vector<double> &z, const std::string &color, int width,
                      const std::string &name, bool showlegend) {
    std::ostringstream os;
    os << "{type:'scatter3d',mode:'lines',x:" << jsArr(x) << ",y:" << jsArr(y) << ",z:" << jsArr(z)
       << ",line:{width:" << width << ",color:'" << color << "'},name:'" << name
       << "',showlegend:" << (showlegend ? "true" : "false") << "}";
    return os.str();
}

std::string surfaceTrace(const std::vector<std::vector<double>> &X,
                         const std::vector<std::vector<double>> &Y,
                         const std::vector<std::vector<double>> &Z, const std::string &name) {
    auto grid = [](const std::vector<std::vector<double>> &G) {
        std::ostringstream os;
        os << '[';
        for (size_t i = 0; i < G.size(); ++i) {
            os << jsArr(G[i]);
            if (i + 1 < G.size()) os << ',';
        }
        os << ']';
        return os.str();
    };
    std::ostringstream os;
    os << "{type:'surface',showscale:false,opacity:0.3,hoverinfo:'skip',name:'" << name
       << "',colorscale:[[0,'gray'],[1,'gray']],x:" << grid(X) << ",y:" << grid(Y)
       << ",z:" << grid(Z) << "}";
    return os.str();
}

void writeHtml(const std::string &path, const std::string &title,
               const std::vector<std::string> &traces) {
    std::ofstream f(path);
    f << "<!DOCTYPE html><html><head><meta charset='utf-8'/>"
         "<script src='https://cdn.plot.ly/plotly-2.27.0.min.js'></script></head><body>"
         "<div id='plot' style='width:100%;height:95vh;'></div><script>\nvar data=[\n";
    for (size_t i = 0; i < traces.size(); ++i) {
        f << traces[i];
        if (i + 1 < traces.size()) f << ",\n";
    }
    f << "\n];\nvar layout={title:'" << title
      << "',margin:{l:10,r:10,t:40,b:10},scene:{xaxis:{title:'X'},yaxis:{title:'Y'},"
         "zaxis:{title:'Z'},aspectmode:'data'}};\nPlotly.newPlot('plot',data,layout);\n"
         "</script></body></html>\n";
}

// Shared traces: per-direction points, cameras, planes, axes, distance lines.
void addCommonTraces(std::vector<std::string> &traces, const std::vector<Point3d> &df,
                     const std::vector<std::string> &dirs,
                     const std::map<std::string, PlaneInfo> &planes, const Vec3 &camL,
                     const Vec3 &camR) {
    for (const auto &dir : dirs) {
        std::vector<double> xs, ys, zs;
        for (const auto &p : df)
            if (p.direction == dir) {
                xs.push_back(p.mid[0]);
                ys.push_back(p.mid[1]);
                zs.push_back(p.mid[2]);
            }
        const auto it = kColorMap.find(dir);
        traces.push_back(markerTrace(xs, ys, zs, it == kColorMap.end() ? "gray" : it->second, 3,
                                     "Dir: " + dir));
    }
    traces.push_back(markerTrace({camL[0]}, {camL[1]}, {camL[2]}, "red", 6, "Camera L"));
    traces.push_back(markerTrace({camR[0]}, {camR[1]}, {camR[2]}, "blue", 6, "Camera R"));

    // Planes (3x3 surfaces).
    const int g = 3;
    for (const auto &kv : planes) {
        const PlaneInfo &pi = kv.second;
        if (!pi.ok) continue;
        std::vector<std::vector<double>> X(g, std::vector<double>(g)), Y = X, Z = X;
        for (int i = 0; i < g; ++i) {
            const double u = -pi.half1 + (2.0 * pi.half1) * i / (g - 1);
            for (int j = 0; j < g; ++j) {
                const double v = -pi.half2 + (2.0 * pi.half2) * j / (g - 1);
                const Vec3 pt = pi.mean + u * pi.basis1 + v * pi.basis2;
                // meshgrid(grid1, grid2): row index over grid2, col over grid1.
                X[j][i] = pt[0];
                Y[j][i] = pt[1];
                Z[j][i] = pt[2];
            }
        }
        traces.push_back(surfaceTrace(X, Y, Z, "Plane " + kv.first));
    }

    // Axes.
    const double L = 300.0;
    traces.push_back(lineTrace({-L, L}, {0, 0}, {0, 0}, "red", 4, "X Axis", true));
    traces.push_back(lineTrace({0, 0}, {-L, L}, {0, 0}, "green", 4, "Y Axis", true));
    traces.push_back(lineTrace({0, 0}, {0, 0}, {-L, L}, "blue", 4, "Z Axis", true));

    // Distance-to-plane segments per point.
    for (const auto &p : df) {
        auto it = planes.find(p.direction);
        if (it == planes.end() || !it->second.ok) continue;
        const Vec3 &n = it->second.normal;
        const Vec3 &c = it->second.mean;
        const double dist = (p.mid - c).dot(n);
        const Vec3 proj = p.mid - dist * n;
        traces.push_back(lineTrace({p.mid[0], proj[0]}, {p.mid[1], proj[1]}, {p.mid[2], proj[2]},
                                   "#cc3333", 2, "", false));
    }
}

// P/Q markers + inter-ray connection lines (full figure only).
void addPqTraces(std::vector<std::string> &traces, const std::vector<Point3d> &df) {
    std::vector<double> px, py, pz, qx, qy, qz, mx, my, mz;
    for (const auto &p : df) {
        px.push_back(p.p[0]); py.push_back(p.p[1]); pz.push_back(p.p[2]);
        qx.push_back(p.q[0]); qy.push_back(p.q[1]); qz.push_back(p.q[2]);
        mx.push_back(p.mid[0]); my.push_back(p.mid[1]); mz.push_back(p.mid[2]);
    }
    traces.push_back(markerTrace(px, py, pz, "red", 3, "Closest point (Cam L)"));
    traces.push_back(markerTrace(qx, qy, qz, "blue", 3, "Closest point (Cam R)"));
    traces.push_back(markerTrace(mx, my, mz, "green", 3, "Mid Point (3D Result)"));
    for (size_t i = 0; i < df.size(); ++i)
        traces.push_back(lineTrace({df[i].p[0], df[i].q[0]}, {df[i].p[1], df[i].q[1]},
                                   {df[i].p[2], df[i].q[2]}, "gray", 1,
                                   i == 0 ? "Shortest inter-ray line" : "", i == 0));
}

// compute_grid_edges_3d: neighbour (right/down) edges per direction; writes CSV.
void writeGridEdges(const std::vector<Point3d> &df,
                    const std::map<std::string, std::pair<int, int>> &patternSize,
                    const std::string &outDir) {
    std::ofstream csv(outDir + "/grid_edges_lengths.csv");
    csv << "direction,edge_type,rows,columns,pid_cam1,pid_cam2,x1,y1,z1,x2,y2,z2,length(mm)\n";
    for (const auto &dir : uniqueDirections(df)) {
        auto ps = patternSize.find(dir);
        if (ps == patternSize.end()) continue;
        const int cols = ps->second.first;
        std::map<int, const Point3d *> byPid;
        for (const auto &p : df)
            if (p.direction == dir) byPid[p.pointId] = &p;
        auto has = [&](int r, int c) { return byPid.count(r * cols + c) > 0; };
        auto emit = [&](const char *type, int r, int c, int a, int b) {
            const Point3d *p1 = byPid[a];
            const Point3d *p2 = byPid[b];
            const double len = (p1->mid - p2->mid).norm();
            csv << dir << ',' << type << ',' << r << ',' << c << ',' << a << ',' << b << ','
                << p1->mid[0] << ',' << p1->mid[1] << ',' << p1->mid[2] << ',' << p2->mid[0] << ','
                << p2->mid[1] << ',' << p2->mid[2] << ',' << len << '\n';
        };
        for (const auto &kv : byPid) {
            const int pid = kv.first;
            const int r = pid / cols, c = pid % cols;
            if (has(r, c + 1)) emit("Horizontal", r, c, pid, r * cols + (c + 1));
            if (has(r + 1, c)) emit("Vertical", r, c, pid, (r + 1) * cols + c);
        }
    }
}

}  // namespace

std::vector<DirPlane> fitPlanesPerDirection(const std::vector<Point3d> &df3d, const Vec3 &faceTo) {
    std::vector<DirPlane> out;
    for (const auto &dir : uniqueDirections(df3d)) {
        const std::vector<Vec3> pts = pointsForDir(df3d, dir);
        const PlaneInfo pi = planeInfoForDir(pts, faceTo);
        if (!pi.ok) continue;  // fewer than 3 points / degenerate
        DirPlane dp;
        dp.direction = dir;
        dp.normal = pi.normal;
        dp.centroid = pi.mean;
        dp.basis1 = pi.basis1;
        dp.basis2 = pi.basis2;
        dp.half1 = pi.half1;
        dp.half2 = pi.half2;
        dp.pointCount = static_cast<int>(pts.size());
        const Plane pl = fitPlanePCA(pts);
        summarizeResiduals(pl.res, dp.thickness, dp.rms);
        out.push_back(dp);
    }
    return out;
}

VizResult show3dPoint2camOriVisualization(
    std::vector<Point3d> &df3d, const Vec3 &camL, const Vec3 &camR,
    const std::map<std::string, std::pair<int, int>> *patternSize,
    const std::string &outputPrefix) {
    VizResult vr;
    const std::string outDir = Measure3d::getOutputDir(outputPrefix);
    const Vec3 baselineCenter = (camL + camR) / 2.0;
    const std::vector<std::string> dirs = uniqueDirections(df3d);

    // plane_info per direction (SVD basis, normal facing baseline center).
    std::map<std::string, PlaneInfo> planes;
    for (const auto &dir : dirs) {
        PlaneInfo pi = planeInfoForDir(pointsForDir(df3d, dir), baselineCenter);
        if (pi.ok) planes[dir] = pi;
    }

    // Inter-plane angles (angle_map) with code letters.
    const std::map<std::string, std::string> code = {
        {"west", "w"}, {"north", "n"}, {"center", "c"}, {"east", "e"}, {"south", "s"}};
    std::vector<std::string> pdirs;
    for (const auto &kv : planes) pdirs.push_back(kv.first);
    for (size_t i = 0; i < pdirs.size(); ++i)
        for (size_t j = i + 1; j < pdirs.size(); ++j) {
            const Vec3 &n1 = planes[pdirs[i]].normal;
            const Vec3 &n2 = planes[pdirs[j]].normal;
            double c = std::max(-1.0, std::min(1.0, n1.dot(n2)));
            const double theta = std::acos(c) * 180.0 / M_PI;
            auto c1 = code.find(pdirs[i]), c2 = code.find(pdirs[j]);
            if (c1 != code.end() && c2 != code.end()) {
                vr.angleMap[c1->second + c2->second] = theta;
                vr.angleMap[c2->second + c1->second] = theta;
            }
        }

    // mean ray_gap per direction + all.
    {
        double allSum = 0;
        int allN = 0;
        for (const auto &dir : dirs) {
            double s = 0;
            int n = 0;
            for (const auto &p : df3d)
                if (p.direction == dir) {
                    s += p.rayGap;
                    ++n;
                }
            if (n) vr.meanMaps[dir] = s / n;
            allSum += s;
            allN += n;
        }
        if (allN) vr.meanMaps["all"] = allSum / allN;
    }

    // thickness per direction + all (mean_plane_dist) and depth-to-origin.
    for (const auto &dir : dirs) {
        std::vector<Vec3> pts = pointsForDir(df3d, dir);
        if (pts.size() < 3) continue;
        Plane pl = fitPlanePCA(pts);
        double thickness, rms;
        summarizeResiduals(pl.res, thickness, rms);
        vr.meanPlaneDist[dir] = thickness;
        vr.depthOriginPerDir[dir] = std::abs(pl.d);
    }
    if (df3d.size() >= 3) {
        std::vector<Vec3> all;
        for (const auto &p : df3d) all.push_back(p.mid);
        Plane pl = fitPlanePCA(all);
        double thickness, rms;
        summarizeResiduals(pl.res, thickness, rms);
        vr.meanPlaneDist["all"] = thickness;
    }

    // Grid edges CSV (also informs edge traces — omitted from HTML for brevity).
    if (patternSize != nullptr) writeGridEdges(df3d, *patternSize, outDir);

    // Build & write the two HTML views.
    std::vector<std::string> basic;
    addCommonTraces(basic, df3d, dirs, planes, camL, camR);
    vr.pathBasic = outDir + "/3d_points_only.html";
    writeHtml(vr.pathBasic, "3D Mid Points", basic);

    std::vector<std::string> full;
    addCommonTraces(full, df3d, dirs, planes, camL, camR);
    addPqTraces(full, df3d);
    vr.pathFull = outDir + "/3d_points_with_pq.html";
    writeHtml(vr.pathFull, "3D Points with Closest points on rays", full);

    return vr;
}

ReprojCompare compareReprojectionWithOriginal(const std::vector<Point3d> &df3d,
                                              const std::vector<Measure3d::DetPoint> &dfL,
                                              const std::vector<Measure3d::DetPoint> &dfR,
                                              const Vec3 &camL, const Vec3 &camR,
                                              const Moildev *moilL, const Moildev *moilR,
                                              const Eigen::Matrix3d *camRmat) {
    ReprojCompare rc;

    // Ground-truth (initial detection) pixels straight from the in-memory points.
    auto buildGt = [](const std::vector<Measure3d::DetPoint> &pts) {
        std::map<std::pair<std::string, int>, std::pair<double, double>> gt;
        for (const auto &p : pts)
            if (p.xFish >= 0 && p.yFish >= 0)
                gt[{p.direction, p.pointId}] = {static_cast<double>(p.xFish),
                                                static_cast<double>(p.yFish)};
        return gt;
    };
    const auto gtL = buildGt(dfL);
    const auto gtR = buildGt(dfR);
    if (gtL.empty() || gtR.empty()) return rc;

    Moil3dAlgorithm::Reprojector3d rpL(moilL, camL);
    Moil3dAlgorithm::Reprojector3d rpR(moilR, camR, camRmat);

    for (const auto &p : df3d) {
        auto lit = gtL.find({p.direction, p.pointId});
        auto rit = gtR.find({p.direction, p.pointId});
        if (lit == gtL.end() || rit == gtR.end()) continue;

        bool okL = false, okR = false;
        const auto uvL = rpL.reprojectPoint(p.mid, okL);
        const auto uvR = rpR.reprojectPoint(p.mid, okR);

        Measure3d::ReprojRow rowL, rowR;
        rowL.direction = rowR.direction = p.direction;
        rowL.pointId = rowR.pointId = p.pointId;
        rowL.uGt = lit->second.first;
        rowL.vGt = lit->second.second;
        rowR.uGt = rit->second.first;
        rowR.vGt = rit->second.second;
        if (okL) {
            rowL.u = uvL.first;
            rowL.v = uvL.second;
            rowL.error = std::hypot(rowL.u - rowL.uGt, rowL.v - rowL.vGt);
        }
        if (okR) {
            rowR.u = uvR.first;
            rowR.v = uvR.second;
            rowR.error = std::hypot(rowR.u - rowR.uGt, rowR.v - rowR.vGt);
        }
        rc.left.push_back(rowL);
        rc.right.push_back(rowR);
    }

    // Sort by (direction, point_id) and persist.
    auto bydir = [](const Measure3d::ReprojRow &a, const Measure3d::ReprojRow &b) {
        if (a.direction != b.direction) return a.direction < b.direction;
        return a.pointId < b.pointId;
    };
    std::sort(rc.left.begin(), rc.left.end(), bydir);
    std::sort(rc.right.begin(), rc.right.end(), bydir);

    const std::string evalDir = Measure3d::getOutputDir("reprojection");
    auto write = [](const std::string &path, const std::vector<Measure3d::ReprojRow> &rows,
                    char side) {
        std::ofstream f(path);
        f << "direction,point_id,u_" << side << ",v_" << side << ",u_" << side << "_gt,v_" << side
          << "_gt,error_" << side << "\n";
        for (const auto &r : rows) {
            auto g = [](double v) { return std::isnan(v) ? std::string() : std::to_string(v); };
            f << r.direction << ',' << r.pointId << ',' << g(r.u) << ',' << g(r.v) << ',' << g(r.uGt)
              << ',' << g(r.vGt) << ',' << g(r.error) << '\n';
        }
    };
    write(evalDir + "/reprojection_compare_left.csv", rc.left, 'L');
    write(evalDir + "/reprojection_compare_right.csv", rc.right, 'R');
    return rc;
}

}  // namespace PlaneFit3dViz

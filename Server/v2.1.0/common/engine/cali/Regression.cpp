#include "Regression.h"

#include <cmath>

#include <Eigen/Dense>

namespace Regression {

std::vector<double> polyFit(const std::vector<double> &x,
                            const std::vector<double> &y, int degree) {
    const int n = static_cast<int>(std::min(x.size(), y.size()));
    if (n == 0 || degree < 0 || n < degree + 1) return {};

    // Vandermonde design matrix A(i,j) = x[i]^j (j = 0..degree), same columns
    // as PolynomialFeatures(include_bias=True).
    Eigen::MatrixXd A(n, degree + 1);
    Eigen::VectorXd b(n);
    for (int i = 0; i < n; ++i) {
        double p = 1.0;
        for (int j = 0; j <= degree; ++j) {
            A(i, j) = p;
            p *= x[i];
        }
        b(i) = y[i];
    }

    const Eigen::VectorXd c = A.colPivHouseholderQr().solve(b);
    return std::vector<double>(c.data(), c.data() + c.size());
}

double polyEval(const std::vector<double> &coef, double x) {
    double y = 0.0, p = 1.0;
    for (double c : coef) {
        y += c * p;
        p *= x;
    }
    return y;
}

std::vector<double> polyPredict(const std::vector<double> &coef,
                                const std::vector<double> &x) {
    std::vector<double> out;
    out.reserve(x.size());
    for (double xi : x) out.push_back(polyEval(coef, xi));
    return out;
}

}  // namespace Regression

#pragma once

#include <vector>

// Polynomial least-squares regression (Eigen), replacing the
// sklearn PolynomialFeatures + LinearRegression usage in
// controller_cali_result.py.
namespace Regression {

// Fit y = c[0] + c[1]*x + ... + c[degree]*x^degree by least squares.
// Returns the (degree+1) coefficients, or empty on bad input.
std::vector<double> polyFit(const std::vector<double> &x,
                            const std::vector<double> &y, int degree);

double polyEval(const std::vector<double> &coef, double x);
std::vector<double> polyPredict(const std::vector<double> &coef,
                                const std::vector<double> &x);

}  // namespace Regression

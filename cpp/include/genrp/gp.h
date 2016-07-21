#ifndef _GENRP_GP_
#define _GENRP_GP_

#include <cmath>
#include <vector>
#include <complex>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "genrp/kernel.h"

namespace genrp {

#define GP_MUST_COMPUTE       -1
#define GP_DIMENSION_MISMATCH -2

// 0.5 * log(2 * pi)
#define GP_CONSTANT 0.91893853320467267

template <typename SolverType>
class GaussianProcess {
public:
  GaussianProcess (Kernel kernel) : kernel_(kernel), dim_(0), computed_(false) {}
  size_t size () const { return kernel_.size(); };
  Eigen::VectorXd params () const { return kernel_.params(); };
  void params (const Eigen::VectorXd& pars) const { kernel_.params(pars); };

  void compute (const Eigen::VectorXd x, const Eigen::VectorXd& yerr);
  void compute (const Eigen::VectorXd& params, const Eigen::VectorXd& x, const Eigen::VectorXd& yerr);
  double log_likelihood (const Eigen::VectorXd& y) const;
  double grad_log_likelihood (const Eigen::VectorXd& y, double* grad) const;

private:
  Kernel kernel_;
  SolverType solver_;
  size_t dim_;
  bool computed_;
  Eigen::VectorXd x_;

};

template <typename SolverType>
void GaussianProcess<SolverType>::compute (
    const Eigen::VectorXd& params, const Eigen::VectorXd& x, const Eigen::VectorXd& yerr) {
  kernel_.params(params);
  compute(x, yerr);
}

template <typename SolverType>
void GaussianProcess<SolverType>::compute (
    const Eigen::VectorXd x, const Eigen::VectorXd& yerr) {
  x_ = x;
  dim_ = x.rows();
  solver_ = SolverType(kernel_.alpha(), kernel_.beta());
  solver_.compute(x, yerr.array() * yerr.array());
  computed_ = true;
}

template <typename SolverType>
double GaussianProcess<SolverType>::log_likelihood (const Eigen::VectorXd& y) const {
  if (!computed_) throw GP_MUST_COMPUTE;
  if (y.rows() != dim_) throw GP_DIMENSION_MISMATCH;
  Eigen::VectorXd alpha(dim_);
  solver_.solve(y, &(alpha(0)));
  double ll = -0.5 * y.transpose() * alpha;
  ll -= 0.5 * solver_.log_determinant() + y.rows() * GP_CONSTANT;
  return ll;
}

template <typename SolverType>
double GaussianProcess<SolverType>::grad_log_likelihood (const Eigen::VectorXd& y, double* grad) const {
  if (!computed_) throw GP_MUST_COMPUTE;
  if (y.rows() != dim_) throw GP_DIMENSION_MISMATCH;
  Eigen::VectorXd alpha(dim_);
  solver_.solve(y, &(alpha(0)));

  // Compute the likelihood.
  double ll = -0.5 * y.transpose() * alpha;
  ll -= 0.5 * solver_.log_determinant() + y.rows() * GP_CONSTANT;

  // Compute 'alpha.alpha^T - K^-1' matrix.
  Eigen::MatrixXd Kinv(dim_, dim_);
  Eigen::VectorXd ident = Eigen::VectorXd::Zero(dim_);
  for (size_t i = 0; i < dim_; ++i) {
    ident(i) = 1.0;
    solver_.solve(ident, &(Kinv(0, i)));
    ident(i) = 0.0;

    Kinv.col(i) -= alpha * alpha(i);
  }
  Kinv.array() *= -1.0;

  // Compute the gradient matrix.
  size_t grad_size = kernel_.size(), ind1, ind2;
  Eigen::MatrixXd dKdt(grad_size, dim_*dim_);
  kernel_.grad(0.0, &(dKdt(0, 0)));
  std::cout << dKdt.col(0) << "\n\n";
  for (size_t i = 0; i < dim_; ++i) {
    dKdt.col(i*dim_ + i) = dKdt.col(0);
    for (size_t j = i+1; j < dim_; ++j) {
      kernel_.grad(x_(j) - x_(i), &(dKdt(0, i*dim_ + j)));
      dKdt.col(j*dim_ + i) = dKdt.col(i*dim_ + j);
    }
  }

  // Compute the gradient.
  Eigen::Map<Eigen::VectorXd> grad_map(grad, grad_size);
  grad_map.array() = Eigen::VectorXd::Zero(grad_size);
  for (size_t i = 0; i < dim_; ++i)
    grad_map += Kinv.row(i) * dKdt.block(0, i*dim_, grad_size, dim_).transpose();
  grad_map.array() *= 0.5;

  return ll;
}

};

#endif
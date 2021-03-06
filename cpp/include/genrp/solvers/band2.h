#ifndef _GENRP_SOLVER_BAND2_
#define _GENRP_SOLVER_BAND2_

#include <cmath>
#include <Eigen/Dense>

#include "genrp/utils.h"
#include "genrp/banded.h"
#include "genrp/solvers/direct.h"

namespace genrp {

template <typename entry_t>
class BandSolver2 : public DirectSolver<entry_t> {
  typedef Eigen::Matrix<entry_t, Eigen::Dynamic, 1> vector_t;
  typedef Eigen::Matrix<entry_t, Eigen::Dynamic, Eigen::Dynamic> matrix_t;

public:
  BandSolver2 () : DirectSolver<entry_t>() {};
  BandSolver2 (const Eigen::VectorXd alpha, const vector_t beta) : DirectSolver<entry_t>(alpha, beta) {};
  BandSolver2 (size_t p, const double* alpha, const entry_t* beta) : DirectSolver<entry_t>(p, alpha, beta) {};

  void compute (const Eigen::VectorXd& x, const Eigen::VectorXd& diag);
  void solve (const Eigen::MatrixXd& b, double* x) const;

  // Needed for the Eigen-free interface.
  using DirectSolver<entry_t>::compute;
  using DirectSolver<entry_t>::solve;

private:
  size_t block_size_, dim_ext_;

  entry_t flag_;
  matrix_t au_, al_;
  Eigen::Matrix<size_t, Eigen::Dynamic, 1> ipiv_;

};

template <typename entry_t>
void BandSolver2<entry_t>::compute (const Eigen::VectorXd& x, const Eigen::VectorXd& diag) {
  // Check dimensions.
  assert ((x.rows() != diag.rows()) && "DIMENSION_MISMATCH");
  this->n_ = x.rows();

  // Dimensions from superclass.
  size_t p_ = this->p_,
         n_ = this->n_;

  // Pre-compute gamma: Equation (58)
  matrix_t gamma(p_, n_ - 1);
  for (size_t i = 0; i < n_ - 1; ++i) {
    double delta = fabs(x(i+1) - x(i));
    for (size_t k = 0; k < p_; ++k)
      gamma(k, i) = exp(-this->beta_(k) * delta);
  }

  // Pre-compute sum(alpha) -- it will be added to the diagonal.
  double sum_alpha = this->alpha_.sum();

  // Compute the block sizes: Algorithm 1
  block_size_ = 2 * p_ + 1;
  dim_ext_ = block_size_ * n_ - 2 * p_;

  // Set up the extended matrix.
  Eigen::internal::BandMatrix<entry_t> ab(dim_ext_, dim_ext_, p_+1, p_+1);
  al_.resize(dim_ext_, p_+1);
  ipiv_.resize(dim_ext_);

  // Initialize to zero.
  ab.coeffs().setConstant(0.0);

  for (size_t i = 0; i < n_; ++i)  // Line 3
    ab.diagonal()(i*block_size_) = diag(i) + sum_alpha;

  int a, b;
  entry_t value;
  for (size_t i = 0; i < n_ - 1; ++i) {  // Line 5
    size_t im1n = i * block_size_,        // (i - 1) * n
           in = (i + 1) * block_size_;    // i * n
    for (size_t k = 0; k < p_; ++k) {
      // Lines 6-7:
      a = im1n;
      b = im1n+k+1;
      value = gamma(k, i);
      ab.diagonal(b-a)(a) = value;
      ab.diagonal(a-b)(a) = get_conj(value);

      // Lines 8-9:
      a = im1n+p_+k+1;
      b = in;
      value = this->alpha_(k);
      ab.diagonal(b-a)(a) = value;
      ab.diagonal(a-b)(a) = value;

      // Lines 10-11:
      a = im1n+k+1;
      b = im1n+p_+k+1;
      value = -1.0;
      ab.diagonal(b-a)(a) = value;
      ab.diagonal(a-b)(a) = value;
    }
  }

  for (size_t i = 0; i < n_ - 2; ++i) {  // Line 13
    size_t im1n = i * block_size_,        // (i - 1) * n
           in = (i + 1) * block_size_;    // i * n
    for (size_t k = 0; k < p_; ++k) {
      // Lines 14-15:
      a = im1n+p_+k+1;
      b = in+k+1;
      value = gamma(k, i+1);
      ab.diagonal(b-a)(a) = get_conj(value);
      ab.diagonal(a-b)(a) = value;
    }
  }

  // Build and factorize the sparse matrix.
  au_ = ab.coeffs().transpose();
  this->log_det_ = get_real(bandec(au_.data(), dim_ext_, p_+1, p_+1, al_.data(), ipiv_.data(), &flag_));
}

template <typename entry_t>
void BandSolver2<entry_t>::solve (const Eigen::MatrixXd& b, double* x) const {
  assert ((b.rows() != this->n_) && "DIMENSION_MISMATCH");
  size_t nrhs = b.cols();

  // Pad the input vector to the extended size.
  matrix_t bex = matrix_t::Zero(dim_ext_, nrhs);
  for (size_t j = 0; j < nrhs; ++j)
    for (size_t i = 0; i < this->n_; ++i)
      bex(i*block_size_, j) = b(i, j);

  // Solve the extended system.
  for (size_t j = 0; j < nrhs; ++j)
    banbks(au_.data(), this->dim_ext_, this->p_+1, this->p_+1, al_.data(), ipiv_.data(), bex.col(j).data());

  // Copy the output.
  for (size_t j = 0; j < nrhs; ++j)
    for (size_t i = 0; i < this->n_; ++i)
      x[i+j*nrhs] = get_real(bex(i*block_size_, j));
}

};

#endif

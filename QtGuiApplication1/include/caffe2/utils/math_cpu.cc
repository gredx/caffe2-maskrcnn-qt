// Implements the math functions for CPU.
// The implementation in this file allows us to route the underlying numerical
// computation library to different backends. Notably:
// (1) For all BLAS-related functions, one can explicitly request a BLAS backend
//     such as MKL, openblas or Atlas. To see the set of supported backends
//     currently provided, check //third_party/blas/.
// (2) If one chooses to link against MKL, we utilize MKL's vector math library
//     (VML) for a few functions such as Exp and Log.
// (3) Fallback implementations are provided in Eigen for cross-platform
//     support. Since Eigen is a header-only library and supports a number of
//     platforms, it allows one to quickly port Caffe2 to different platforms
//     where BLAS may not be present.

#include "caffe2/utils/eigen_utils.h"
#include "caffe2/utils/math.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "caffe2/core/context.h"
#include "caffe2/utils/cpu_neon.h"

#include "Eigen/Core"
#include "Eigen/Dense"

#ifdef CAFFE2_USE_MKL
#include <mkl.h>
#endif // CAFFE2_USE_MKL

#ifdef CAFFE2_USE_HPTT
#include <hptt.h>
#endif // CAFFE2_USE_HPTT

#if defined(_MSC_VER)
#include <process.h>
#endif

namespace caffe2 {

namespace math {

////////////////////////////////////////////////////////////////////////////////
// BLAS alternatives.
// Depending on whether we have specified an external BLAS library or not, we
// will delegate the Caffe math functions that are BLAS-related to either the
// CBLAS call or the Eigen implementation.
////////////////////////////////////////////////////////////////////////////////
#ifdef CAFFE2_USE_EIGEN_FOR_BLAS

// Caffe2 gemm provides a simpler interface to the gemm functions, with the
// limitation that the data has to be contiguous in memory.
//
// The gemm call implements the following operation:
//
//                  C = alpha * op(A) * op(B) + beta * C
//
// where op(A) has size M x K, op(B) has size K x N, and C has size M x N. Each
// of A, B, and C are matrices and alpha and beta are scalars. Note that the
// most common use case of gemm will involve setting alpha to 1 and beta to 0.
//
// op(A) and op(B) represent the transformations that are done to A and B before
// the matrix multiply; depending on the flags set, op(A) is equal to A or A^T
// (transpose) if the argument TransA or TransB is set to CblasNoTrans or
// CblasTrans, respectively, for each of A and B.
template <>
void Gemm<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const CBLAS_TRANSPOSE TransB,
    const int M,
    const int N,
    const int K,
    const float alpha,
    const float* A,
    const float* B,
    const float beta,
    float* C,
    CPUContext* context,
    TensorProto::DataType math_type) {
  auto C_mat = EigenMatrixMap<float>(C, N, M);
  if (beta == 0) {
    C_mat.setZero();
  } else {
    C_mat *= beta;
  }
  switch (TransA) {
    case CblasNoTrans: {
      switch (TransB) {
        case CblasNoTrans:
          C_mat.noalias() += alpha *
              (ConstEigenMatrixMap<float>(B, N, K) *
               ConstEigenMatrixMap<float>(A, K, M));
          return;
        case CblasTrans:
          C_mat.noalias() += alpha *
              (ConstEigenMatrixMap<float>(B, K, N).transpose() *
               ConstEigenMatrixMap<float>(A, K, M));
          return;
        default:
          LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransB";
      }
    }
    case CblasTrans: {
      switch (TransB) {
        case CblasNoTrans:
          C_mat.noalias() += alpha *
              (ConstEigenMatrixMap<float>(B, N, K) *
               ConstEigenMatrixMap<float>(A, M, K).transpose());
          return;
        case CblasTrans:
          C_mat.noalias() += alpha *
              (ConstEigenMatrixMap<float>(B, K, N).transpose() *
               ConstEigenMatrixMap<float>(A, M, K).transpose());
          return;
        default:
          LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransB";
      }
    }
    default:
      LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransA";
  }
}

template <>
void GemmEx<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const CBLAS_TRANSPOSE TransB,
    const int M,
    const int N,
    const int K,
    const float alpha,
    const float* A,
    const int lda,
    const float* B,
    const int ldb,
    const float beta,
    float* C,
    const int ldc,
    CPUContext*) {
  using OuterStride = Eigen::OuterStride<Eigen::Dynamic>;
  using StridedMap = Eigen::Map<Eigen::MatrixXf, 0, OuterStride>;
  using ConstStridedMap = Eigen::Map<const Eigen::MatrixXf, 0, OuterStride>;
  auto C_mat = StridedMap(C, N, M, OuterStride(ldc));
  if (beta == 0) {
    C_mat.setZero();
  } else {
    C_mat *= beta;
  }
  switch (TransA) {
    case CblasNoTrans: {
      switch (TransB) {
        case CblasNoTrans:
          C_mat.noalias() += alpha *
              (ConstStridedMap(B, N, K, OuterStride(ldb)) *
               ConstStridedMap(A, K, M, OuterStride(lda)));
          return;
        case CblasTrans:
          C_mat.noalias() += alpha *
              (ConstStridedMap(B, K, N, OuterStride(ldb)).transpose() *
               ConstStridedMap(A, K, M, OuterStride(lda)));
          return;
        default:
          LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransB";
      }
    }
    case CblasTrans: {
      switch (TransB) {
        case CblasNoTrans:
          C_mat.noalias() += alpha *
              (ConstStridedMap(B, N, K, OuterStride(ldb)) *
               ConstStridedMap(A, M, K, OuterStride(lda)).transpose());
          return;
        case CblasTrans:
          C_mat.noalias() += alpha *
              (ConstStridedMap(B, K, N, OuterStride(ldb)).transpose() *
               ConstStridedMap(A, M, K, OuterStride(lda)).transpose());
          return;
        default:
          LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransB";
      }
    }
    default:
      LOG(FATAL) << "Unexpected CBLAS_TRANSPOSE for TransA";
  }
}

template <>
void Gemv<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const int M,
    const int N,
    const float alpha,
    const float* A,
    const float* x,
    const float beta,
    float* y,
    CPUContext* context,
    TensorProto::DataType math_type) {
  EigenVectorMap<float> y_vec(y, TransA == CblasNoTrans ? M : N);
  if (beta == 0) {
    // In Caffe2 we often do a lazy initialization, which may contain NaNs in
    // the float values. As a result, if beta is 0, we explicitly do a setzero.
    y_vec.setZero();
  } else {
    y_vec *= beta;
  }
  switch (TransA) {
    case CblasNoTrans: {
      y_vec.noalias() += alpha *
          (ConstEigenMatrixMap<float>(A, N, M).transpose() *
           ConstEigenVectorMap<float>(x, N));
      return;
    }
    case CblasTrans: {
      y_vec.noalias() += alpha *
          (ConstEigenMatrixMap<float>(A, N, M) *
           ConstEigenVectorMap<float>(x, M));
      return;
    }
    default:
      LOG(FATAL) << "Gemv float found an unexpected CBLAS_TRANSPOSE input.";
  }
}

#define CAFFE2_SPECIALIZED_SCALE(T)                                            \
  template <>                                                                  \
  void Scale<T, CPUContext>(                                                   \
      const int n, const float alpha, const T* x, T* y, CPUContext* context) { \
    EigenVectorMap<T>(y, n) = ConstEigenVectorMap<T>(x, n) * alpha;            \
  }                                                                            \
  template <>                                                                  \
  void Scale<T, CPUContext>(                                                   \
      const int n,                                                             \
      const float* alpha,                                                      \
      const T* x,                                                              \
      T* y,                                                                    \
      CPUContext* context) {                                                   \
    EigenVectorMap<T>(y, n) = ConstEigenVectorMap<T>(x, n) * (*alpha);         \
  }
CAFFE2_SPECIALIZED_SCALE(float)
#undef CAFFE2_SPECIALIZED_SCALE

#define CAFFE2_SPECIALIZED_DOT(T)                                        \
  template <>                                                            \
  void Dot<T, CPUContext>(                                               \
      const int N, const T* a, const T* b, T* y, CPUContext* context) {  \
    *y = ConstEigenVectorMap<T>(a, N).dot(ConstEigenVectorMap<T>(b, N)); \
  }
CAFFE2_SPECIALIZED_DOT(float)
#undef CAFFE2_SPECIALIZED_DOT

#define CAFFE2_SPECIALIZED_AXPY(T)                                          \
  template <>                                                               \
  void Axpy<T, CPUContext>(                                                 \
      const int N, const T alpha, const T* x, T* Y, CPUContext* context) {  \
    EigenVectorMap<T>(Y, N) += ConstEigenVectorMap<T>(x, N) * alpha;        \
  }                                                                         \
  template <>                                                               \
  void Axpy<T, CPUContext>(                                                 \
      const int N, const T* alpha, const T* x, T* Y, CPUContext* context) { \
    EigenVectorMap<T>(Y, N) += ConstEigenVectorMap<T>(x, N) * (*alpha);     \
  }
CAFFE2_SPECIALIZED_AXPY(float)
#undef CAFFE2_SPECIALIZED_AXPY

#define CAFFE2_SPECIALIZED_AXPBY(T)                              \
  template <>                                                    \
  void Axpby<T, CPUContext>(                                     \
      const int N,                                               \
      const T alpha,                                             \
      const T* x,                                                \
      const T beta,                                              \
      T* y,                                                      \
      CPUContext* context) {                                     \
    EigenVectorMap<T> y_vec(y, N);                               \
    y_vec = y_vec * beta + ConstEigenVectorMap<T>(x, N) * alpha; \
  }
CAFFE2_SPECIALIZED_AXPBY(float)
#undef CAFFE2_SPECIALIZED_AXPBY

#else // CAFFE2_USE_EIGEN_FOR_BLAS

template <>
void Gemm<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const CBLAS_TRANSPOSE TransB,
    const int M,
    const int N,
    const int K,
    const float alpha,
    const float* A,
    const float* B,
    const float beta,
    float* C,
    CPUContext* /*context*/,
    TensorProto::DataType /*math_type*/) {
  int lda = (TransA == CblasNoTrans) ? K : M;
  int ldb = (TransB == CblasNoTrans) ? N : K;
  cblas_sgemm(
      CblasRowMajor,
      TransA,
      TransB,
      M,
      N,
      K,
      alpha,
      A,
      lda,
      B,
      ldb,
      beta,
      C,
      N);
}

template <>
void GemmEx<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const CBLAS_TRANSPOSE TransB,
    const int M,
    const int N,
    const int K,
    const float alpha,
    const float* A,
    const int lda,
    const float* B,
    const int ldb,
    const float beta,
    float* C,
    const int ldc,
    CPUContext* /*context*/) {
  cblas_sgemm(
      CblasRowMajor,
      TransA,
      TransB,
      M,
      N,
      K,
      alpha,
      A,
      lda,
      B,
      ldb,
      beta,
      C,
      ldc);
}

template <>
void Gemv<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const int M,
    const int N,
    const float alpha,
    const float* A,
    const float* x,
    const float beta,
    float* y,
    CPUContext* /*context*/,
    TensorProto::DataType /*math_type*/) {
  cblas_sgemv(CblasRowMajor, TransA, M, N, alpha, A, N, x, 1, beta, y, 1);
}

#define CAFFE2_SPECIALIZED_SCALE(T, prefix)                             \
  template <>                                                           \
  void Scale<T, CPUContext>(                                            \
      const int n, const float alpha, const T* x, T* y, CPUContext*) {  \
    if (y != x)                                                         \
      cblas_##prefix##copy(n, x, 1, y, 1);                              \
    cblas_##prefix##scal(n, static_cast<float>(alpha), y, 1);           \
  }                                                                     \
  template <>                                                           \
  void Scale<T, CPUContext>(                                            \
      const int n, const float* alpha, const T* x, T* y, CPUContext*) { \
    if (y != x)                                                         \
      cblas_##prefix##copy(n, x, 1, y, 1);                              \
    cblas_##prefix##scal(n, static_cast<float>(*alpha), y, 1);          \
  }
CAFFE2_SPECIALIZED_SCALE(float, s)
#undef CAFFE2_SPECIALIZED_SCALE

#define CAFFE2_SPECIALIZED_DOT(T, prefix)                       \
  template <>                                                   \
  void Dot<T, CPUContext>(                                      \
      const int N, const T* a, const T* b, T* y, CPUContext*) { \
    *y = cblas_##prefix##dot(N, a, 1, b, 1);                    \
  }
CAFFE2_SPECIALIZED_DOT(float, s)
#undef CAFFE2_SPECIALIZED_DOT

#define CAFFE2_SPECIALIZED_AXPY(T, prefix)                          \
  template <>                                                       \
  void Axpy<T, CPUContext>(                                         \
      const int N, const T alpha, const T* x, T* y, CPUContext*) {  \
    cblas_##prefix##axpy(N, alpha, x, 1, y, 1);                     \
  }                                                                 \
  template <>                                                       \
  void Axpy<T, CPUContext>(                                         \
      const int N, const T* alpha, const T* x, T* y, CPUContext*) { \
    cblas_##prefix##axpy(N, *alpha, x, 1, y, 1);                    \
  }
CAFFE2_SPECIALIZED_AXPY(float, s)
#undef CAFFE2_SPECIALIZED_AXPY

// cblas_[sd]axpby is not a standard blas function, and if MKL is not present,
// we will need to implement it.
#ifdef CAFFE2_USE_MKL
#define CAFFE2_SPECIALIZED_AXPBY(T, prefix)            \
  template <>                                          \
  void Axpby<T, CPUContext>(                           \
      const int N,                                     \
      const T alpha,                                   \
      const T* x,                                      \
      const T beta,                                    \
      T* y,                                            \
      CPUContext*) {                                   \
    cblas_##prefix##axpby(N, alpha, x, 1, beta, y, 1); \
  }
#else // CAFFE2_USE_MKL
#define CAFFE2_SPECIALIZED_AXPBY(T, prefix)     \
  template <>                                   \
  void Axpby<T, CPUContext>(                    \
      const int N,                              \
      const T alpha,                            \
      const T* x,                               \
      const T beta,                             \
      T* y,                                     \
      CPUContext*) {                            \
    cblas_##prefix##scal(N, beta, y, 1);        \
    cblas_##prefix##axpy(N, alpha, x, 1, y, 1); \
  }
#endif // CAFFE2_USE_MKL
CAFFE2_SPECIALIZED_AXPBY(float, s)
#undef CAFFE2_SPECIALIZED_AXPBY

#endif // CAFFE2_USE_EIGEN_FOR_BLAS

template <>
void GemmBatched<float, CPUContext>(
    const CBLAS_TRANSPOSE TransA,
    const CBLAS_TRANSPOSE TransB,
    const int batch_size,
    const int M,
    const int N,
    const int K,
    const float alpha,
    const float* A,
    const float* B,
    const float beta,
    float* C,
    CPUContext* context,
    Tensor<CPUContext>*, /* scratch */
    TensorProto::DataType /* math_type */) {
  const int a_stride = M * K;
  const int b_stride = K * N;
  const int c_stride = M * N;

#ifdef CAFFE2_USE_MKL
  (void)context;

  const int lda = (TransA == CblasNoTrans) ? K : M;
  const int ldb = (TransB == CblasNoTrans) ? N : K;
  std::vector<const float*> a_array(batch_size, nullptr);
  std::vector<const float*> b_array(batch_size, nullptr);
  std::vector<float*> c_array(batch_size, nullptr);
  for (int i = 0; i < batch_size; ++i) {
    a_array[i] = A + a_stride * i;
    b_array[i] = B + b_stride * i;
    c_array[i] = C + c_stride * i;
  }
  cblas_sgemm_batch(
      CblasRowMajor,
      &TransA,
      &TransB,
      &M,
      &N,
      &K,
      &alpha,
      a_array.data(),
      &lda,
      b_array.data(),
      &ldb,
      &beta,
      c_array.data(),
      &N, // ldc_array
      1,
      &batch_size);
#else // CAFFE2_USE_MKL
  // loop over matrices in the batch
  for (int i = 0; i < batch_size; ++i) {
    math::Gemm<float, CPUContext>(
        TransA,
        TransB,
        M,
        N,
        K,
        alpha,
        A + a_stride * i,
        B + b_stride * i,
        beta,
        C + c_stride * i,
        context);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// MKL VML alternatives.
// Depending on whether we are using MKL, we will delegate the Caffe math
// functions that are VML-related to either the VML call or the Eigen
// implementation. If you are setting the flags (such as AVX) right for your CPU
// architecture, usually Eigen will deliver a throughput as fast as the VML
// functions.
////////////////////////////////////////////////////////////////////////////////
#ifdef CAFFE2_USE_MKL

#define DELEGATE_SIMPLE_UNARY_FUNCTION(T, Funcname, OriginalFunc, ...)       \
  template <>                                                                \
  void Funcname<T, CPUContext>(const int N, const T* x, T* y, CPUContext*) { \
    OriginalFunc(N, x, y, ##__VA_ARGS__);                                    \
  }
DELEGATE_SIMPLE_UNARY_FUNCTION(
    float,
    Exp,
    vmsExp,
    VML_HA | VML_FTZDAZ_OFF | VML_ERRMODE_IGNORE)
DELEGATE_SIMPLE_UNARY_FUNCTION(
    double,
    Exp,
    vmdExp,
    VML_HA | VML_FTZDAZ_OFF | VML_ERRMODE_IGNORE)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Log, vsLn)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Log, vdLn)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Cos, vsCos)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Cos, vdCos)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Acos, vsAcos)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Acos, vdAcos)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sin, vsSin)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Sin, vdSin)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Asin, vsAsin)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Asin, vdAsin)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Tan, vsTan)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Tan, vdTan)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Atan, vsAtan)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Atan, vdAtan)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sinh, vsSinh)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Sinh, vdSinh)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Cosh, vsCosh)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Cosh, vdCosh)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Tanh, vsTanh)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Tanh, vdTanh)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Abs, vsAbs)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Abs, vdAbs)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sqr, vsSqr)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Sqr, vdSqr)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sqrt, vsSqrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Sqrt, vdSqrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Rsqrt, vsInvSqrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Rsqrt, vdInvSqrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Cbrt, vsCbrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(double, Cbrt, vdCbrt)
#undef DELEGATE_SIMPLE_UNARY_FUNCTION

#define DELEGATE_SINCOS_FUNCTION(T, OriginalFunc)           \
  template <>                                               \
  void SinCos<T, CPUContext>(                               \
      const int N, const T* a, T* ys, T* yc, CPUContext*) { \
    OriginalFunc(N, a, ys, yc);                             \
  }
DELEGATE_SINCOS_FUNCTION(float, vsSinCos)
DELEGATE_SINCOS_FUNCTION(double, vdSinCos)
#undef DELEGATE_SINCOS_FUNCTION

#define DELEGATE_POWX_FUNCTION(T, OriginalFunc)                               \
  template <>                                                                 \
  void Powx<T, CPUContext>(const int N, const T* a, T b, T* y, CPUContext*) { \
    OriginalFunc(N, a, b, y);                                                 \
  }
DELEGATE_POWX_FUNCTION(float, vsPowx)
DELEGATE_POWX_FUNCTION(double, vdPowx)
#undef DELEGATE_POWX_FUNCTION

#define DELEGATE_SIMPLE_BINARY_FUNCTION(T, Func, FuncImpl)      \
  template <>                                                   \
  void Func<T, CPUContext>(                                     \
      const int N, const T* A, const T* B, T* C, CPUContext*) { \
    FuncImpl(N, A, B, C);                                       \
  }
DELEGATE_SIMPLE_BINARY_FUNCTION(float, Add, vsAdd)
DELEGATE_SIMPLE_BINARY_FUNCTION(double, Add, vdAdd)
DELEGATE_SIMPLE_BINARY_FUNCTION(float, Sub, vsSub)
DELEGATE_SIMPLE_BINARY_FUNCTION(double, Sub, vdSub)
DELEGATE_SIMPLE_BINARY_FUNCTION(float, Mul, vsMul)
DELEGATE_SIMPLE_BINARY_FUNCTION(double, Mul, vdMul)
DELEGATE_SIMPLE_BINARY_FUNCTION(float, Div, vsDiv)
DELEGATE_SIMPLE_BINARY_FUNCTION(double, Div, vdDiv)
#undef DELEGATE_SIMPLE_BINARY_FUNCTION

#else // CAFFE2_USE_MKL

#define DELEGATE_SIMPLE_UNARY_FUNCTION(T, Funcname, expr)                    \
  template <>                                                                \
  void Funcname<T, CPUContext>(const int N, const T* x, T* y, CPUContext*) { \
    EigenVectorMap<T>(y, N) = ConstEigenVectorArrayMap<T>(x, N).expr();      \
  }
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Exp, exp)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Log, log)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Cos, cos)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Acos, acos)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sin, sin)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Asin, asin)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Tan, tan)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Atan, atan)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Abs, abs)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sqr, square)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Sqrt, sqrt)
DELEGATE_SIMPLE_UNARY_FUNCTION(float, Rsqrt, rsqrt)

#undef DELEGATE_SIMPLE_UNARY_FUNCTION

#define DELEGATE_SINCOS_FUNCTION(T)                                     \
  template <>                                                           \
  void SinCos<T, CPUContext>(                                           \
      const int N, const T* x, T* ys, T* yc, CPUContext*) {             \
    EigenVectorMap<T>(ys, N) = ConstEigenVectorArrayMap<T>(x, N).sin(); \
    EigenVectorMap<T>(yc, N) = ConstEigenVectorArrayMap<T>(x, N).cos(); \
  }
DELEGATE_SINCOS_FUNCTION(float)
DELEGATE_SINCOS_FUNCTION(double)
#undef DELEGATE_SINCOS_FUNCTION

#define DELEGATE_TANH_FUNCTION(T)                                             \
  template <>                                                                 \
  void Tanh<T, CPUContext>(const int N, const T* X, T* Y, CPUContext*) {      \
    EigenVectorMap<T>(Y, N) = T(1) -                                          \
        ((ConstEigenVectorArrayMap<T>(X, N) * T(2)).exp() + T(1)).inverse() * \
            T(2);                                                             \
  }
DELEGATE_TANH_FUNCTION(float)
DELEGATE_TANH_FUNCTION(double)
#undef DELEGATE_TANH_FUNCTION

#define DELEGATE_CBRT_FUNCTION(T)                                        \
  template <>                                                            \
  void Cbrt<T, CPUContext>(const int N, const T* X, T* Y, CPUContext*) { \
    std::transform(X, X + N, Y, [](const T x) { return cbrt(x); });      \
  }
DELEGATE_CBRT_FUNCTION(float)
DELEGATE_CBRT_FUNCTION(double)
#undef DELEGATE_CBRT_FUNCTION

#define DELEGATE_POWX_FUNCTION(T)                                       \
  template <>                                                           \
  void Powx<T, CPUContext>(                                             \
      const int N, const T* a, const T b, T* y, CPUContext*) {          \
    EigenVectorMap<T>(y, N) = ConstEigenVectorArrayMap<T>(a, N).pow(b); \
  }
DELEGATE_POWX_FUNCTION(float)
#undef DELEGATE_POWX_FUNCTION

#define DELEGATE_SINH_FUNCTION(T)                                        \
  template <>                                                            \
  void Sinh<T, CPUContext>(const int N, const T* X, T* Y, CPUContext*) { \
    ConstEigenVectorArrayMap<T> X_arr(X, N);                             \
    EigenVectorMap<T>(Y, N) = (X_arr.exp() - (-X_arr).exp()) / 2;        \
  }
DELEGATE_SINH_FUNCTION(float)
DELEGATE_SINH_FUNCTION(double)
#undef DELEGATE_SINH_FUNCTION

#define DELEGATE_COSH_FUNCTION(T)                                        \
  template <>                                                            \
  void Cosh<T, CPUContext>(const int N, const T* X, T* Y, CPUContext*) { \
    ConstEigenVectorArrayMap<T> X_arr(X, N);                             \
    EigenVectorMap<T>(Y, N) = (X_arr.exp() + (-X_arr).exp()) / 2;        \
  }
DELEGATE_COSH_FUNCTION(float)
DELEGATE_COSH_FUNCTION(double)
#undef DELEGATE_COSH_FUNCTION

#endif // CAFFE2_USE_MKL

#define DELEGATE_NEG_FUNCTION(T)                                        \
  template <>                                                           \
  void Neg<T, CPUContext>(const int N, const T* x, T* y, CPUContext*) { \
    EigenVectorMap<T>(y, N) = -ConstEigenVectorMap<T>(x, N);            \
  }
DELEGATE_NEG_FUNCTION(float)
DELEGATE_NEG_FUNCTION(double)
DELEGATE_NEG_FUNCTION(std::int32_t)
DELEGATE_NEG_FUNCTION(std::int64_t)
#undef DELEGATE_NEG_FUNCTION

#define DELEGATE_SIGN_FUNCTION(T)                                        \
  template <>                                                            \
  void Sign<T, CPUContext>(const int N, const T* x, T* y, CPUContext*) { \
    EigenVectorMap<T>(y, N) = ConstEigenVectorArrayMap<T>(x, N).sign();  \
  }
DELEGATE_SIGN_FUNCTION(float)
DELEGATE_SIGN_FUNCTION(double)
DELEGATE_SIGN_FUNCTION(std::int32_t)
DELEGATE_SIGN_FUNCTION(std::int64_t)
#undef DELEGATE_SIGN_FUNCTION

#define DELEGATE_CUBE_FUNCTION(T)                                        \
  template <>                                                            \
  void Cube<T, CPUContext>(const int N, const T* X, T* Y, CPUContext*) { \
    EigenVectorMap<T>(Y, N) = ConstEigenVectorArrayMap<T>(X, N).cube();  \
  }
DELEGATE_CUBE_FUNCTION(float)
DELEGATE_CUBE_FUNCTION(double)
DELEGATE_CUBE_FUNCTION(std::int32_t)
DELEGATE_CUBE_FUNCTION(std::int64_t)
#undef DELEGATE_CUBE_FUNCTION

#define EIGEN_SIMPLE_BINARY_FUNCTION(T, Func, expr)             \
  template <>                                                   \
  void Func<T, CPUContext>(                                     \
      const int N, const T* A, const T* B, T* C, CPUContext*) { \
    EigenVectorMap<T>(C, N) = ConstEigenVectorArrayMap<T>(A, N) \
        expr ConstEigenVectorArrayMap<T>(B, N);                 \
  }

#ifdef CAFFE2_USE_MKL

#define DEFINE_SIMPLE_BINARY_FUNCTION(Func, expr)        \
  EIGEN_SIMPLE_BINARY_FUNCTION(std::int32_t, Func, expr) \
  EIGEN_SIMPLE_BINARY_FUNCTION(std::int64_t, Func, expr)

#else

#define DEFINE_SIMPLE_BINARY_FUNCTION(Func, expr)        \
  EIGEN_SIMPLE_BINARY_FUNCTION(float, Func, expr)        \
  EIGEN_SIMPLE_BINARY_FUNCTION(double, Func, expr)       \
  EIGEN_SIMPLE_BINARY_FUNCTION(std::int32_t, Func, expr) \
  EIGEN_SIMPLE_BINARY_FUNCTION(std::int64_t, Func, expr)

#endif

DEFINE_SIMPLE_BINARY_FUNCTION(Add, +)
DEFINE_SIMPLE_BINARY_FUNCTION(Sub, -)
DEFINE_SIMPLE_BINARY_FUNCTION(Mul, *)
DEFINE_SIMPLE_BINARY_FUNCTION(Div, /)

#undef DEFINE_SIMPLE_BINARY_FUNCTION
#undef EIGEN_SIMPLE_BINARY_FUNCTION

////////////////////////////////////////////////////////////////////////////////
// Common math functions being used in Caffe that do not have a BLAS or MKL
// equivalent. For all these functions, we will simply implement them either via
// Eigen or via custom code.
////////////////////////////////////////////////////////////////////////////////

#define CAFFE2_SPECIALIZED_REDUCEMIN(T)    \
  template <>                              \
  void ReduceMin<T, CPUContext>(           \
      const int N,                         \
      const T* x,                          \
      T* y,                                \
      Tensor<CPUContext>* /*scratch_ptr*/, \
      CPUContext* /*context*/) {           \
    *y = *std::min_element(x, x + N);      \
  }
CAFFE2_SPECIALIZED_REDUCEMIN(float)
#undef CAFFE2_SPECIALIZED_REDUCEMIN

#define CAFFE2_SPECIALIZED_REDUCEMAX(T)    \
  template <>                              \
  void ReduceMax<T, CPUContext>(           \
      const int N,                         \
      const T* x,                          \
      T* y,                                \
      Tensor<CPUContext>* /*scratch_ptr*/, \
      CPUContext* /*context*/) {           \
    *y = *std::max_element(x, x + N);      \
  }
CAFFE2_SPECIALIZED_REDUCEMAX(float)
CAFFE2_SPECIALIZED_REDUCEMAX(int32_t)
CAFFE2_SPECIALIZED_REDUCEMAX(int64_t)

#undef CAFFE2_SPECIALIZED_REDUCEMAX

namespace {

template <typename T, class Reducer>
void ReduceTensor(
    const int num_dims,
    const int* dims,
    const int num_axes,
    const int* axes,
    const Reducer& reducer,
    const T& init,
    const T* X,
    T* Y,
    CPUContext* context) {
  CAFFE_ENFORCE_LE(num_axes, num_dims);
  if (X == Y) {
    return;
  }
  std::vector<int> Y_dims(dims, dims + num_dims);
  for (int i = 0; i < num_axes; ++i) {
    Y_dims[axes[i]] = 1;
  }
  const int X_size =
      std::accumulate(dims, dims + num_dims, 1, std::multiplies<int>());
  const int Y_size = std::accumulate(
      Y_dims.cbegin(), Y_dims.cend(), 1, std::multiplies<int>());
  Set<T, CPUContext>(Y_size, init, Y, context);
  std::vector<int> index(num_dims, 0);
  for (int X_index = 0; X_index < X_size; ++X_index) {
    const int Y_index =
        utils::GetIndexFromDims(num_dims, Y_dims.data(), index.data());
    Y[Y_index] = reducer(Y[Y_index], X[X_index]);
    utils::IncreaseIndexInDims(num_dims, dims, index.data());
  }
}

template <typename T>
void ReduceMeanImpl(
    const int num_dims,
    const int* dims,
    const int num_axes,
    const int* axes,
    const T* X,
    T* Y,
    CPUContext* context) {
  ReduceTensor(
      num_dims, dims, num_axes, axes, std::plus<T>(), T(0), X, Y, context);
  const int X_size =
      std::accumulate(dims, dims + num_dims, 1, std::multiplies<int>());
  int scale = 1;
  for (int i = 0; i < num_axes; ++i) {
    scale *= dims[axes[i]];
  }
  const int Y_size = X_size / scale;
  Scale<T, CPUContext>(Y_size, 1.0f / static_cast<float>(scale), Y, Y, context);
}

} // namespace

#define CAFFE2_SPECIALIZED_REDUCE_MIN(T)                       \
  template <>                                                  \
  void ReduceMin<T, CPUContext>(                               \
      const int num_dims,                                      \
      const int* dims,                                         \
      const int num_axes,                                      \
      const int* axes,                                         \
      const T* X,                                              \
      T* Y,                                                    \
      CPUContext* context) {                                   \
    ReduceTensor(                                              \
        num_dims,                                              \
        dims,                                                  \
        num_axes,                                              \
        axes,                                                  \
        [](const T& a, const T& b) { return std::min(a, b); }, \
        std::numeric_limits<T>::max(),                         \
        X,                                                     \
        Y,                                                     \
        context);                                              \
  }
CAFFE2_SPECIALIZED_REDUCE_MIN(std::int32_t)
CAFFE2_SPECIALIZED_REDUCE_MIN(std::int64_t)
CAFFE2_SPECIALIZED_REDUCE_MIN(float)
CAFFE2_SPECIALIZED_REDUCE_MIN(double)
#undef CAFFE2_SPECIALIZED_REDUCE_MIN

#define CAFFE2_SPECIALIZED_REDUCE_MAX(T)                       \
  template <>                                                  \
  void ReduceMax<T, CPUContext>(                               \
      const int num_dims,                                      \
      const int* dims,                                         \
      const int num_axes,                                      \
      const int* axes,                                         \
      const T* X,                                              \
      T* Y,                                                    \
      CPUContext* context) {                                   \
    ReduceTensor(                                              \
        num_dims,                                              \
        dims,                                                  \
        num_axes,                                              \
        axes,                                                  \
        [](const T& a, const T& b) { return std::max(a, b); }, \
        std::numeric_limits<T>::lowest(),                      \
        X,                                                     \
        Y,                                                     \
        context);                                              \
  }
CAFFE2_SPECIALIZED_REDUCE_MAX(std::int32_t)
CAFFE2_SPECIALIZED_REDUCE_MAX(std::int64_t)
CAFFE2_SPECIALIZED_REDUCE_MAX(float)
CAFFE2_SPECIALIZED_REDUCE_MAX(double)
#undef CAFFE2_SPECIALIZED_REDUCE_MAX

#define CAFFE2_SPECIALIZED_REDUCE_SUM(T)                                      \
  template <>                                                                 \
  void ReduceSum<T, CPUContext>(                                              \
      const int num_dims,                                                     \
      const int* dims,                                                        \
      const int num_axes,                                                     \
      const int* axes,                                                        \
      const T* X,                                                             \
      T* Y,                                                                   \
      CPUContext* context) {                                                  \
    ReduceTensor(                                                             \
        num_dims, dims, num_axes, axes, std::plus<T>(), T(0), X, Y, context); \
  }
CAFFE2_SPECIALIZED_REDUCE_SUM(std::int32_t)
CAFFE2_SPECIALIZED_REDUCE_SUM(std::int64_t)
CAFFE2_SPECIALIZED_REDUCE_SUM(float)
CAFFE2_SPECIALIZED_REDUCE_SUM(double)
#undef CAFFE2_SPECIALIZED_REDUCE_SUM

#define CAFFE2_SPECIALIZED_REDUCE_MEAN(T)                             \
  template <>                                                         \
  void ReduceMean<T, CPUContext>(                                     \
      const int num_dims,                                             \
      const int* dims,                                                \
      const int num_axes,                                             \
      const int* axes,                                                \
      const T* X,                                                     \
      T* Y,                                                           \
      CPUContext* context) {                                          \
    ReduceMeanImpl<T>(num_dims, dims, num_axes, axes, X, Y, context); \
  }
CAFFE2_SPECIALIZED_REDUCE_MEAN(float)
#undef CAFFE2_SPECIALIZED_REDUCE_MEAN

#define CAFFE2_SPECIALIZED_REDUCE_L1(T)                         \
  template <>                                                   \
  void ReduceL1<T, CPUContext>(                                 \
      const int num_dims,                                       \
      const int* dims,                                          \
      const int num_axes,                                       \
      const int* axes,                                          \
      const T* X,                                               \
      T* Y,                                                     \
      CPUContext* context) {                                    \
    ReduceTensor(                                               \
        num_dims,                                               \
        dims,                                                   \
        num_axes,                                               \
        axes,                                                   \
        [](const T& a, const T& b) { return a + std::abs(b); }, \
        T(0),                                                   \
        X,                                                      \
        Y,                                                      \
        context);                                               \
  }
CAFFE2_SPECIALIZED_REDUCE_L1(float)
#undef CAFFE2_SPECIALIZED_REDUCE_L1

#define CAFFE2_SPECIALIZED_REDUCE_L2(T)                             \
  template <>                                                       \
  void ReduceL2<T, CPUContext>(                                     \
      const int num_dims,                                           \
      const int* dims,                                              \
      const int num_axes,                                           \
      const int* axes,                                              \
      const T* X,                                                   \
      T* Y,                                                         \
      CPUContext* context) {                                        \
    ReduceTensor(                                                   \
        num_dims,                                                   \
        dims,                                                       \
        num_axes,                                                   \
        axes,                                                       \
        [](const T& a, const T& b) { return a + b * b; },           \
        T(0),                                                       \
        X,                                                          \
        Y,                                                          \
        context);                                                   \
    std::vector<int> Y_dims(dims, dims + num_dims);                 \
    for (int i = 0; i < num_axes; ++i) {                            \
      Y_dims[axes[i]] = 1;                                          \
    }                                                               \
    const int Y_size = std::accumulate(                             \
        Y_dims.cbegin(), Y_dims.cend(), 1, std::multiplies<int>()); \
    Sqrt<T, CPUContext>(Y_size, Y, Y, context);                     \
  }
CAFFE2_SPECIALIZED_REDUCE_L2(float)
#undef CAFFE2_SPECIALIZED_REDUCE_L2

namespace {

template <typename T>
void BroadcastImpl(
    const int X_ndim,
    const int* X_dims,
    const int Y_ndim,
    const int* Y_dims,
    const T* X,
    T* Y) {
  CAFFE_ENFORCE_LE(X_ndim, Y_ndim);
  std::vector<int> X_dims_ex(Y_ndim);
  const int d = Y_ndim - X_ndim;
  std::fill(X_dims_ex.begin(), X_dims_ex.begin() + d, 1);
  for (int i = d; i < Y_ndim; ++i) {
    CAFFE_ENFORCE(X_dims[i - d] == 1 || X_dims[i - d] == Y_dims[i]);
    X_dims_ex[i] = X_dims[i - d];
  }
  const int Y_size =
      std::accumulate(Y_dims, Y_dims + Y_ndim, 1, std::multiplies<int>());
  std::vector<int> index(Y_ndim, 0);
  for (int Y_index = 0; Y_index < Y_size; ++Y_index) {
    const int X_index =
        utils::GetIndexFromDims(Y_ndim, X_dims_ex.data(), index.data());
    Y[Y_index] = X[X_index];
    utils::IncreaseIndexInDims(Y_ndim, Y_dims, index.data());
  }
}

} // namespace

#define CAFFE2_SPECIALIZED_BROADCAST(T)                     \
  template <>                                               \
  void Broadcast<T, CPUContext>(                            \
      const int X_ndim,                                     \
      const int* X_dims,                                    \
      const int Y_ndim,                                     \
      const int* Y_dims,                                    \
      const T* X,                                           \
      T* Y,                                                 \
      CPUContext* /* context */) {                          \
    BroadcastImpl<T>(X_ndim, X_dims, Y_ndim, Y_dims, X, Y); \
  }
CAFFE2_SPECIALIZED_BROADCAST(std::int32_t)
CAFFE2_SPECIALIZED_BROADCAST(std::int64_t)
CAFFE2_SPECIALIZED_BROADCAST(float)
CAFFE2_SPECIALIZED_BROADCAST(double)
#undef CAFFE2_SPECIALIZED_BROADCAST

namespace {

template <typename T>
void MomentsImpl(
    const int num_dims,
    const int* dims,
    const int num_axes,
    const int* axes,
    const T* X,
    T* mean,
    T* variance,
    CPUContext* context) {
  std::vector<int> Y_dims(dims, dims + num_dims);
  for (int i = 0; i < num_axes; ++i) {
    Y_dims[axes[i]] = 1;
  }
  const int X_size =
      std::accumulate(dims, dims + num_dims, 1, std::multiplies<int>());
  const int Y_size = std::accumulate(
      Y_dims.cbegin(), Y_dims.cend(), 1, std::multiplies<int>());
  const int scale = X_size / Y_size;
  Set<T, CPUContext>(Y_size, T(0), mean, context);
  Set<T, CPUContext>(Y_size, T(0), variance, context);
  std::vector<int> index(num_dims, 0);
  for (int X_index = 0; X_index < X_size; ++X_index) {
    const int Y_index =
        utils::GetIndexFromDims(num_dims, Y_dims.data(), index.data());
    mean[Y_index] += X[X_index];
    variance[Y_index] += X[X_index] * X[X_index];
    utils::IncreaseIndexInDims(num_dims, dims, index.data());
  }
  for (int Y_index = 0; Y_index < Y_size; ++Y_index) {
    mean[Y_index] /= static_cast<T>(scale);
    variance[Y_index] = variance[Y_index] / static_cast<T>(scale) -
        mean[Y_index] * mean[Y_index];
  }
}

} // namespace

#define CAFFE2_SPECIALIZED_MOMENTS(T)                                \
  template <>                                                        \
  void Moments<T, CPUContext>(                                       \
      const int num_dims,                                            \
      const int* dims,                                               \
      const int num_axes,                                            \
      const int* axes,                                               \
      const T* X,                                                    \
      T* mean,                                                       \
      T* variance,                                                   \
      CPUContext* context) {                                         \
    MomentsImpl<T>(                                                  \
        num_dims, dims, num_axes, axes, X, mean, variance, context); \
  }
CAFFE2_SPECIALIZED_MOMENTS(float)
#undef CAFFE2_SPECIALIZED_MOMENTS

#define CAFFE2_SPECIALIZED_ROWWISEMAX(T)                         \
  template <>                                                    \
  void RowwiseMax<T, CPUContext>(                                \
      const int N, const int D, const T* x, T* y, CPUContext*) { \
    EigenVectorMap<T>(y, N) =                                    \
        ConstEigenMatrixMap<T>(x, D, N).colwise().maxCoeff();    \
  }
CAFFE2_SPECIALIZED_ROWWISEMAX(float)
#undef CAFFE2_SPECIALIZED_ROWWISEMAX

#define CAFFE2_SPECIALIZED_COLWISEMAX(T)                         \
  template <>                                                    \
  void ColwiseMax<T, CPUContext>(                                \
      const int N, const int D, const T* x, T* y, CPUContext*) { \
    EigenVectorMap<T>(y, D) =                                    \
        ConstEigenMatrixMap<T>(x, D, N).rowwise().maxCoeff();    \
  }
CAFFE2_SPECIALIZED_COLWISEMAX(float)
#undef CAFFE2_SPECIALIZED_COLWISEMAX

#define CAFFE2_SPECIALIZED_ELEMWISEMAX(T)                                   \
  template <>                                                               \
  void ElemwiseMax<T, CPUContext>(                                          \
      const int N, const T* x, const T* y, T* z, CPUContext* /*context*/) { \
    std::transform(x, x + N, y, z, [](const T& x_i, const T& y_i) {         \
      return std::max(x_i, y_i);                                            \
    });                                                                     \
  }
CAFFE2_SPECIALIZED_ELEMWISEMAX(float)
#undef CAFFE2_SPECIALIZED_ELEMWISEMAX

#define CAFFE2_SPECIALIZED_MAXIMUM(T)                                          \
  template <>                                                                  \
  void Maximum<T, CPUContext>(                                                 \
      const int N, const float alpha, const T* x, T* y, CPUContext* context) { \
    std::transform(                                                            \
        x, x + N, y, [&alpha](const T& x_i) { return std::max(x_i, alpha); }); \
  }
CAFFE2_SPECIALIZED_MAXIMUM(float)
#undef CAFFE2_SPECIALIZED_MAXIMUM

// The actual implementation uses eigen which is column major, so notice the
// row/column swap in the actual implementation.

#define DELEGATE_EIGEN_2D_BROADCAST_1ST_BINARY_FUNCTION(T, Func, expr) \
  template <>                                                          \
  void Rowwise##Func<T, CPUContext, true>(                             \
      const int rows,                                                  \
      const int cols,                                                  \
      const T* A,                                                      \
      const T* B,                                                      \
      T* C,                                                            \
      CPUContext*) {                                                   \
    if (C == B) {                                                      \
      EigenArrayMap<T>(C, cols, rows).colwise() expr## =               \
          ConstEigenVectorArrayMap<T>(A, cols);                        \
    } else {                                                           \
      EigenArrayMap<T>(C, cols, rows) =                                \
          ConstEigenArrayMap<T>(B, cols, rows)                         \
              .colwise() expr ConstEigenVectorArrayMap<T>(A, cols);    \
    }                                                                  \
  }                                                                    \
  template <>                                                          \
  void Colwise##Func<T, CPUContext, true>(                             \
      const int rows,                                                  \
      const int cols,                                                  \
      const T* A,                                                      \
      const T* B,                                                      \
      T* C,                                                            \
      CPUContext*) {                                                   \
    if (C == B) {                                                      \
      EigenArrayMap<T>(C, cols, rows).rowwise() expr## =               \
          ConstEigenVectorArrayMap<T>(A, rows).transpose();            \
    } else {                                                           \
      EigenArrayMap<T>(C, cols, rows) =                                \
          ConstEigenArrayMap<T>(B, cols, rows)                         \
              .rowwise() expr ConstEigenVectorArrayMap<T>(A, rows)     \
              .transpose();                                            \
    }                                                                  \
  }

#define DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(T, Func, expr) \
  template <>                                                          \
  void Rowwise##Func<T, CPUContext, false>(                            \
      const int rows,                                                  \
      const int cols,                                                  \
      const T* A,                                                      \
      const T* B,                                                      \
      T* C,                                                            \
      CPUContext*) {                                                   \
    if (C == A) {                                                      \
      EigenArrayMap<T>(C, cols, rows).colwise() expr## =               \
          ConstEigenVectorArrayMap<T>(B, cols);                        \
    } else {                                                           \
      EigenArrayMap<T>(C, cols, rows) =                                \
          ConstEigenArrayMap<T>(A, cols, rows)                         \
              .colwise() expr ConstEigenVectorArrayMap<T>(B, cols);    \
    }                                                                  \
  }                                                                    \
  template <>                                                          \
  void Colwise##Func<T, CPUContext, false>(                            \
      const int rows,                                                  \
      const int cols,                                                  \
      const T* A,                                                      \
      const T* B,                                                      \
      T* C,                                                            \
      CPUContext*) {                                                   \
    if (C == A) {                                                      \
      EigenArrayMap<T>(C, cols, rows).rowwise() expr## =               \
          ConstEigenVectorArrayMap<T>(B, rows).transpose();            \
    } else {                                                           \
      EigenArrayMap<T>(C, cols, rows) =                                \
          ConstEigenArrayMap<T>(A, cols, rows)                         \
              .rowwise() expr ConstEigenVectorArrayMap<T>(B, rows)     \
              .transpose();                                            \
    }                                                                  \
  }

#define DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(T, Func, expr) \
  DELEGATE_EIGEN_2D_BROADCAST_1ST_BINARY_FUNCTION(T, Func, expr)   \
  DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(T, Func, expr)

#define DEFINE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(Func, expr)           \
  DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(float, Func, expr)        \
  DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(double, Func, expr)       \
  DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(std::int32_t, Func, expr) \
  DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(std::int64_t, Func, expr)

DEFINE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(Add, +)
DEFINE_EIGEN_2D_BROADCAST_BINARY_FUNCTION(Mul, *)

#undef DEFINE_EIGEN_2D_BROADCAST_BINARY_FUNCTION
#undef DELEGATE_EIGEN_2D_BROADCAST_BINARY_FUNCTION

#define DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION(T)           \
  template <>                                               \
  void RowwiseSub<T, CPUContext, true>(                     \
      const int rows,                                       \
      const int cols,                                       \
      const T* A,                                           \
      const T* B,                                           \
      T* C,                                                 \
      CPUContext*) {                                        \
    EigenArrayMap<T>(C, cols, rows) =                       \
        (-ConstEigenArrayMap<T>(B, cols, rows)).colwise() + \
        ConstEigenVectorArrayMap<T>(A, cols);               \
  }                                                         \
  template <>                                               \
  void ColwiseSub<T, CPUContext, true>(                     \
      const int rows,                                       \
      const int cols,                                       \
      const T* A,                                           \
      const T* B,                                           \
      T* C,                                                 \
      CPUContext*) {                                        \
    EigenArrayMap<T>(C, cols, rows) =                       \
        (-ConstEigenArrayMap<T>(B, cols, rows)).rowwise() + \
        ConstEigenVectorArrayMap<T>(A, rows).transpose();   \
  }                                                         \
  DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(T, Sub, -)

DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION(float)
DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION(double)
DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION(std::int32_t)
DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION(std::int64_t)

#undef DEFINE_EIGEN_2D_BROADCAST_SUB_FUNCTION

#define DEFINE_EIGEN_2D_BROADCAST_DIV_FUNCTION(T)                  \
  template <>                                                      \
  void RowwiseDiv<T, CPUContext, true>(                            \
      const int rows,                                              \
      const int cols,                                              \
      const T* A,                                                  \
      const T* B,                                                  \
      T* C,                                                        \
      CPUContext*) {                                               \
    EigenArrayMap<T>(C, cols, rows) =                              \
        ConstEigenArrayMap<T>(B, cols, rows).inverse().colwise() * \
        ConstEigenVectorArrayMap<T>(A, cols);                      \
  }                                                                \
  template <>                                                      \
  void ColwiseDiv<T, CPUContext, true>(                            \
      const int rows,                                              \
      const int cols,                                              \
      const T* A,                                                  \
      const T* B,                                                  \
      T* C,                                                        \
      CPUContext*) {                                               \
    EigenArrayMap<T>(C, cols, rows) =                              \
        ConstEigenArrayMap<T>(B, cols, rows).inverse().rowwise() * \
        ConstEigenVectorArrayMap<T>(A, rows).transpose();          \
  }                                                                \
  DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(T, Div, /)

DEFINE_EIGEN_2D_BROADCAST_DIV_FUNCTION(float)
DEFINE_EIGEN_2D_BROADCAST_DIV_FUNCTION(double)
DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(std::int32_t, Div, /)
DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION(std::int64_t, Div, /)

#undef DEFINE_EIGEN_2D_BROADCAST_DIV_FUNCTION

#undef DELEGATE_EIGEN_2D_BROADCAST_1ST_BINARY_FUNCTION
#undef DELEGATE_EIGEN_2D_BROADCAST_2ND_BINARY_FUNCTION

#define CAFFE2_SPECIALIZED_SET(T)                                             \
  template <>                                                                 \
  void Set<T, CPUContext>(const size_t N, const T alpha, T* Y, CPUContext*) { \
    if (alpha == (T)0) {                                                      \
      if (Y != nullptr) {                                                     \
        memset(Y, 0, N * sizeof(T));                                          \
      }                                                                       \
    } else {                                                                  \
      EigenVectorMap<T>(Y, N).setConstant(alpha);                             \
    }                                                                         \
  }

CAFFE2_SPECIALIZED_SET(float);
CAFFE2_SPECIALIZED_SET(double);
CAFFE2_SPECIALIZED_SET(int8_t);
CAFFE2_SPECIALIZED_SET(int16_t);
CAFFE2_SPECIALIZED_SET(int);
CAFFE2_SPECIALIZED_SET(int64_t);
CAFFE2_SPECIALIZED_SET(bool);
CAFFE2_SPECIALIZED_SET(char);
CAFFE2_SPECIALIZED_SET(uint8_t);
CAFFE2_SPECIALIZED_SET(uint16_t);
#undef CAFFE2_SPECIALIZED_SET

template <>
void Not<bool, CPUContext>(
    const int N,
    const bool* x,
    bool* y,
    CPUContext* /*context*/) {
  for (int i = 0; i < N; ++i) {
    y[i] = !x[i];
  }
}

#undef CAFFE2_DEFINE_BINARY_OP
#undef CAFFE2_INSTANTIATE_BINARY_OP

#define CAFFE2_SPECIALIZED_CPU_ADD_STRIPED_BATCH(T)             \
  template <>                                                   \
  void AddStripedBatch(                                         \
      const int N,                                              \
      const T* first,                                           \
      T* y,                                                     \
      const int stripe,                                         \
      const int batch,                                          \
      CPUContext* context) {                                    \
    for (int j = 0; j < batch; j++) {                           \
      Add<T, CPUContext>(N, first + j * stripe, y, y, context); \
    }                                                           \
  }

CAFFE2_SPECIALIZED_CPU_ADD_STRIPED_BATCH(float);
#undef CAFFE2_SPECIALIZED_CPU_ADD_STRIPED_BATCH

namespace {

template <typename TIn, typename TOut, class BinaryOperator, bool kBroadcast1st>
void RowwiseBinaryOp(
    const int rows,
    const int cols,
    const BinaryOperator& op,
    const TIn* A,
    const TIn* B,
    TOut* C) {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      const int C_index = i * cols + j;
      const int A_index = kBroadcast1st ? j : C_index;
      const int B_index = kBroadcast1st ? C_index : j;
      C[C_index] = op(A[A_index], B[B_index]);
    }
  }
}

template <typename TIn, typename TOut, class BinaryOperator, bool kBroadcast1st>
void ColwiseBinaryOp(
    const int rows,
    const int cols,
    const BinaryOperator& op,
    const TIn* A,
    const TIn* B,
    TOut* C) {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      const int C_index = i * cols + j;
      const int A_index = kBroadcast1st ? i : C_index;
      const int B_index = kBroadcast1st ? C_index : i;
      C[C_index] = op(A[A_index], B[B_index]);
    }
  }
}

template <typename TIn, typename TOut, class Operator1, class Operator2>
void BinaryOpWith2DBroadcasting(
    const int ndim,
    const int* dims,
    const int pivot,
    const bool broadcast_1st,
    const Operator1& op1,
    const Operator2& op2,
    const TIn* A,
    const TIn* B,
    TOut* C,
    CPUContext* context) {
  const int rows =
      std::accumulate(dims, dims + pivot, 1, std::multiplies<int>());
  const int cols =
      std::accumulate(dims + pivot, dims + ndim, 1, std::multiplies<int>());
  if (broadcast_1st) {
    op1(rows, cols, A, B, C, context);
  } else {
    op2(rows, cols, A, B, C, context);
  }
}

template <typename TIn, typename TOut, class BinaryOperator>
void BroadcastBinaryOpImpl(
    const int ndim,
    const int* A_dims,
    const int* B_dims,
    const int* C_dims,
    const BinaryOperator& op,
    const TIn* A,
    const TIn* B,
    TOut* C) {
  std::vector<int> index(ndim, 0);
  const int C_size =
      std::accumulate(C_dims, C_dims + ndim, 1, std::multiplies<int>());
  for (int C_index = 0; C_index < C_size; ++C_index) {
    const int A_index = utils::GetIndexFromDims(ndim, A_dims, index.data());
    const int B_index = utils::GetIndexFromDims(ndim, B_dims, index.data());
    C[C_index] = op(A[A_index], B[B_index]);
    utils::IncreaseIndexInDims(ndim, C_dims, index.data());
  }
}

} // namespace

#define DELEGATE_1D_BINARY_FUNCTION(TIn, TOut, Func, Op)               \
  template <>                                                          \
  void Func<TIn, CPUContext>(                                          \
      const int N, const TIn* A, const TIn* B, TOut* C, CPUContext*) { \
    std::transform(A, A + N, B, C, Op<TIn>());                         \
  }

#define DEFINE_1D_COMPARE_FUNCTION(Func, Op)                \
  DELEGATE_1D_BINARY_FUNCTION(float, bool, Func, Op)        \
  DELEGATE_1D_BINARY_FUNCTION(double, bool, Func, Op)       \
  DELEGATE_1D_BINARY_FUNCTION(std::int32_t, bool, Func, Op) \
  DELEGATE_1D_BINARY_FUNCTION(std::int64_t, bool, Func, Op) \
  DELEGATE_1D_BINARY_FUNCTION(bool, bool, Func, Op)

DEFINE_1D_COMPARE_FUNCTION(EQ, std::equal_to)
DEFINE_1D_COMPARE_FUNCTION(NE, std::not_equal_to)
DEFINE_1D_COMPARE_FUNCTION(LT, std::less)
DEFINE_1D_COMPARE_FUNCTION(LE, std::less_equal)
DEFINE_1D_COMPARE_FUNCTION(GT, std::greater)
DEFINE_1D_COMPARE_FUNCTION(GE, std::greater_equal)

#undef DEFINE_1D_COMPARE_FUNCTION

DELEGATE_1D_BINARY_FUNCTION(bool, bool, And, std::logical_and)
DELEGATE_1D_BINARY_FUNCTION(bool, bool, Or, std::logical_or)
DELEGATE_1D_BINARY_FUNCTION(bool, bool, Xor, std::bit_xor)

#define DEFINE_1D_BITWISE_BINARY_FUNCTION(Func, op)                 \
  DELEGATE_1D_BINARY_FUNCTION(bool, bool, Func, op)                 \
  DELEGATE_1D_BINARY_FUNCTION(std::int32_t, std::int32_t, Func, op) \
  DELEGATE_1D_BINARY_FUNCTION(std::int64_t, std::int64_t, Func, op)

DEFINE_1D_BITWISE_BINARY_FUNCTION(BitwiseAnd, std::bit_and)
DEFINE_1D_BITWISE_BINARY_FUNCTION(BitwiseOr, std::bit_or)
DEFINE_1D_BITWISE_BINARY_FUNCTION(BitwiseXor, std::bit_xor)

#undef DEFINE_1D_BITWISE_BINARY_FUNCTION

#undef DELEGATE_1D_BINARY_FUNCTION

#define DELEGATE_2D_BROADCAST_BINARY_FUNCTION(TIn, TOut, Func, Op)             \
  template <>                                                                  \
  void Rowwise##Func<TIn, CPUContext, true>(                                   \
      const int rows,                                                          \
      const int cols,                                                          \
      const TIn* A,                                                            \
      const TIn* B,                                                            \
      TOut* C,                                                                 \
      CPUContext*) {                                                           \
    RowwiseBinaryOp<TIn, TOut, Op<TIn>, true>(rows, cols, Op<TIn>(), A, B, C); \
  }                                                                            \
  template <>                                                                  \
  void Rowwise##Func<TIn, CPUContext, false>(                                  \
      const int rows,                                                          \
      const int cols,                                                          \
      const TIn* A,                                                            \
      const TIn* B,                                                            \
      TOut* C,                                                                 \
      CPUContext*) {                                                           \
    RowwiseBinaryOp<TIn, TOut, Op<TIn>, false>(                                \
        rows, cols, Op<TIn>(), A, B, C);                                       \
  }                                                                            \
  template <>                                                                  \
  void Colwise##Func<TIn, CPUContext, true>(                                   \
      const int rows,                                                          \
      const int cols,                                                          \
      const TIn* A,                                                            \
      const TIn* B,                                                            \
      TOut* C,                                                                 \
      CPUContext*) {                                                           \
    ColwiseBinaryOp<TIn, TOut, Op<TIn>, true>(rows, cols, Op<TIn>(), A, B, C); \
  }                                                                            \
  template <>                                                                  \
  void Colwise##Func<TIn, CPUContext, false>(                                  \
      const int rows,                                                          \
      const int cols,                                                          \
      const TIn* A,                                                            \
      const TIn* B,                                                            \
      TOut* C,                                                                 \
      CPUContext*) {                                                           \
    ColwiseBinaryOp<TIn, TOut, Op<TIn>, false>(                                \
        rows, cols, Op<TIn>(), A, B, C);                                       \
  }

#define DEFINE_2D_COMPARE_FUNCTION(Func, Op)                          \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(float, bool, Func, Op)        \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(double, bool, Func, Op)       \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(std::int32_t, bool, Func, Op) \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(std::int64_t, bool, Func, Op) \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(bool, bool, Func, Op)

DEFINE_2D_COMPARE_FUNCTION(EQ, std::equal_to)
DEFINE_2D_COMPARE_FUNCTION(NE, std::not_equal_to)
DEFINE_2D_COMPARE_FUNCTION(LT, std::less)
DEFINE_2D_COMPARE_FUNCTION(LE, std::less_equal)
DEFINE_2D_COMPARE_FUNCTION(GT, std::greater)
DEFINE_2D_COMPARE_FUNCTION(GE, std::greater_equal)

#undef DEFINE_2D_COMPARE_FUNCTION

DELEGATE_2D_BROADCAST_BINARY_FUNCTION(bool, bool, And, std::logical_and)
DELEGATE_2D_BROADCAST_BINARY_FUNCTION(bool, bool, Or, std::logical_or)
DELEGATE_2D_BROADCAST_BINARY_FUNCTION(bool, bool, Xor, std::bit_xor)

#define DEFINE_2D_BROADCAST_BITWISE_BINARY_FUNCTION(Func, Op)                 \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(bool, bool, Func, Op)                 \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(std::int32_t, std::int32_t, Func, Op) \
  DELEGATE_2D_BROADCAST_BINARY_FUNCTION(std::int64_t, std::int64_t, Func, Op)

DEFINE_2D_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseAnd, std::bit_and)
DEFINE_2D_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseOr, std::bit_or)
DEFINE_2D_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseXor, std::bit_xor)

#undef DEFINE_2D_BROADCAST_BITWISE_BINARY_FUNCTION

#undef DELEGATE_2D_BROADCAST_BINARY_FUNCTION

#define DEFINE_2D_BROADCAST_1ST_DIV_FUNCTION(T)   \
  template <>                                     \
  void RowwiseDiv<T, CPUContext, true>(           \
      const int rows,                             \
      const int cols,                             \
      const T* A,                                 \
      const T* B,                                 \
      T* C,                                       \
      CPUContext*) {                              \
    RowwiseBinaryOp<T, T, std::divides<T>, true>( \
        rows, cols, std::divides<T>(), A, B, C);  \
  }                                               \
  template <>                                     \
  void ColwiseDiv<T, CPUContext, true>(           \
      const int rows,                             \
      const int cols,                             \
      const T* A,                                 \
      const T* B,                                 \
      T* C,                                       \
      CPUContext*) {                              \
    ColwiseBinaryOp<T, T, std::divides<T>, true>( \
        rows, cols, std::divides<T>(), A, B, C);  \
  }
DEFINE_2D_BROADCAST_1ST_DIV_FUNCTION(std::int32_t)
DEFINE_2D_BROADCAST_1ST_DIV_FUNCTION(std::int64_t)
#undef DEFINE_2D_BROADCAST_1ST_DIV_FUNCTION

#define DELEGATE_BROADCAST_BINARY_FUNCTION(TIn, TOut, Func, Op) \
  template <>                                                   \
  void Func<TIn, CPUContext>(                                   \
      const int A_ndim,                                         \
      const int* A_dims,                                        \
      const int B_ndim,                                         \
      const int* B_dims,                                        \
      const TIn* A,                                             \
      const TIn* B,                                             \
      TOut* C,                                                  \
      CPUContext* context) {                                    \
    const int ndim = std::max(A_ndim, B_ndim);                  \
    std::vector<int> A_dims_array(ndim);                        \
    std::vector<int> B_dims_array(ndim);                        \
    std::vector<int> C_dims_array(ndim);                        \
    utils::ComputeBroadcastBinaryOpDims(                        \
        A_ndim,                                                 \
        A_dims,                                                 \
        B_ndim,                                                 \
        B_dims,                                                 \
        A_dims_array.data(),                                    \
        B_dims_array.data(),                                    \
        C_dims_array.data());                                   \
    if (A_dims_array == B_dims_array) {                         \
      const int size = std::accumulate(                         \
          C_dims_array.cbegin(),                                \
          C_dims_array.cend(),                                  \
          1,                                                    \
          std::multiplies<int>());                              \
      Func<TIn, CPUContext>(size, A, B, C, context);            \
      return;                                                   \
    }                                                           \
    int pivot;                                                  \
    bool broadcast_1st;                                         \
    if (utils::IsRowwiseBroadcastBinaryOp(                      \
            ndim,                                               \
            A_dims_array.data(),                                \
            B_dims_array.data(),                                \
            &pivot,                                             \
            &broadcast_1st)) {                                  \
      BinaryOpWith2DBroadcasting(                               \
          ndim,                                                 \
          C_dims_array.data(),                                  \
          pivot,                                                \
          broadcast_1st,                                        \
          Rowwise##Func<TIn, CPUContext, true>,                 \
          Rowwise##Func<TIn, CPUContext, false>,                \
          A,                                                    \
          B,                                                    \
          C,                                                    \
          context);                                             \
      return;                                                   \
    }                                                           \
    if (utils::IsColwiseBroadcastBinaryOp(                      \
            ndim,                                               \
            A_dims_array.data(),                                \
            B_dims_array.data(),                                \
            &pivot,                                             \
            &broadcast_1st)) {                                  \
      BinaryOpWith2DBroadcasting(                               \
          ndim,                                                 \
          C_dims_array.data(),                                  \
          pivot,                                                \
          broadcast_1st,                                        \
          Colwise##Func<TIn, CPUContext, true>,                 \
          Colwise##Func<TIn, CPUContext, false>,                \
          A,                                                    \
          B,                                                    \
          C,                                                    \
          context);                                             \
      return;                                                   \
    }                                                           \
    BroadcastBinaryOpImpl(                                      \
        ndim,                                                   \
        A_dims_array.data(),                                    \
        B_dims_array.data(),                                    \
        C_dims_array.data(),                                    \
        Op<TIn>(),                                              \
        A,                                                      \
        B,                                                      \
        C);                                                     \
  }

#define DEFINE_BROADCAST_COMPARE_FUNCTION(Func, Op)                \
  DELEGATE_BROADCAST_BINARY_FUNCTION(float, bool, Func, Op)        \
  DELEGATE_BROADCAST_BINARY_FUNCTION(double, bool, Func, Op)       \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int32_t, bool, Func, Op) \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int64_t, bool, Func, Op) \
  DELEGATE_BROADCAST_BINARY_FUNCTION(bool, bool, Func, Op)

DEFINE_BROADCAST_COMPARE_FUNCTION(EQ, std::equal_to)
DEFINE_BROADCAST_COMPARE_FUNCTION(NE, std::not_equal_to)
DEFINE_BROADCAST_COMPARE_FUNCTION(LT, std::less)
DEFINE_BROADCAST_COMPARE_FUNCTION(LE, std::less_equal)
DEFINE_BROADCAST_COMPARE_FUNCTION(GT, std::greater)
DEFINE_BROADCAST_COMPARE_FUNCTION(GE, std::greater_equal)

#undef DEFINE_BROADCAST_COMPARE_FUNCTION

#define DEFINE_BROADCAST_BINARY_FUNCTION(Func, Op)                         \
  DELEGATE_BROADCAST_BINARY_FUNCTION(float, float, Func, Op)               \
  DELEGATE_BROADCAST_BINARY_FUNCTION(double, double, Func, Op)             \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int32_t, std::int32_t, Func, Op) \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int64_t, std::int64_t, Func, Op)

DEFINE_BROADCAST_BINARY_FUNCTION(Add, std::plus)
DEFINE_BROADCAST_BINARY_FUNCTION(Sub, std::minus)
DEFINE_BROADCAST_BINARY_FUNCTION(Mul, std::multiplies)
DEFINE_BROADCAST_BINARY_FUNCTION(Div, std::divides)

#undef DEFINE_BROADCAST_BINARY_FUNCTION

DELEGATE_BROADCAST_BINARY_FUNCTION(bool, bool, And, std::logical_and)
DELEGATE_BROADCAST_BINARY_FUNCTION(bool, bool, Or, std::logical_or)
DELEGATE_BROADCAST_BINARY_FUNCTION(bool, bool, Xor, std::bit_xor)

#define DEFINE_BROADCAST_BITWISE_BINARY_FUNCTION(Func, Op)                 \
  DELEGATE_BROADCAST_BINARY_FUNCTION(bool, bool, Func, Op)                 \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int32_t, std::int32_t, Func, Op) \
  DELEGATE_BROADCAST_BINARY_FUNCTION(std::int64_t, std::int64_t, Func, Op)

DEFINE_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseAnd, std::bit_and)
DEFINE_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseOr, std::bit_or)
DEFINE_BROADCAST_BITWISE_BINARY_FUNCTION(BitwiseXor, std::bit_xor)

#undef DEFINE_BITWISE_BROADCAST_BINARY_FUNCTION

#undef DELEGATE_BROADCAST_BINARY_FUNCTION

#define CAFFE2_RAND_UNIFORM_REAL(T)                                      \
  template <>                                                            \
  void RandUniform<T, CPUContext>(                                       \
      const size_t n, const T a, const T b, T* r, CPUContext* context) { \
    std::uniform_real_distribution<T> distribution(a, b);                \
    for (size_t i = 0; i < n; ++i) {                                     \
      r[i] = distribution(context->RandGenerator());                     \
    }                                                                    \
  }
CAFFE2_RAND_UNIFORM_REAL(float);
CAFFE2_RAND_UNIFORM_REAL(double);
#undef CAFFE2_RAND_UNIFORM_REAL

#define CAFFE2_RAND_UNIFORM_CHAR(T)                                        \
  template <>                                                              \
  void RandUniform<T, CPUContext>(                                         \
      const size_t n, const T a, const T b, T* r, CPUContext* context) {   \
    std::uniform_int_distribution<short> distribution((short)a, (short)b); \
    for (size_t i = 0; i < n; ++i) {                                       \
      r[i] = static_cast<T>(distribution(context->RandGenerator()));       \
    }                                                                      \
  }
CAFFE2_RAND_UNIFORM_CHAR(int8_t);
CAFFE2_RAND_UNIFORM_CHAR(uint8_t);
#undef CAFFE2_RAND_UNIFORM_CHAR

#define CAFFE2_RAND_UNIFORM_INT(T)                                       \
  template <>                                                            \
  void RandUniform<T, CPUContext>(                                       \
      const size_t n, const T a, const T b, T* r, CPUContext* context) { \
    std::uniform_int_distribution<T> distribution(a, b);                 \
    for (size_t i = 0; i < n; ++i) {                                     \
      r[i] = distribution(context->RandGenerator());                     \
    }                                                                    \
  }

CAFFE2_RAND_UNIFORM_INT(int16_t);
CAFFE2_RAND_UNIFORM_INT(int32_t);
CAFFE2_RAND_UNIFORM_INT(int64_t);
CAFFE2_RAND_UNIFORM_INT(uint16_t);
CAFFE2_RAND_UNIFORM_INT(uint32_t);
CAFFE2_RAND_UNIFORM_INT(uint64_t);
#undef CAFFE2_RAND_UNIFORM_INT

// This is not uniformly distributed between a and b.
// It takes advantage of normal distribution to generate numbers
// with mean = sum / n.
// Ideally the algorithm should be generating n numbers between 0 and 1,
// sum them up as scaled_sum, and use sum / scaled_sum to adjust the values
// to between a and b.
// The algorithm is non-trivial given the adjustment would be different towards
// each value.
#define CAFFE2_RAND_FIXED_SUM(T)                                        \
  template <>                                                           \
  void RandFixedSum<T, CPUContext>(                                     \
      const size_t n,                                                   \
      const T a,                                                        \
      const T b,                                                        \
      const T sum,                                                      \
      T* r,                                                             \
      CPUContext* context) {                                            \
    CAFFE_ENFORCE_GE(a, 0);                                             \
    CAFFE_ENFORCE_GE(sum / (double)n, a);                               \
    CAFFE_ENFORCE_LE(sum / (double)n, b);                               \
    T current_sum = 0;                                                  \
    for (size_t i = 0; i < n - 1; ++i) {                                \
      auto remaining_numbers = n - 1 - i;                               \
      double mean = (sum - current_sum) / remaining_numbers;            \
      double stdev = std::min(mean - a, b - mean);                      \
      std::normal_distribution<double> distribution{mean, stdev / 4.0}; \
      T value = distribution(context->RandGenerator());                 \
      auto remaining_sum = sum - current_sum - value;                   \
      if (value < a || remaining_sum > b * remaining_numbers) {         \
        value = a;                                                      \
      } else if (value > b || remaining_sum < a * remaining_numbers) {  \
        value = b;                                                      \
      }                                                                 \
      r[i] = value;                                                     \
      CAFFE_ENFORCE(a <= value && value <= b);                          \
      current_sum += value;                                             \
    }                                                                   \
    r[n - 1] = sum - current_sum;                                       \
    CAFFE_ENFORCE(a <= r[n - 1] && r[n - 1] <= b);                      \
  }
CAFFE2_RAND_FIXED_SUM(float);
CAFFE2_RAND_FIXED_SUM(double);
CAFFE2_RAND_FIXED_SUM(int8_t);
CAFFE2_RAND_FIXED_SUM(int16_t);
CAFFE2_RAND_FIXED_SUM(int32_t);
CAFFE2_RAND_FIXED_SUM(int64_t);
CAFFE2_RAND_FIXED_SUM(uint8_t);
CAFFE2_RAND_FIXED_SUM(uint16_t);
CAFFE2_RAND_FIXED_SUM(uint32_t);
CAFFE2_RAND_FIXED_SUM(uint64_t);
#undef CAFFE2_RAND_FIXED_SUM

#define CAFFE2_SPECIALIZED_RAND_UNIFORM_UNIQUE(T)                      \
  template <>                                                          \
  void RandUniformUnique<T, CPUContext>(                               \
      const size_t n,                                                  \
      const T a,                                                       \
      const T b,                                                       \
      T* r,                                                            \
      const size_t m,                                                  \
      const T* avoid,                                                  \
      CPUContext* context) {                                           \
    CAFFE_ENFORCE_LE(                                                  \
        n, b - a - m + 1, "Cannot satisfy the unique requirement");    \
    std::unordered_set<T> avoid_set(n);                                \
    if (m) {                                                           \
      avoid_set.insert(avoid, avoid + m);                              \
      CAFFE_ENFORCE_EQ(m, avoid_set.size(), "Avoid should be unique"); \
    }                                                                  \
    std::uniform_int_distribution<T> distribution(a, b);               \
    T v = 0;                                                           \
    for (size_t i = 0; i < n; ++i) {                                   \
      do {                                                             \
        v = distribution(context->RandGenerator());                    \
      } while (avoid_set.count(v));                                    \
      r[i] = v;                                                        \
      avoid_set.insert(v);                                             \
    }                                                                  \
  }

CAFFE2_SPECIALIZED_RAND_UNIFORM_UNIQUE(int32_t);
CAFFE2_SPECIALIZED_RAND_UNIFORM_UNIQUE(int64_t);
#undef CAFFE2_SPECIALIZED_RAND_UNIFORM_UNIQUE

template <>
void RandGaussian<float, CPUContext>(
    const size_t n,
    const float mean,
    const float std,
    float* r,
    CPUContext* context) {
  std::normal_distribution<float> distribution(mean, std);
  for (size_t i = 0; i < n; ++i) {
    r[i] = distribution(context->RandGenerator());
  }
}

#define CAFFE2_SPECIALIZED_SUM(T)            \
  template <>                                \
  void Sum<T, CPUContext>(                   \
      const int N,                           \
      const T* x,                            \
      T* y,                                  \
      CPUContext* /* unused */,              \
      Tensor<CPUContext>* /* unused */) {    \
    *y = ConstEigenVectorMap<T>(x, N).sum(); \
  }

CAFFE2_SPECIALIZED_SUM(float);
CAFFE2_SPECIALIZED_SUM(int32_t);
CAFFE2_SPECIALIZED_SUM(int64_t);

#undef CAFFE2_SPECIALIZED_SUM

template <>
void SumSqr<float, CPUContext>(
    const int N,
    const float* x,
    float* y,
    CPUContext* /*context*/ /* unused */,
    Tensor<CPUContext>* /*scratch_ptr*/ /* unused */) {
  *y = ConstEigenVectorMap<float>(x, N).squaredNorm();
}

template <>
void Select<float, CPUContext>(
    const int N,
    const int D,
    const float* x,
    const int* idx,
    float* y,
    CPUContext* /*context*/) {
  for (int i = 0; i < N; ++i) {
    DCHECK_LT(idx[i], D);
    y[i] = x[i * D + idx[i]];
  }
}

namespace {

template <typename T, bool kCol2Im>
void Im2ColNdNCHWImpl(
    const int N,
    const int img_size,
    const int col_size,
    const int* img_shape,
    const int* col_shape,
    const int* kernel_shape,
    const int* stride,
    const int* dilation,
    const int* pad,
    const float* X_data,
    float* Y_data,
    CPUContext* context) {
  if (kCol2Im) {
    Set<T, CPUContext>(img_size, 0, Y_data, context);
  }
  const int outer_size = col_shape[0];
  const int inner_size = col_size / outer_size;
  const int kernel_size = std::accumulate(
      kernel_shape, kernel_shape + N, 1, std::multiplies<int>());
  std::vector<int> d_offset(N, 0);
  std::vector<int> d_iter(N, 0);
  for (int i = 0; i < outer_size; ++i) {
    // Loop over spatial axes in reverse order to compute a per-axis offset.
    int offset = i;
    for (int d_i = N - 1; d_i >= 0; --d_i) {
      d_offset[d_i] = offset % kernel_shape[d_i];
      offset /= kernel_shape[d_i];
    }
    for (int j = 0; j < inner_size; ++j) {
      // Loop over spatial axes in forward order to compute the indices in the
      // image and column, and whether the index lies in the padding.
      const int col_index = i * inner_size + j;
      int img_index = i / kernel_size;
      bool is_padding = false;
      for (int d_i = 0; d_i < N; ++d_i) {
        const int d_img = d_iter[d_i] * stride[d_i] - pad[d_i] +
            d_offset[d_i] * dilation[d_i];
        is_padding |= d_img < 0 || d_img >= img_shape[d_i + 1];
        img_index = img_index * img_shape[d_i + 1] + d_img;
      }
      if (!kCol2Im) {
        Y_data[col_index] = is_padding ? 0 : X_data[img_index];
      } else if (!is_padding) {
        Y_data[img_index] += X_data[col_index];
      }
      utils::IncreaseIndexInDims(N, col_shape + 1, d_iter.data());
    }
  }
}

} // namespace

template <>
void Im2ColNd<float, CPUContext, StorageOrder::NCHW>(
    const int N,
    const int img_size,
    const int col_size,
    const int* img_shape,
    const int* col_shape,
    const int* kernel_shape,
    const int* stride,
    const int* dilation,
    const int* pad,
    const float* img_data,
    float* col_data,
    CPUContext* context) {
  Im2ColNdNCHWImpl<float, false>(
      N,
      img_size,
      col_size,
      img_shape,
      col_shape,
      kernel_shape,
      stride,
      dilation,
      pad,
      img_data,
      col_data,
      context);
}

template <>
void Col2ImNd<float, CPUContext, StorageOrder::NCHW>(
    const int N,
    const int img_size,
    const int col_size,
    const int* img_shape,
    const int* col_shape,
    const int* kernel_shape,
    const int* stride,
    const int* dilation,
    const int* pad,
    const float* col_data,
    float* img_data,
    CPUContext* context) {
  Im2ColNdNCHWImpl<float, true>(
      N,
      img_size,
      col_size,
      img_shape,
      col_shape,
      kernel_shape,
      stride,
      dilation,
      pad,
      col_data,
      img_data,
      context);
}

template <>
void Im2Col<float, CPUContext, StorageOrder::NCHW>(
    const int channels,
    const int height,
    const int width,
    const int kernel_h,
    const int kernel_w,
    const int dilation_h,
    const int dilation_w,
    const int pad_t,
    const int pad_l,
    const int pad_b,
    const int pad_r,
    const int stride_h,
    const int stride_w,
    const float* img_data,
    float* col_data,
    CPUContext* /*context*/) {
  const int output_h =
      (height + pad_b + pad_t - (dilation_h * (kernel_h - 1) + 1)) / stride_h +
      1;
  const int output_w =
      (width + pad_l + pad_r - (dilation_w * (kernel_w - 1) + 1)) / stride_w +
      1;

  // Fast path for zero padding and no dilation
  // From Torch, THNN_(unfolded_copy)
  if (dilation_h == 1 && dilation_w == 1 && pad_l == 0 && pad_r == 0 &&
      pad_t == 0 && pad_b == 0) {
    for (auto k = 0; k < channels * kernel_h * kernel_w; k++) {
      const auto nip = k / (kernel_h * kernel_w);
      const auto rest = k % (kernel_h * kernel_w);
      const auto kh = rest / kernel_w;
      const auto kw = rest % kernel_w;
      auto* dst = col_data + nip * (kernel_h * kernel_w * output_h * output_w) +
          kh * (kernel_w * output_h * output_w) + kw * (output_h * output_w);
      const auto* src = img_data + nip * (height * width);
      for (auto y = 0; y < output_h; y++) {
        const auto iy = y * stride_h + kh;
        const auto ix = kw;
        if (stride_w == 1) {
          memcpy(
              dst + (y * output_w),
              src + (iy * width + ix),
              sizeof(float) * output_w);
        } else {
          for (auto x = 0; x < output_w; x++) {
            memcpy(
                dst + (y * output_w + x),
                src + (iy * width + ix + x * stride_w),
                sizeof(float));
          }
        }
      }
    }
    return;
  }

  // Fast path for equal padding
  if (pad_l == pad_r && pad_t == pad_b) {
    // From Intel, https://github.com/BVLC/caffe/pull/3536
    const int pad_h = pad_t;
    const int pad_w = pad_l;
    const int channel_size = height * width;
    for (int channel = channels; channel--; img_data += channel_size) {
      for (int kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
        for (int kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
          int input_row = -pad_h + kernel_row * dilation_h;
          for (int output_rows = output_h; output_rows; output_rows--) {
            if (!is_a_ge_zero_and_a_lt_b(input_row, height)) {
              for (int output_cols = output_w; output_cols; output_cols--) {
                *(col_data++) = 0;
              }
            } else {
              int input_col = -pad_w + kernel_col * dilation_w;
              for (int output_col = output_w; output_col; output_col--) {
                if (is_a_ge_zero_and_a_lt_b(input_col, width)) {
                  *(col_data++) = img_data[input_row * width + input_col];
                } else {
                  *(col_data++) = 0;
                }
                input_col += stride_w;
              }
            }
            input_row += stride_h;
          }
        }
      }
    }
    return;
  }

  // Baseline
  const int dkernel_h = dilation_h * (kernel_h - 1) + 1;
  const int dkernel_w = dilation_w * (kernel_w - 1) + 1;

  int height_col = (height + pad_t + pad_b - dkernel_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - dkernel_w) / stride_w + 1;

  int channels_col = channels * kernel_h * kernel_w;
  for (int c = 0; c < channels_col; ++c) {
    int w_offset = c % kernel_w;
    int h_offset = (c / kernel_w) % kernel_h;
    int c_im = c / kernel_h / kernel_w;
    for (int h = 0; h < height_col; ++h) {
      for (int w = 0; w < width_col; ++w) {
        int h_pad = h * stride_h - pad_t + h_offset * dilation_h;
        int w_pad = w * stride_w - pad_l + w_offset * dilation_w;
        if (h_pad >= 0 && h_pad < height && w_pad >= 0 && w_pad < width) {
          col_data[(c * height_col + h) * width_col + w] =
              img_data[(c_im * height + h_pad) * width + w_pad];
        } else {
          col_data[(c * height_col + h) * width_col + w] = 0;
        }
      }
    }
  }
}

template <>
void Im2Col<float, CPUContext, StorageOrder::NHWC>(
    const int channels,
    const int height,
    const int width,
    const int kernel_h,
    const int kernel_w,
    const int dilation_h,
    const int dilation_w,
    const int pad_t,
    const int pad_l,
    const int pad_b,
    const int pad_r,
    const int stride_h,
    const int stride_w,
    const float* img_data,
    float* col_data,
    CPUContext* /*context*/) {
  const int dkernel_h = dilation_h * (kernel_h - 1) + 1;
  const int dkernel_w = dilation_w * (kernel_w - 1) + 1;

  int height_col = (height + pad_t + pad_b - dkernel_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - dkernel_w) / stride_w + 1;

  int h_pad = -pad_t;
  for (int h = 0; h < height_col; ++h) {
    int w_pad = -pad_l;
    for (int w = 0; w < width_col; ++w) {
      for (int ih = h_pad; ih < h_pad + dkernel_h; ih += dilation_h) {
        for (int iw = w_pad; iw < w_pad + dkernel_w; iw += dilation_w) {
          if (ih >= 0 && ih < height && iw >= 0 && iw < width) {
            memcpy(
                col_data,
                img_data + (ih * width + iw) * channels,
                sizeof(float) * channels);
          } else {
            // This should be simply padded with zero.
            memset(col_data, 0, sizeof(float) * channels);
          }
          col_data += channels;
        }
      }
      w_pad += stride_w;
    }
    h_pad += stride_h;
  }
}

template <>
void Col2Im<float, CPUContext, StorageOrder::NCHW>(
    const int channels,
    const int height,
    const int width,
    const int kernel_h,
    const int kernel_w,
    const int dilation_h,
    const int dilation_w,
    const int pad_t,
    const int pad_l,
    const int pad_b,
    const int pad_r,
    const int stride_h,
    const int stride_w,
    const float* col_data,
    float* img_data,
    CPUContext* context) {
  const int output_h =
      (height + pad_b + pad_t - (dilation_h * (kernel_h - 1) + 1)) / stride_h +
      1;
  const int output_w =
      (width + pad_l + pad_r - (dilation_w * (kernel_w - 1) + 1)) / stride_w +
      1;

  Set<float, CPUContext>(height * width * channels, 0, img_data, context);

  // Fast path for zero padding and no dilation
  // From Torch, modified THNN_(unfolded_acc)
  if (dilation_h == 1 && dilation_w == 1 && pad_l == 0 && pad_r == 0 &&
      pad_t == 0 && pad_b == 0) {
    for (auto k = 0; k < channels * kernel_h * kernel_w; k++) {
      const auto nip = k / (kernel_h * kernel_w);
      const auto rest = k % (kernel_h * kernel_w);
      const auto kh = rest / kernel_w;
      const auto kw = rest % kernel_w;
      const auto* dst = col_data +
          nip * (kernel_h * kernel_w * output_h * output_w) +
          kh * (kernel_w * output_h * output_w) + kw * (output_h * output_w);
      auto* src = img_data + nip * (height * width);
      for (auto y = 0; y < output_h; y++) {
        const auto iy = y * stride_h + kh;
        const auto ix = kw;
        if (stride_w == 1) {
          auto offsrc = src + (iy * width + ix);
          const auto offdst = dst + (y * output_w);
          for (auto i = 0; i < output_w; ++i) {
            offsrc[i] += offdst[i];
          }
        } else {
          for (auto x = 0; x < output_w; x++) {
            auto offsrc = src + (iy * width + ix + x * stride_w);
            const auto offdst = dst + (y * output_w + x);
            *offsrc += *offdst;
          }
        }
      }
    }
    return;
  }

  // Fast path for equal padding
  if (pad_l == pad_r && pad_t == pad_b) {
    // From Intel, https://github.com/BVLC/caffe/pull/3536
    const int pad_h = pad_t;
    const int pad_w = pad_l;
    const int channel_size = height * width;
    for (int channel = channels; channel--; img_data += channel_size) {
      for (int kernel_row = 0; kernel_row < kernel_h; kernel_row++) {
        for (int kernel_col = 0; kernel_col < kernel_w; kernel_col++) {
          int input_row = -pad_h + kernel_row * dilation_h;
          for (int output_rows = output_h; output_rows; output_rows--) {
            if (!is_a_ge_zero_and_a_lt_b(input_row, height)) {
              col_data += output_w;
            } else {
              int input_col = -pad_w + kernel_col * dilation_w;
              for (int output_col = output_w; output_col; output_col--) {
                if (is_a_ge_zero_and_a_lt_b(input_col, width)) {
                  img_data[input_row * width + input_col] += *col_data;
                }
                ++col_data;
                input_col += stride_w;
              }
            }
            input_row += stride_h;
          }
        }
      }
    }
    return;
  }

  // Fallback
  const int dkernel_h = dilation_h * (kernel_h - 1) + 1;
  const int dkernel_w = dilation_w * (kernel_w - 1) + 1;

  int height_col = (height + pad_t + pad_b - dkernel_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - dkernel_w) / stride_w + 1;
  int channels_col = channels * kernel_h * kernel_w;
  for (int c = 0; c < channels_col; ++c) {
    int w_offset = c % kernel_w;
    int h_offset = (c / kernel_w) % kernel_h;
    int c_im = c / kernel_h / kernel_w;
    for (int h = 0; h < height_col; ++h) {
      for (int w = 0; w < width_col; ++w) {
        int h_pad = h * stride_h - pad_t + h_offset * dilation_h;
        int w_pad = w * stride_w - pad_l + w_offset * dilation_w;
        if (h_pad >= 0 && h_pad < height && w_pad >= 0 && w_pad < width) {
          img_data[(c_im * height + h_pad) * width + w_pad] +=
              col_data[(c * height_col + h) * width_col + w];
        }
      }
    }
  }
}

template <>
void Col2Im<float, CPUContext, StorageOrder::NHWC>(
    const int channels,
    const int height,
    const int width,
    const int kernel_h,
    const int kernel_w,
    const int dilation_h,
    const int dilation_w,
    const int pad_t,
    const int pad_l,
    const int pad_b,
    const int pad_r,
    const int stride_h,
    const int stride_w,
    const float* col_data,
    float* img_data,
    CPUContext* context) {
  const int dkernel_h = dilation_h * (kernel_h - 1) + 1;
  const int dkernel_w = dilation_w * (kernel_w - 1) + 1;

  Set<float, CPUContext>(height * width * channels, 0, img_data, context);
  int height_col = (height + pad_t + pad_b - dkernel_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - dkernel_w) / stride_w + 1;
  int h_pad = -pad_t;
  for (int h = 0; h < height_col; ++h) {
    int w_pad = -pad_l;
    for (int w = 0; w < width_col; ++w) {
      for (int ih = h_pad; ih < h_pad + dkernel_h; ih += dilation_h) {
        for (int iw = w_pad; iw < w_pad + dkernel_w; iw += dilation_w) {
          if (ih >= 0 && ih < height && iw >= 0 && iw < width) {
            auto* img_data_patch = img_data + (ih * width + iw) * channels;
            Add<float, CPUContext>(
                channels, img_data_patch, col_data, img_data_patch, context);
          }
          col_data += channels;
        }
      }
      w_pad += stride_w;
    }
    h_pad += stride_h;
  }
}

template <>
void BiasCHW<float, CPUContext>(
    const float* bias,
    const float* /*bias_multiplier*/,
    const int bias_channels,
    const int image_size,
    float* image,
    CPUContext* /*context*/) {
  // Sum the per-channel bias into every image plane
  for (int c = 0; c < bias_channels; ++c) {
    float b = bias[c];

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
    float32x4_t vBias = vdupq_n_f32(b);

    // We give alignment hints for additional speed, so handle the
    // non-vectorizable prologue separately
    constexpr int kVecSizeInFloat = sizeof(float32x4_t) / sizeof(float);

    // FIXME: if input < kVecSizeInFloat, can't vectorize at all

    int prologue = kVecSizeInFloat -
        // remainder in floats
        (((uintptr_t)image) % (sizeof(float32x4_t))) / sizeof(float);

    int i = 0;
    // Prologue loop
    for (; i < prologue; ++i) {
      image[i] += b;
    }

    // The loop is manually unrolled by 8
    constexpr int kUnroll = 8;
    constexpr int kFloatsPerLoop = kUnroll * kVecSizeInFloat;

    int remainder = image_size - prologue;
    int vectorizable = prologue + (remainder / kFloatsPerLoop) * kFloatsPerLoop;

    // Vectorizable body
    for (; i < vectorizable; i += kFloatsPerLoop) {
      // Manually unrolled
      float32x4_t v0 = vld1q_f32_aligned(image + i + 0);
      float32x4_t v1 = vld1q_f32_aligned(image + i + 4);
      float32x4_t v2 = vld1q_f32_aligned(image + i + 8);
      float32x4_t v3 = vld1q_f32_aligned(image + i + 12);
      float32x4_t v4 = vld1q_f32_aligned(image + i + 16);
      float32x4_t v5 = vld1q_f32_aligned(image + i + 20);
      float32x4_t v6 = vld1q_f32_aligned(image + i + 24);
      float32x4_t v7 = vld1q_f32_aligned(image + i + 28);

      v0 = vaddq_f32(v0, vBias);
      v1 = vaddq_f32(v1, vBias);
      v2 = vaddq_f32(v2, vBias);
      v3 = vaddq_f32(v3, vBias);
      v4 = vaddq_f32(v4, vBias);
      v5 = vaddq_f32(v5, vBias);
      v6 = vaddq_f32(v6, vBias);
      v7 = vaddq_f32(v7, vBias);

      vst1q_f32_aligned(image + i + 0, v0);
      vst1q_f32_aligned(image + i + 4, v1);
      vst1q_f32_aligned(image + i + 8, v2);
      vst1q_f32_aligned(image + i + 12, v3);
      vst1q_f32_aligned(image + i + 16, v4);
      vst1q_f32_aligned(image + i + 20, v5);
      vst1q_f32_aligned(image + i + 24, v6);
      vst1q_f32_aligned(image + i + 28, v7);
    }

    // Non-vectorizable epilogue
    for (; i < image_size; ++i) {
      image[i] += b;
    }
#else
    // Non-NEON CPU implementation
    for (int i = 0; i < image_size; ++i) {
      image[i] += b;
    }
#endif // defined(__ARM_NEON__) || defined(__ARM_NEON)

    image += image_size;
  }
}

template <>
void CopyMatrix<CPUContext>(
    const size_t itemsize,
    const int M,
    const int N,
    const void* A,
    const int lda,
    void* B,
    const int ldb,
    CPUContext* /*context*/,
    TypeMeta::TypedCopy copy) {
  if (A == nullptr || B == nullptr) {
    return;
  }
  if (lda == N && ldb == N) {
    // can coalese to a single memcpy of size M * N
    if (copy) {
      copy(static_cast<const char*>(A), static_cast<char*>(B), N * M);
    } else {
      memcpy(
          static_cast<char*>(B), static_cast<const char*>(A), itemsize * N * M);
    }
    return;
  }

  for (int i = 0; i < M; ++i) {
    if (copy) {
      copy(
          static_cast<const char*>(A) + lda * i * itemsize,
          static_cast<char*>(B) + ldb * i * itemsize,
          N);
    } else {
      memcpy(
          static_cast<char*>(B) + ldb * i * itemsize,
          static_cast<const char*>(A) + lda * i * itemsize,
          itemsize * N);
    }
  }
}

#define CAFFE2_SPECIALIZED_COPYVECTOR(T)                            \
  template <>                                                       \
  void CopyVector<T, CPUContext>(                                   \
      const int N, const T* src, T* dst, CPUContext* /*context*/) { \
    if (src != dst && N > 0) {                                      \
      memcpy(dst, src, sizeof(T) * N);                              \
    }                                                               \
  }
CAFFE2_SPECIALIZED_COPYVECTOR(float)
#undef CAFFE2_SPECIALIZED_COPYVECTOR

namespace {

#ifdef CAFFE2_USE_HPTT

bool TryTransposeWithHPTT(
    const int ndim,
    const int* dims,
    const int* axes,
    const float* X,
    float* Y) {
  std::vector<int> axes_cm(ndim);
  std::vector<int> dims_cm(ndim);
  // Convert row-major index to column-major.
  const auto cm_fn = [ndim](const int i) { return ndim - i - 1; };
  for (int i = 0; i < ndim; ++i) {
    axes_cm[i] = cm_fn(axes[cm_fn(i)]);
    dims_cm[i] = dims[cm_fn(i)];
  }
  auto plan = hptt::create_plan(
      axes_cm.data(),
      ndim,
      1.0,
      X,
      dims_cm.data(),
      nullptr,
      0.0,
      Y,
      nullptr,
      hptt::ESTIMATE,
      1);
  if (plan == nullptr) {
    return false;
  }
  plan->execute();
  return true;
}

#endif // CAFFE2_USE_HPTT

std::vector<int>
ComputeXStrides(const int ndim, const int* dims, const int* axes) {
  std::vector<int> x_strides(ndim);
  std::vector<int> buff(ndim);
  int cur_stride = 1;
  for (int i = ndim - 1; i >= 0; --i) {
    buff[i] = cur_stride;
    cur_stride *= dims[i];
  }
  for (int i = 0; i < ndim; ++i) {
    x_strides[i] = buff[axes[i]];
  }
  return x_strides;
}

template <typename T>
void TransposeCPUImpl(
    const int ndim,
    const int* dims,
    const int* axes,
    const T* X,
    T* Y) {
  std::vector<int> Y_dims(ndim);
  for (int i = 0; i < ndim; ++i) {
    Y_dims[i] = dims[axes[i]];
  }
  // Measure amount of contiguous data we can copy at once
  int block_size = 1;
  int num_shared_idx = 0;
  for (int i = ndim - 1; i >= 0 && axes[i] == i; --i) {
    block_size *= Y_dims[i];
    ++num_shared_idx;
  }
  const int itr_axes = ndim - num_shared_idx;
  const int num_blocks = std::accumulate(
      Y_dims.cbegin(), Y_dims.cbegin() + itr_axes, 1, std::multiplies<int>());
  if (ndim < 2 || itr_axes == 0) {
    std::memcpy(Y, X, num_blocks * block_size * sizeof(T));
    return;
  }
  const std::vector<int> X_strides = ComputeXStrides(itr_axes, dims, axes);
  std::vector<int> index(itr_axes, 0);
  for (int Y_index = 0; Y_index < num_blocks; ++Y_index) {
    const int X_index = std::inner_product(
        X_strides.cbegin(), X_strides.cend(), index.cbegin(), 0);
    if (block_size == 1) {
      Y[Y_index] = X[X_index];
    } else {
      std::memcpy(
          Y + block_size * Y_index,
          X + block_size * X_index,
          block_size * sizeof(T));
    }
    utils::IncreaseIndexInDims(itr_axes, Y_dims.data(), index.data());
  }
}

} // namespace

template <>
void Transpose<float, CPUContext>(
    const int ndim,
    const int* dims,
    const int* axes,
    const float* X,
    float* Y,
    CPUContext* /* context */) {
#ifdef CAFFE2_USE_HPTT
  if (TryTransposeWithHPTT(ndim, dims, axes, X, Y)) {
    return;
  }
#endif // CAFFE2_USE_HPTT
  TransposeCPUImpl(ndim, dims, axes, X, Y);
}

#define CAFFE2_SPECIALIZED_TRANSPOSE(T)       \
  template <>                                 \
  void Transpose<T, CPUContext>(              \
      const int ndim,                         \
      const int* dims,                        \
      const int* axes,                        \
      const T* X,                             \
      T* Y,                                   \
      CPUContext* /* context */) {            \
    TransposeCPUImpl(ndim, dims, axes, X, Y); \
  }
CAFFE2_SPECIALIZED_TRANSPOSE(double)
CAFFE2_SPECIALIZED_TRANSPOSE(int)
CAFFE2_SPECIALIZED_TRANSPOSE(TIndex)
#ifdef CAFFE2_UNIQUE_LONG_TYPEMETA
CAFFE2_SPECIALIZED_TRANSPOSE(long)
#endif
CAFFE2_SPECIALIZED_TRANSPOSE(std::uint8_t)
CAFFE2_SPECIALIZED_TRANSPOSE(std::uint16_t)
#undef CAFFE2_SPECIALIZED_TRANSPOSE

} // namespace math
} // namespace caffe2

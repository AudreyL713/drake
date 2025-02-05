#include "drake/math/roll_pitch_yaw.h"

#include <cmath>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "drake/common/drake_assert.h"
#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/math/rotation_matrix.h"

namespace drake {
namespace math {
namespace {

using Eigen::Vector3d;
using Eigen::Matrix3d;
using symbolic::Expression;
using symbolic::Variable;

const double kEpsilon = std::numeric_limits<double>::epsilon();

// This data structure is used in symbolic code. If a `Matrix3` overload is
// added to the constructors, then it creates an ambiguous overload for types
// like this one (and things like `Eigen::Ref<>`).
using Vector3dUnaligned = Eigen::Matrix<double, 3, 1, Eigen::DontAlign>;

// This tests the RollPitchYaw constructors and IsNearlyEqualTo().
GTEST_TEST(RollPitchYaw, testConstructorsAndIsNearlyEqualTo) {
  const RollPitchYaw<double> a(0.1, 0.2, -0.3);
  const RollPitchYaw<double> b(0.1, 0.2, -0.4);
  const RollPitchYaw<double> c(Vector3d(0.1, 0.2, -0.3));
  EXPECT_FALSE(a.IsNearlyEqualTo(b, kEpsilon));
  EXPECT_TRUE(a.IsNearlyEqualTo(c, kEpsilon));
  EXPECT_FALSE(a.IsNearlyEqualTo(b, 0.1 - 10*kEpsilon));
  EXPECT_TRUE(a.IsNearlyEqualTo(b, 0.1 + 10*kEpsilon));

  // Test additional constructors.
  const RotationMatrix<double> R = a.ToRotationMatrix();
  const RollPitchYaw<double> d(R);
  EXPECT_TRUE(a.IsNearlyEqualTo(d, kEpsilon));
  const RollPitchYaw<double> e(R.ToQuaternion());
  EXPECT_TRUE(a.IsNearlyEqualTo(e, kEpsilon));
  const RollPitchYaw<double> f(Vector3dUnaligned(0.1, 0.2, -0.3));
  EXPECT_TRUE(a.IsNearlyEqualTo(f, kEpsilon));
}

// Test typedef (using) RollPitchYawd.
GTEST_TEST(RollPitchYaw, RollPitchYawd) {
  const RollPitchYaw<double> rpy(0.1, 0.2, 0.3);
  const RollPitchYawd rpy_double(0.1, 0.2, 0.3);
  EXPECT_TRUE(CompareMatrices(rpy.vector(), rpy_double.vector(), kEpsilon));
}

// This tests the RollPitchYaw access methods.
GTEST_TEST(RollPitchYaw, testAccessSetGetMethods) {
  RollPitchYaw<double> rpy(0.12, 0.34, -0.56);
  const Vector3d v = rpy.vector();
  const double roll = rpy.roll_angle();
  const double pitch = rpy.pitch_angle();
  const double yaw = rpy.yaw_angle();
  EXPECT_TRUE(v(0) == roll && roll == 0.12);
  EXPECT_TRUE(v(1) == pitch && pitch == 0.34);
  EXPECT_TRUE(v(2) == yaw && yaw == -0.56);

  // Check 3-argument set method.
  rpy.set(1.1, 2.2, 3.3);
  EXPECT_TRUE(rpy.roll_angle() == 1.1);
  EXPECT_TRUE(rpy.pitch_angle() == 2.2);
  EXPECT_TRUE(rpy.yaw_angle() == 3.3);

  // Check 1-argument set method.
  rpy.set(Vector3d(4.4, 5.5, 6.6));
  EXPECT_TRUE(rpy.roll_angle() == 4.4);
  EXPECT_TRUE(rpy.pitch_angle() == 5.5);
  EXPECT_TRUE(rpy.yaw_angle() == 6.6);

  // Check single angle set methods.
  rpy.set_roll_angle(7.7);
  rpy.set_pitch_angle(8.8);
  rpy.set_yaw_angle(9.9);
  EXPECT_TRUE(rpy.roll_angle() == 7.7);
  EXPECT_TRUE(rpy.pitch_angle() == 8.8);
  EXPECT_TRUE(rpy.yaw_angle() == 9.9);
}

// Test making a rotation matrix and its associated 3x3 matrix from a
// RollPitchYaw rotation sequence.
GTEST_TEST(RollPitchYaw, ToRotationMatrix) {
  const double r(0.5), p(0.4), y(0.3);
  const RollPitchYaw<double> rpy(r, p, y);
  const Matrix3d m_eigen = (Eigen::AngleAxisd(y, Vector3d::UnitZ())
                          * Eigen::AngleAxisd(p, Vector3d::UnitY())
                          * Eigen::AngleAxisd(r, Vector3d::UnitX())).matrix();
  const RotationMatrix<double> R_eigen(m_eigen);
  const RotationMatrix<double> R_rpy = rpy.ToRotationMatrix();
  EXPECT_TRUE(R_rpy.IsNearlyEqualTo(R_eigen, kEpsilon));

  // Also test associated convenience "sugar" method that returns 3x3 matrix.
  const Matrix3d m_rpy = rpy.ToMatrix3ViaRotationMatrix();
  EXPECT_TRUE(CompareMatrices(m_rpy, m_eigen, kEpsilon));
}

// Test whether or not pitch angle is near gimbal lock.
GTEST_TEST(RollPitchYaw, testDoesPitchAngleViolateGimbalLock) {
  RollPitchYaw<double> rpy(-2.1, 0, 5.7);
  EXPECT_FALSE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
  rpy.set(-2.1, M_PI_2, 5.7);
  EXPECT_TRUE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
  rpy.set(M_PI_2, 1, M_PI_2);
  EXPECT_FALSE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
  rpy.set(2.3, -M_PI_2, 4.5);
  EXPECT_TRUE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
  rpy.set(-9 * M_PI_2, 90, M_PI_2);
  EXPECT_FALSE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
  rpy.set(2.3, 91 * M_PI_2, 4.5);
  EXPECT_TRUE(rpy.DoesPitchAngleViolateGimbalLockTolerance());
}

// This tests the RollPitchYaw.ToQuaternion() method.
GTEST_TEST(RollPitchYaw, testToQuaternion) {
  const RollPitchYaw<double> rpy(0.12, 0.34, -0.56);
  const Eigen::Quaterniond quat = rpy.ToQuaternion();
  const RotationMatrix<double> R1(rpy);
  const RotationMatrix<double> R2(quat);
  EXPECT_TRUE(R1.IsNearlyEqualTo(R2, kEpsilon));

  // Test SetFromQuaternion.
  RollPitchYaw<double> rpy2(0, 0, 0);
  rpy2.SetFromQuaternion(quat);
  EXPECT_TRUE(rpy2.IsNearlySameOrientation(rpy, kEpsilon));

  // Test SetFromRotationMatrix.
  rpy2.SetFromRotationMatrix(R1);
  EXPECT_TRUE(rpy2.IsNearlySameOrientation(rpy, kEpsilon));
}

// This tests the RollPitchYaw.IsValid() method.
GTEST_TEST(RollPitchYaw, testIsValid) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  const Vector3d a(0.12, 0.34, -0.56);
  const Vector3d b(0, 0, NAN);
  const Vector3d c(0, 0, kInfinity);
  EXPECT_TRUE(RollPitchYaw<double>::IsValid(a));
  EXPECT_FALSE(RollPitchYaw<double>::IsValid(b));
  EXPECT_FALSE(RollPitchYaw<double>::IsValid(c));
}

// For a rotation matrix R that depends on roll-pitch-yaw angles `rpy`,
// calculate the ordinary derivative of R with respect to t.
GTEST_TEST(RollPitchYaw, OrdinaryDerivativeRotationMatrixRollPitchYaw) {
  const RollPitchYaw<double> rpy(0.2, 0.3, 0.4);
  const Vector3d rpyDt(-2.1, 3.3, 5.7);
  const Matrix3d RDt = rpy.CalcRotationMatrixDt(rpyDt);

  // Results generated by MotionGenesis.
  const double r = rpy.roll_angle();
  const double p = rpy.pitch_angle();
  const double y = rpy.yaw_angle();
  const double c0 = cos(r),  c1 = cos(p),  c2 = cos(y);
  const double s0 = sin(r),  s1 = sin(p),  s2 = sin(y);
  const double rDt = rpyDt(0), pDt = rpyDt(1), yDt = rpyDt(2);
  Matrix3d MDt;
  MDt << -s1*c2*pDt - s2*c1*yDt,
      s0*s2*rDt + s1*c0*c2*rDt + s0*c1*c2*pDt - c0*c2*yDt - s1*s0*s2*yDt,
      s0*c2*yDt + s2*c0*rDt + c1*c0*c2*pDt - s1*s0*c2*rDt - s1*s2*c0*yDt,
      c1*c2*yDt - s1*s2*pDt,
      s1*s0*c2*yDt + s1*s2*c0*rDt + s0*s2*c1*pDt - s0*c2*rDt - s2*c0*yDt,
      s0*s2*yDt + s1*c0*c2*yDt + s2*c1*c0*pDt - c0*c2*rDt - s1*s0*s2*rDt,
      -c1*pDt,
      c1*c0*rDt - s1*s0*pDt,
      -s1*c0*pDt - s0*c1*rDt;

  EXPECT_TRUE(CompareMatrices(RDt, MDt, 16 * kEpsilon,
                              MatrixCompareType::absolute));
}

// For a RollPitchYaw R_AD(rpy) that relates frame D's orientation to frame A,
// calculate conversion from rpy and its time-derivative rpyDt to w_AD_A
// (D's angular velocity in A, expressed in either A or D).
GTEST_TEST(RollPitchYaw, CalcAngularVelocityFromRpyDtAndViceVersa) {
  const RollPitchYaw<double> rpy(0.2, 0.3, 0.4);
  const Vector3d rpyDt(-2.1, 3.3, 5.7);

  // Form w_AD_A (D's angular velocity in A, expressed in "parent" A),
  // and  w_AD_D (D's angular velocity in D, expressed in "child" D).
  // Ensure both parent/child methods give the same angular velocity.
  const Vector3d w_AD_A = rpy.CalcAngularVelocityInParentFromRpyDt(rpyDt);
  const Vector3d w_AD_D = rpy.CalcAngularVelocityInChildFromRpyDt(rpyDt);
  const RotationMatrix<double> R_AD(rpy);
  EXPECT_TRUE(CompareMatrices(w_AD_A, R_AD * w_AD_D,
                              4 * kEpsilon, MatrixCompareType::absolute));

  // Form the matrix Np relating ṙ, ṗ, ẏ to ωx, ωy, ωz, where frame D's angular
  // velocity in A, expressed in "parent" A is `w_AD_A = ωx Ax + ωy Ay + ωz Az`.
  // ⌈ ṙ ⌉      ⌈ ωx ⌉          ⌈ cos(y)/cos(p)  sin(y)/cos(p)   0 ⌉
  // | ṗ | = Np | ωy |     Np = |   −sin(y)          cos(y)      0 |
  // ⌊ ẏ ⌋      ⌊ ωz ⌋ᴀ         ⌊ cos(y)*tan(p)   sin(y)*tan(p)  1 ⌋
  const Matrix3<double> Np =
      rpy.CalcMatrixRelatingRpyDtToAngularVelocityInParent();

  // Form the matrix Nc relating ṙ, ṗ, ẏ to ω0, ω1, ω2, where frame D's angular
  // velocity in A, expressed in "child" D is `w_AD_D = ω0 Dx + ω1 Dy + ω2 Dz`.
  // ⌈ ṙ ⌉      ⌈ ω0 ⌉          ⌈ 1  sin(r)*tan(p)  cos(r)*tan(p) ⌉
  // | ṗ | = Nc | ω1 |     Nc = | 0      cos(r)        −sin(r)    |
  // ⌊ ẏ ⌋      ⌊ ω2 ⌋ᴅ         ⌊ 0  sin(r)/cos(p)  cos(r)/cos(p) ⌋
  const Matrix3<double> Nc =
      rpy.CalcMatrixRelatingRpyDtToAngularVelocityInChild();

  // Verify Np by ensuring Np * Np_inv produces an identity matrix, with Np
  // calculated by a Drake function and Np_inv (the matrix inverse of Np)
  // calculated by-hand/MotionGenesis. Since Np and Np_inv are from different
  // sources, their product being the identity matrix is not a tautology.
  // Similarly for Nc * Nc_inv, where Nc_inv is also available on pg. 428 of
  // Spacecraft Dynamics, by Kane & Levinson, 1983.
  const double r = rpy.roll_angle();
  const double p = rpy.pitch_angle();
  const double y = rpy.yaw_angle();
  using std::cos;
  using std::sin;
  Matrix3<double> Np_inv, Nc_inv;
  // clang-format off
  Np_inv << cos(p) * cos(y),  -sin(y),    0,
            sin(y) * cos(p),   cos(y),    0,
                    -sin(p),        0,    1;
  Nc_inv << 1,        0,  -sin(p),
            0,   cos(r),   sin(r) * cos(p),
            0,  -sin(r),   cos(r) * cos(p);
  // clang-format on
  EXPECT_TRUE(CompareMatrices(Np * Np_inv, Matrix3<double>::Identity(),
                              4 * kEpsilon, MatrixCompareType::absolute));
  EXPECT_TRUE(CompareMatrices(Nc * Nc_inv, Matrix3<double>::Identity(),
                              4 * kEpsilon, MatrixCompareType::absolute));
  const Vector3d w_AD_A_expected = Np_inv * rpyDt;
  const Vector3d w_AD_D_expected = Nc_inv * rpyDt;
  EXPECT_TRUE(CompareMatrices(w_AD_A, w_AD_A_expected, 16 * kEpsilon,
                              MatrixCompareType::absolute));
  EXPECT_TRUE(CompareMatrices(w_AD_D, w_AD_D_expected, 16 * kEpsilon,
                              MatrixCompareType::absolute));

  // Check that CalcMatrixRelatingRpyDtToAngularVelocityInParent() and
  // CalcMatrixRelatingRpyDtToAngularVelocityInChild() throw near gimbal lock.
  const RollPitchYaw<double> rpyA(0.2, M_PI / 2, 0.4);
  const char* expected_message_parentA =
      "RollPitchYaw::CalcMatrixRelatingRpyDtToAngularVelocityInParent()"
      ".*gimbal-lock.*";
  const char* expected_message_childA =
      "RollPitchYaw::CalcMatrixRelatingRpyDtToAngularVelocityInChild()"
      ".*gimbal-lock.*";
  DRAKE_EXPECT_THROWS_MESSAGE(
      rpyA.CalcMatrixRelatingRpyDtToAngularVelocityInParent(),
      expected_message_parentA);
  DRAKE_EXPECT_THROWS_MESSAGE(
      rpyA.CalcMatrixRelatingRpyDtToAngularVelocityInChild(),
      expected_message_childA);

  // Now test the inverse relationships.
  const Vector3d rpyDt_calculated_from_parent =
      rpy.CalcRpyDtFromAngularVelocityInParent(w_AD_A);
  const Vector3d rpyDt_calculated_from_child =
      rpy.CalcRpyDtFromAngularVelocityInChild(w_AD_D);
  EXPECT_TRUE(CompareMatrices(rpyDt_calculated_from_parent, rpyDt,
      16 * kEpsilon, MatrixCompareType::absolute));
  EXPECT_TRUE(CompareMatrices(rpyDt_calculated_from_child, rpyDt,
      16 * kEpsilon, MatrixCompareType::absolute));

  // Check that CalcRpyDtFromAngularVelocityInParent() and
  // CalcRpyDtFromAngularVelocityInChild() throw near gimbal lock.
  const char* expected_message_parentB =
      "RollPitchYaw::CalcRpyDtFromAngularVelocityInParent()"
      ".*gimbal-lock.*";
  const char* expected_message_childB =
      "RollPitchYaw::CalcRpyDtFromAngularVelocityInChild()"
      ".*gimbal-lock.*";
  DRAKE_EXPECT_THROWS_MESSAGE(rpyA.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyA.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);

  const RollPitchYaw<double> rpyB(0.2, -M_PI / 2, 0.4);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyB.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyB.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);

  const RollPitchYaw<double> rpyC(0.2, 3 * M_PI / 2, 0.4);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyC.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyC.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);

  const RollPitchYaw<double> rpyD(0.2, -3 * M_PI / 2, 0.4);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyD.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyD.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);

  const RollPitchYaw<double> rpyE(0.2, 3 * M_PI / 2 + 1E-8, 0.4);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyE.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyE.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);

  const RollPitchYaw<double> rpyF(0.2, -3 * M_PI / 2 + 1E-8, 0.4);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyF.CalcRpyDtFromAngularVelocityInParent(w_AD_A),
                              expected_message_parentB);
  DRAKE_EXPECT_THROWS_MESSAGE(rpyF.CalcRpyDtFromAngularVelocityInChild(w_AD_D),
                              expected_message_childB);
}

// Test accuracy of back-and-forth conversion from angular velocity to rpyDt
// (time-derivative of roll-pitch-yaw angles) and back to angular velocity as a
// way to understand how many digits of precision are lost near gimbal-lock.
GTEST_TEST(RollPitchYaw, PrecisionOfAngularVelocityFromRpyDtAndViceVersa) {
  const Vector3d wA(1, 1, 1);
  const Vector3d alphaA(1, 1, 1);
  const double tolerance =
      RollPitchYaw<double>::GimbalLockPitchAngleTolerance();
  int number_of_precise_cases = 0, number_of_imprecise_cases = 0;
  // Note: The for-loop logic is designed to only test a few imprecises cases.
  for (double i = -2.00; i <= 2.001; i += (-0.9 <= i && i <= 0.8) ? 1 : 0.05) {
    const double difference_from_gimbal_lock = i * tolerance;
    const double pitch_angle = M_PI / 2 + difference_from_gimbal_lock;
    const RollPitchYaw<double> rpy(1, pitch_angle, 1);
    const bool is_imprecise = rpy.DoesPitchAngleViolateGimbalLockTolerance();

    // Calculate rpyDt from angular velocity.
    Vector3d rpyDt, rpyDDt;
    if (is_imprecise) {
      ++number_of_imprecise_cases;
      const char* expected_message =
          "RollPitchYaw::CalcRpyDtFromAngularVelocityInParent()"
          ".*gimbal-lock.*";
      DRAKE_EXPECT_THROWS_MESSAGE(
          rpyDt = rpy.CalcRpyDtFromAngularVelocityInParent(wA),
          expected_message);
      expected_message =
          "RollPitchYaw::CalcRpyDDtFromRpyDtAndAngularAccelInParent()"
          ".*gimbal-lock.*";
      DRAKE_EXPECT_THROWS_MESSAGE(rpyDDt =
        rpy.CalcRpyDDtFromRpyDtAndAngularAccelInParent(rpyDt, alphaA),
        expected_message);
    } else {
      ++number_of_precise_cases;
      rpyDt = rpy.CalcRpyDtFromAngularVelocityInParent(wA);
      rpyDDt = rpy.CalcRpyDDtFromRpyDtAndAngularAccelInParent(rpyDt, alphaA);

      const double max_rpyDt = rpyDt.template lpNorm<Eigen::Infinity>();
      const double max_rpyDDt = rpyDDt.template lpNorm<Eigen::Infinity>();

      // max_rpyDt scales with 1/cos(pitch_angle) multiplied by angular velocity
      // wA = (1, 1, 1).  Check that max_rpyDt has a range that is within
      // a reasonable multiplier (1000) of that scale.
      // max_rpyDDt scales with 1/cos(pitch_angle)² multiplied by angular
      // acceleration alphaA = (1, 1, 1).  Check that max_rpyDDt has a range
      // that is within a reasonable multiplier (1000²) of that scale.
      EXPECT_TRUE(1E-3 / tolerance <= max_rpyDt &&
                  max_rpyDt <= 1E3 / tolerance);
      EXPECT_TRUE(1E-6 / (tolerance * tolerance) <= max_rpyDDt &&
                  max_rpyDDt <= 1E6 / (tolerance * tolerance));

      // Now, reverse procedure by calculating angular velocity from rpyDt.
      const Vector3d wB = rpy.CalcAngularVelocityInParentFromRpyDt(rpyDt);

      // Compare the given and calculated angular velocities.
      const Vector3d w_diff = wB - wA;
      const Vector3d w_error(w_diff(0) / wA(0),
                             w_diff(1) / wA(1),
                             w_diff(2) / wA(2));
      const double max_error = w_diff.template lpNorm<Eigen::Infinity>();

      // Since RollPitchYaw::kGimbalLockToleranceCosPitchAngle = 0.008,
      // we expect max_error <= (2^7 = 128) * kEpsilon ≈ 2.842E-14.
      // This test uses (2^8 = 256) * kEpsilon in the unlikely event that a
      // compiler, operating system, ... inadvertently loses an extra bit.
      // For details, see documentation for kGimbalLockToleranceCosPitchAngle.
      EXPECT_LE(max_error, 256 * kEpsilon);  // Up to 8 bits lost.
    }
  }
  EXPECT_TRUE(number_of_precise_cases > 0  &&  number_of_imprecise_cases > 0);
}

// For a RollPitchYaw rpy that relates orientation of a frame A to a frame D,
// calculate conversion from rpy and its time-derivative rpyDt to w_AD_D
// (D's angular velocity in A, expressed in D).
GTEST_TEST(RollPitchYaw, CalcAngularVelocityInChildFromRpyDt) {
  const RollPitchYaw<double> rpy(0.2, 0.3, 0.4);
  const Vector3d rpyDt(-2.1, 3.3, 5.7);
  const Vector3d w_AD_D = rpy.CalcAngularVelocityInChildFromRpyDt(rpyDt);

  // Results generated by MotionGenesis.
  const double r = rpy.roll_angle();
  const double p = rpy.pitch_angle();
  const double rDt = rpyDt(0), pDt = rpyDt(1), yDt = rpyDt(2);
  using std::cos;
  using std::sin;
  const double wx = rDt - sin(p) * yDt;
  const double wy = cos(r)*pDt + sin(r) * cos(p) *yDt;
  const double wz = cos(p) * cos(r) * yDt - sin(r) * pDt;
  const Vector3d w_AD_D_expected(wx, wy, wz);

  EXPECT_TRUE(CompareMatrices(w_AD_D, w_AD_D_expected, 16 * kEpsilon,
                              MatrixCompareType::absolute));
}

// For a RollPitchYaw rpy that relates orientation of a frame A to a frame D,
// calculate conversion from alpha_AD_A (D's angular acceleration in A,
// expressed in A) to rpyDDt (2nd time-derivative of rpy), i.e., `[r̈, p̈, ÿ]`.
// Cross-validate with `[r̈, p̈, ÿ]` calculated from alpha_AD_D (D's angular
// acceleration in A, expressed in D).
GTEST_TEST(RollPitchYaw, CalcRpyDDtFromAngularAccel) {
  const Vector3d rpyDt(-2.1, 3.3, 5.7);
  const Vector3d alpha_AD_A(0.5, 0.7, 0.9);

  // Set up to test a reasonable range of values.
  // Note: tol helps ensure that each for-loop's end-value is reached.
  const double tol = 64 * kEpsilon * M_PI;
  const double deg = M_PI / 180;
  for (double roll = -M_PI; roll <= M_PI + tol; roll += 10 * deg) {
    for (double pitch = -M_PI/2; pitch <= M_PI/2 + tol; pitch += 2 * deg) {
      for (double yaw = -M_PI; yaw <= M_PI + tol; yaw += 10 * deg) {
        const RollPitchYaw<double> rpy(roll, pitch, yaw);

        // Calculate [r̈, p̈, ÿ] from alpha_AD_A which is
        // D's angular acceleration in A, expressed in A.
        // Also calculate [r̈, p̈, ÿ] from alpha_AD_D which is
        // D's angular acceleration in A, expressed in D.
        const RotationMatrix<double> R_AD(rpy);
        const Vector3d alpha_AD_D = R_AD.inverse() * alpha_AD_A;
        Vector3d rpyDDt, rpyDDt_verify;
        if (rpy.DoesPitchAngleViolateGimbalLockTolerance()) {
          const char* expected_message =
              "RollPitchYaw::CalcRpyDDtFromRpyDtAndAngularAccelInParent()"
              ".*gimbal-lock.*";
          DRAKE_EXPECT_THROWS_MESSAGE(rpyDDt =
             rpy.CalcRpyDDtFromRpyDtAndAngularAccelInParent(rpyDt, alpha_AD_A),
             expected_message);
          expected_message = "RollPitchYaw::CalcRpyDDtFromAngularAccelInChild()"
                             ".*gimbal-lock.*";
          DRAKE_EXPECT_THROWS_MESSAGE(rpyDDt_verify =
             rpy.CalcRpyDDtFromAngularAccelInChild(rpyDt, alpha_AD_D),
             expected_message);
          continue;
        }

        rpyDDt =
          rpy.CalcRpyDDtFromRpyDtAndAngularAccelInParent(rpyDt, alpha_AD_A);
        rpyDDt_verify =
            rpy.CalcRpyDDtFromAngularAccelInChild(rpyDt, alpha_AD_D);

        // These two calculations should produce identical answers.
        const double rpyDDt_norm = rpyDDt.norm();
        const double reasonable_value = rpyDDt_norm > 1E-5 ? rpyDDt_norm : 1E-5;
        const double tolerance = 16 * kEpsilon * reasonable_value;
        EXPECT_TRUE(CompareMatrices(rpyDDt, rpyDDt_verify, tolerance,
                                    MatrixCompareType::absolute));

        // Cross-validate with results generated by MotionGenesis.
        using std::cos;
        using std::sin;
        const double p = rpy.pitch_angle();
        const double y = rpy.yaw_angle();
        const double rDt = rpyDt(0), pDt = rpyDt(1), yDt = rpyDt(2);
        const double alfAx = alpha_AD_A(0);
        const double alfAy = alpha_AD_A(1);
        const double alfAz = alpha_AD_A(2);
        const double rDDt = (alfAx * cos(y) + alfAy * sin(y)
                          + pDt * yDt + sin(p) * pDt * rDt) / cos(p);
        const double pDDt =  alfAy * cos(y) - alfAx * sin(y)
                          - cos(p) * rDt * yDt;
        const double yDDt = alfAz + cos(p) * pDt * rDt + tan(p) *
             (alfAx * cos(y) + alfAy * sin(y) + pDt * yDt + sin(p) * pDt * rDt);
        const Vector3d rpyDDt_expected(rDDt, pDDt, yDDt);
        EXPECT_TRUE(CompareMatrices(rpyDDt, rpyDDt_expected, tolerance,
                              MatrixCompareType::absolute));
      }
    }
  }
}

GTEST_TEST(RollPitchYaw, Hash) {
  RollPitchYaw<double> R_AB(Vector3d{0.0, 0.5, 1.0});
  RollPitchYaw<double> R_CD = R_AB;
  const DefaultHash hash_func;
  EXPECT_EQ(hash_func(R_AB), hash_func(R_CD));
  R_CD.set_roll_angle(1.0);
  EXPECT_NE(hash_func(R_AB), hash_func(R_CD));
}

// Verify the constructor and a few key operations are compatible with
// symbolic::Expression.  We focus only on methods that required tweaks in
// order to compile against Expression.
GTEST_TEST(RollPitchYaw, SymbolicTest) {
  const Variable r1("r1");
  const Variable r2("r2");
  const Variable p1("p1");
  const Variable p2("p2");
  const Variable y1("y1");
  const Variable y2("y2");
  const RollPitchYaw<Expression> dut1(r1, p1, y1);
  const RollPitchYaw<Expression> dut2(r2, p2, y2);

  EXPECT_EQ(
      dut1.IsNearlyEqualTo(dut2, 1e-3).to_string(),
      fmt::format(
          "(max({}, max({}, {})) <= 0.001)",
          "abs((r1 - r2))",
          "abs((p1 - p2))",
          "abs((y1 - y2))"));

  EXPECT_THAT(
      dut1.IsNearlySameOrientation(dut2, 1e-3).to_string(),
      testing::MatchesRegex("\\(max.* <= 0.001\\)"));

  EXPECT_THAT(
      dut1.IsRollPitchYawInCanonicalRange().to_string(),
      testing::MatchesRegex(".*(and.*){5}"));

  EXPECT_THAT(
      RollPitchYaw<Expression>::DoesCosPitchAngleViolateGimbalLockTolerance(
          cos(p1)).to_string(),
      testing::StartsWith("(abs(cos(p1)) < 0.008"));

  const Vector3<Expression> vec(r1, p1, y1);
  EXPECT_THAT(
      RollPitchYaw<Expression>::IsValid(vec).to_string(),
      testing::MatchesRegex(".*inf.*(and.*inf.*){5}"));
}


// Test the stream insertion operator to write into a stream.
GTEST_TEST(RollPitchYaw, StreamInsertionOperator) {
  // Test stream insertion for RollPitchYaw<double>.
  const RollPitchYaw<double> rpy_double(0.2, 0.3, 0.4);
  std::stringstream streamA;  streamA << rpy_double;
  std::string rpy_expected_string = "rpy = 0.2 0.3 0.4";
  EXPECT_EQ(rpy_expected_string, streamA.str());

  // Test stream insertion for RollPitchYaw<AutoDiffXd>.
  const RollPitchYaw<AutoDiffXd> rpy_autodiff(0.5, 0.6, 0.7);
  std::stringstream streamB;  streamB << rpy_autodiff;
  rpy_expected_string = "rpy = 0.5 0.6 0.7";
  EXPECT_EQ(rpy_expected_string, streamB.str());

  // Test stream insertion for RollPitchYaw<symbolic::Expression>.
  const symbolic::Variable r("foo"), p("bar"), y("baz");
  const RollPitchYaw<Expression> rpy_symbolic(r, p, y);
  std::stringstream streamC;  streamC << rpy_symbolic;
  rpy_expected_string = "rpy = foo bar baz";
  EXPECT_EQ(rpy_expected_string, streamC.str());

  // When the expression strings are very long, they will be truncated.
  const Expression big_expr = sqrt(pow(r, 2) + pow(p, 2) + pow(y, 2));
  const RollPitchYaw<symbolic::Expression> rpy_symbolic_huge(
      big_expr, big_expr, big_expr);
  std::stringstream streamD;  streamD << rpy_symbolic_huge;
  rpy_expected_string = "rpy = <symbolic> <symbolic> <symbolic>";
  EXPECT_EQ(rpy_expected_string, streamD.str());
}


}  // namespace
}  // namespace math
}  // namespace drake

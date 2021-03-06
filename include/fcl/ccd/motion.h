/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */


#ifndef FCL_CCD_MOTION_H
#define FCL_CCD_MOTION_H

#include "fcl/ccd/motion_base.h"
#include "fcl/intersect.h"
#include <iostream>
#include <vector>

#include "fcl/articulated_model/link_bound.h"

#include <boost/shared_ptr.hpp>

namespace fcl
{

class SplineMotion : public MotionBase
{
public:
  /// @brief Construct motion from 4 deBoor points
  SplineMotion(const Vec3f& Td0, const Vec3f& Td1, const Vec3f& Td2, const Vec3f& Td3,
               const Vec3f& Rd0, const Vec3f& Rd1, const Vec3f& Rd2, const Vec3f& Rd3);

  
  /// @brief Integrate the motion from 0 to dt
  /// We compute the current transformation from zero point instead of from last integrate time, for precision.
  bool integrate(double start_time, double end_time = -1.0) const;

  /// @brief Compute the motion bound for a bounding volume along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const BVMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Compute the motion bound for a triangle along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const TriangleMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Get the rotation and translation in current step
  void getCurrentTransform(Matrix3f& R, Vec3f& T) const
  {
    R = tf.getRotation();
    T = tf.getTranslation();
  }

  void getCurrentRotation(Matrix3f& R) const
  {
    R = tf.getRotation();
  }

  void getCurrentTranslation(Vec3f& T) const
  {
    T = tf.getTranslation();
  }

  void getCurrentTransform(Transform3f& tf_) const
  {
    tf_ = tf;
  }

  void getTaylorModel(TMatrix3& tm, TVector3& tv) const
  {
    // set tv
    Vec3f c[4];
    c[0] = (Td[0] + Td[1] * 4 + Td[2] + Td[3]) * (1/6.0);
    c[1] = (-Td[0] + Td[2]) * (1/2.0);
    c[2] = (Td[0] - Td[1] * 2 + Td[2]) * (1/2.0);
    c[3] = (-Td[0] + Td[1] * 3 - Td[2] * 3 + Td[3]) * (1/6.0);
    tv.setTimeInterval(getTimeInterval());
    for(std::size_t i = 0; i < 3; ++i)
    {
      for(std::size_t j = 0; j < 4; ++j)
      {
        tv[i].coeff(j) = c[j][i];
      }
    }

    // set tm
    Matrix3f I(1, 0, 0, 0, 1, 0, 0, 0, 1);
    // R(t) = R(t0) + R'(t0) (t-t0) + 1/2 R''(t0)(t-t0)^2 + 1 / 6 R'''(t0) (t-t0)^3 + 1 / 24 R''''(l)(t-t0)^4; t0 = 0.5
    /// 1. compute M(1/2)
    Vec3f Rt0 = (Rd[0] + Rd[1] * 23 + Rd[2] * 23 + Rd[3]) * (1 / 48.0);
    FCL_REAL Rt0_len = Rt0.length();
    FCL_REAL inv_Rt0_len = 1.0 / Rt0_len;
    FCL_REAL inv_Rt0_len_3 = inv_Rt0_len * inv_Rt0_len * inv_Rt0_len;
    FCL_REAL inv_Rt0_len_5 = inv_Rt0_len_3 * inv_Rt0_len * inv_Rt0_len;
    FCL_REAL theta0 = Rt0_len;
    FCL_REAL costheta0 = cos(theta0);
    FCL_REAL sintheta0 = sin(theta0);
    
    Vec3f Wt0 = Rt0 * inv_Rt0_len;
    Matrix3f hatWt0;
    hat(hatWt0, Wt0);
    Matrix3f hatWt0_sqr = hatWt0 * hatWt0;
    Matrix3f Mt0 = I + hatWt0 * sintheta0 + hatWt0_sqr * (1 - costheta0);

    /// 2. compute M'(1/2)
    Vec3f dRt0 = (-Rd[0] - Rd[1] * 5 + Rd[2] * 5 + Rd[3]) * (1 / 8.0);
    FCL_REAL Rt0_dot_dRt0 = Rt0.dot(dRt0);
    FCL_REAL dtheta0 = Rt0_dot_dRt0 * inv_Rt0_len;
    Vec3f dWt0 = dRt0 * inv_Rt0_len - Rt0 * (Rt0_dot_dRt0 * inv_Rt0_len_3);
    Matrix3f hatdWt0;
    hat(hatdWt0, dWt0);
    Matrix3f dMt0 = hatdWt0 * sintheta0 + hatWt0 * (costheta0 * dtheta0) + hatWt0_sqr * (sintheta0 * dtheta0) + (hatWt0 * hatdWt0 + hatdWt0 * hatWt0) * (1 - costheta0);

    /// 3.1. compute M''(1/2)
    Vec3f ddRt0 = (Rd[0] - Rd[1] - Rd[2] + Rd[3]) * 0.5;
    FCL_REAL Rt0_dot_ddRt0 = Rt0.dot(ddRt0);
    FCL_REAL dRt0_dot_dRt0 = dRt0.sqrLength();
    FCL_REAL ddtheta0 = (Rt0_dot_ddRt0 + dRt0_dot_dRt0) * inv_Rt0_len - Rt0_dot_dRt0 * Rt0_dot_dRt0 * inv_Rt0_len_3;
    Vec3f ddWt0 = ddRt0 * inv_Rt0_len - (dRt0 * (2 * Rt0_dot_dRt0) + Rt0 * (Rt0_dot_ddRt0 + dRt0_dot_dRt0)) * inv_Rt0_len_3 + (Rt0 * (3 * Rt0_dot_dRt0 * Rt0_dot_dRt0)) * inv_Rt0_len_5;
    Matrix3f hatddWt0;
    hat(hatddWt0, ddWt0);
    Matrix3f ddMt0 =
      hatddWt0 * sintheta0 +
      hatWt0 * (costheta0 * dtheta0 - sintheta0 * dtheta0 * dtheta0 + costheta0 * ddtheta0) +
      hatdWt0 * (costheta0 * dtheta0) +
      (hatWt0 * hatdWt0 + hatdWt0 * hatWt0) * (sintheta0 * dtheta0 * 2) +
      hatdWt0 * hatdWt0 * (2 * (1 - costheta0)) +
      hatWt0 * hatWt0 * (costheta0 * dtheta0 * dtheta0 + sintheta0 * ddtheta0) +
      (hatWt0 * hatddWt0 + hatddWt0 + hatWt0) * (1 - costheta0);


    tm.setTimeInterval(getTimeInterval());
    for(std::size_t i = 0; i < 3; ++i)
    {
      for(std::size_t j = 0; j < 3; ++j)
      {
        tm(i, j).coeff(0) = Mt0(i, j) - dMt0(i, j) * 0.5 + ddMt0(i, j) * 0.25 * 0.5;
        tm(i, j).coeff(1) = dMt0(i, j) - ddMt0(i, j) * 0.5;
        tm(i, j).coeff(2) = ddMt0(i, j) * 0.5;
        tm(i, j).coeff(3) = 0;

        tm(i, j).remainder() = Interval(-1/48.0, 1/48.0); /// not correct, should fix
      }
    } 
  }

protected:
  virtual MotionBase* doClone() const
  {
    return new SplineMotion(*this);
  }

  void computeSplineParameter()
  {
  }

  FCL_REAL getWeight0(FCL_REAL t) const;
  FCL_REAL getWeight1(FCL_REAL t) const;
  FCL_REAL getWeight2(FCL_REAL t) const;
  FCL_REAL getWeight3(FCL_REAL t) const;
  
  Vec3f Td[4];
  Vec3f Rd[4];

  Vec3f TA, TB, TC;
  Vec3f RA, RB, RC;

  FCL_REAL Rd0Rd0, Rd0Rd1, Rd0Rd2, Rd0Rd3, Rd1Rd1, Rd1Rd2, Rd1Rd3, Rd2Rd2, Rd2Rd3, Rd3Rd3;
  //// @brief The transformation at current time t
  mutable Transform3f tf;

  /// @brief The time related with tf
  mutable FCL_REAL tf_t;

public:
  FCL_REAL computeTBound(const Vec3f& n) const;
  
  FCL_REAL computeDWMax() const;

  FCL_REAL getCurrentTime() const
  {
    return tf_t;
  }

};

class ScrewMotion : public MotionBase
{
public:
  /// @brief Default transformations are all identities
  ScrewMotion() :
    angular_vel(0)
  {
    // Default angular velocity is zero
    axis.setValue(1, 0, 0);    

    // Default reference point is local zero point

    // Default linear velocity is zero
    linear_vel = 0;
  }

  /// @brief Construct motion from the initial rotation/translation and goal rotation/translation
  ScrewMotion(const Matrix3f& R1, const Vec3f& T1,
              const Matrix3f& R2, const Vec3f& T2) : tf1(R1, T1),
                                                     tf2(R2, T2),
                                                     tf(tf1),
                                                     angular_vel(0)
  {
    computeScrewParameter();
  }

  /// @brief Construct motion from the initial transform and goal transform
  ScrewMotion(const Transform3f& tf1_,
              const Transform3f& tf2_) : tf1(tf1_),
                                         tf2(tf2_),
                                         tf(tf1)
  {
    computeScrewParameter();
  }

  /// @brief Integrate the motion from 0 to dt
  /// We compute the current transformation from zero point instead of from last integrate time, for precision.
  bool integrate(double start_time, double end_time = -1.0) const
  {
    if(start_time > 1) start_time = 1;
    
    tf.setQuatRotation(absoluteRotation(start_time));
    
    Quaternion3f delta_rot = deltaRotation(start_time);
    tf.setTranslation(p + axis * (start_time * linear_vel) + delta_rot.transform(tf1.getTranslation() - p));

    return true;
  }

  /// @brief Compute the motion bound for a bounding volume along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const BVMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Compute the motion bound for a triangle along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const TriangleMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }


  /// @brief Get the rotation and translation in current step
  void getCurrentTransform(Matrix3f& R, Vec3f& T) const
  {
    R = tf.getRotation();
    T = tf.getTranslation();
  }

  void getCurrentRotation(Matrix3f& R) const
  {
    R = tf.getRotation();
  }

  void getCurrentTranslation(Vec3f& T) const
  {
    T = tf.getTranslation();
  }

  void getCurrentTransform(Transform3f& tf_) const
  {
    tf_ = tf;
  }

  void getTaylorModel(TMatrix3& tm, TVector3& tv) const
  {
    Matrix3f hat_axis;
    hat(hat_axis, axis);

    TaylorModel cos_model(getTimeInterval());
    generateTaylorModelForCosFunc(cos_model, angular_vel, 0);
    
    TaylorModel sin_model(getTimeInterval());
    generateTaylorModelForSinFunc(sin_model, angular_vel, 0);

    TMatrix3 delta_R = hat_axis * sin_model - hat_axis * hat_axis * (cos_model - 1) + Matrix3f(1, 0, 0, 0, 1, 0, 0, 0, 1);

    TaylorModel a(getTimeInterval()), b(getTimeInterval()), c(getTimeInterval());
    generateTaylorModelForLinearFunc(a, 0, linear_vel * axis[0]);
    generateTaylorModelForLinearFunc(b, 0, linear_vel * axis[1]);
    generateTaylorModelForLinearFunc(c, 0, linear_vel * axis[2]);
    TVector3 delta_T = p - delta_R * p + TVector3(a, b, c);

    tm = delta_R * tf1.getRotation();
    tv = delta_R * tf1.getTranslation() + delta_T;
  }

protected:
  virtual MotionBase* doClone() const
  {
    return new ScrewMotion(*this);
  }

  void computeScrewParameter()
  {
    Quaternion3f deltaq = tf2.getQuatRotation() * inverse(tf1.getQuatRotation());
    deltaq.toAxisAngle(axis, angular_vel);
    if(angular_vel < 0)
    {
      angular_vel = -angular_vel;
      axis = -axis;
    }

    if(angular_vel < 1e-10)
    {
      angular_vel = 0;
      axis = tf2.getTranslation() - tf1.getTranslation();
      linear_vel = axis.length();
      p = tf1.getTranslation();
    }
    else
    {
      Vec3f o = tf2.getTranslation() - tf1.getTranslation();
      p = (tf1.getTranslation() + tf2.getTranslation() + axis.cross(o) * (1.0 / tan(angular_vel / 2.0))) * 0.5;
      linear_vel = o.dot(axis);
    }
  }

  Quaternion3f deltaRotation(FCL_REAL dt) const
  {
    Quaternion3f res;
    res.fromAxisAngle(axis, (FCL_REAL)(dt * angular_vel));
    return res;
  }

  Quaternion3f absoluteRotation(FCL_REAL dt) const
  {
    Quaternion3f delta_t = deltaRotation(dt);
    return delta_t * tf1.getQuatRotation();
  }

  /// @brief The transformation at time 0
  Transform3f tf1;

  /// @brief The transformation at time 1
  Transform3f tf2;

  /// @brief The transformation at current time t
  mutable Transform3f tf;

  /// @brief screw axis
  Vec3f axis;

  /// @brief A point on the axis S
  Vec3f p;

  /// @brief linear velocity along the axis
  FCL_REAL linear_vel;

  /// @brief angular velocity
  FCL_REAL angular_vel;

public:
  inline FCL_REAL getLinearVelocity() const
  {
    return linear_vel;
  }

  inline FCL_REAL getAngularVelocity() const
  {
    return angular_vel;
  }

  inline const Vec3f& getAxis() const
  {
    return axis;
  }

  inline const Vec3f& getAxisOrigin() const
  {
    return p;
  }
};



/// @brief Linear interpolation motion
/// Each Motion is assumed to have constant linear velocity and angular velocity
/// The motion is R(t)(p - p_ref) + p_ref + T(t)
/// Therefore, R(0) = R0, R(1) = R1
///            T(0) = T0 + R0 p_ref - p_ref
///            T(1) = T1 + R1 p_ref - p_ref
class InterpMotion : public MotionBase
{
public:
  /// @brief Default transformations are all identities
  InterpMotion();

  /// @brief Construct motion from the initial rotation/translation and goal rotation/translation
  InterpMotion(const Matrix3f& R1, const Vec3f& T1,
               const Matrix3f& R2, const Vec3f& T2);

  InterpMotion(const Transform3f& tf1_, const Transform3f& tf2_);

  /// @brief Construct motion from the initial rotation/translation and goal rotation/translation related to some rotation center
  InterpMotion(const Matrix3f& R1, const Vec3f& T1,
               const Matrix3f& R2, const Vec3f& T2,
               const Vec3f& O);

  InterpMotion(const Transform3f& tf1_, const Transform3f& tf2_, const Vec3f& O);

  /// @brief Integrate the motion from 0 to dt
  /// We compute the current transformation from zero point instead of from last integrate time, for precision.
  bool integrate(double start_time, double end_time = -1.0) const;

  /// @brief Compute the motion bound for a bounding volume along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const BVMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Compute the motion bound for a triangle along a given direction n, which is defined in the visitor 
  FCL_REAL computeMotionBound(const TriangleMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Get the rotation and translation in current step
  void getCurrentTransform(Matrix3f& R, Vec3f& T) const
  {
    R = tf.getRotation();
    T = tf.getTranslation();
  }

  void getCurrentRotation(Matrix3f& R) const
  {
    R = tf.getRotation();
  }

  void getCurrentTranslation(Vec3f& T) const
  {
    T = tf.getTranslation();
  }

  void getCurrentTransform(Transform3f& tf_) const
  {
    tf_ = tf;
  }

  void getTaylorModel(TMatrix3& tm, TVector3& tv) const
  {
    Matrix3f hat_angular_axis;
    hat(hat_angular_axis, angular_axis);

    TaylorModel cos_model(getTimeInterval());
    generateTaylorModelForCosFunc(cos_model, angular_vel, 0);
    TaylorModel sin_model(getTimeInterval());
    generateTaylorModelForSinFunc(sin_model, angular_vel, 0);

    TMatrix3 delta_R = hat_angular_axis * sin_model - hat_angular_axis * hat_angular_axis * (cos_model - 1) + Matrix3f(1, 0, 0, 0, 1, 0, 0, 0, 1);

    TaylorModel a(getTimeInterval()), b(getTimeInterval()), c(getTimeInterval());
    generateTaylorModelForLinearFunc(a, 0, linear_vel[0]);
    generateTaylorModelForLinearFunc(b, 0, linear_vel[1]);
    generateTaylorModelForLinearFunc(c, 0, linear_vel[2]);
    TVector3 delta_T(a, b, c);
    
    tm = delta_R * tf1.getRotation();
    tv = tf1.transform(reference_p) + delta_T - delta_R * tf1.getQuatRotation().transform(reference_p);
  }

protected:
  virtual MotionBase* doClone() const
  {
    return new InterpMotion(*this);
  }

  void computeVelocity();

  Quaternion3f deltaRotation(FCL_REAL dt) const;
  
  Quaternion3f absoluteRotation(FCL_REAL dt) const;

protected:  
  /// @brief The transformation at time 0
  Transform3f tf1;

  /// @brief The transformation at time 1
  Transform3f tf2;

  /// @brief The transformation at current time t
  mutable Transform3f tf;

  /// @brief Linear velocity
  Vec3f linear_vel;

  /// @brief Angular speed
  FCL_REAL angular_vel;

  /// @brief Angular velocity axis
  Vec3f angular_axis;

  /// @brief Reference point for the motion (in the object's local frame)
  Vec3f reference_p;

public:
  const Vec3f& getReferencePoint() const
  {
    return reference_p;
  }

  const Vec3f& getAngularAxis() const
  {
    return angular_axis;
  }

  FCL_REAL getAngularVelocity() const
  {
    return angular_vel;
  }

  const Vec3f& getLinearVelocity() const
  {
    return linear_vel;
  }
};

class ArticularMotion : public MotionBase
{
public:  
  /// @brief Construct motion from the LinkBound (that contains Model, Start and End Model's Configuration and Motion)
  ArticularMotion(boost::shared_ptr<LinkBound> link_bound);

  /// @brief Integrate the motion from 0 to dt
  /// We compute the current transformation from zero point instead of from last integrate time, for precision.
  bool integrate(double start_time, double end_time = -1.0) const;

  /// @brief Compute the motion bound for a bounding volume along a given direction n, which is defined in the visitor
  FCL_REAL computeMotionBound(const BVMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  /// @brief Compute the motion bound for a triangle along a given direction n, which is defined in the visitor 
  FCL_REAL computeMotionBound(const TriangleMotionBoundVisitor& mb_visitor) const
  {
    return mb_visitor.visit(*this);
  }

  FCL_REAL getMotionBound(const Vec3f& direction, const FCL_REAL max_distance_from_joint_center = 0) const;
  FCL_REAL getNonDirectionalMotionBound(const FCL_REAL max_distance_from_joint_center = 0) const;

  /// @brief Get the rotation and translation in current step
  void getCurrentTransform(Matrix3f& R, Vec3f& T) const
  {
    R = tf_.getRotation();
    T = tf_.getTranslation();
  }

  void getCurrentRotation(Matrix3f& R) const
  {
    R = tf_.getRotation();
  }

  void getCurrentTranslation(Vec3f& T) const
  {
    T = tf_.getTranslation();
  }

  void getCurrentTransform(Transform3f& tf) const
  {
    tf = tf_;
  }

  void getTaylorModel(TMatrix3& tm, TVector3& tv) const
  {     
    /*tm = TMatrix3(Matrix3f::getIdentity(), getTimeInterval() );

    Vec3f translation;
    getCurrentTranslation(translation);

    tv = TVector3(translation, getTimeInterval() );
    tv[0].remainder().setValue();*/
  }

  void setReferencePoint(const Vec3f& reference_point)
  {
    reference_point_ = reference_point;
  }

  Vec3f getReferencePoint() const
  {
    return reference_point_;
  }

protected:
	virtual MotionBase* doClone() const
	{
		return new ArticularMotion(*this);
	}

private:
  // Non parametrized constructor is not allowed
  ArticularMotion() :
    start_time_(0.0) {};

private:
  boost::shared_ptr<LinkBound> link_bound_;

  /// @brief The transformation at current time t
  mutable Transform3f tf_;

  /// @brief The transformation at current time t
  mutable FCL_REAL start_time_;
  mutable FCL_REAL end_time_;

  /// @brief Reference point for the motion (in the object's local frame)
  Vec3f reference_point_;  

};


}

#endif

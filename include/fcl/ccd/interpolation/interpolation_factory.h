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

/** \author Dalibor Matura, Jia Pan */

#ifndef FCL_CCD_INTERPOLATION_INTERPOLATION_FACTORY_H
#define FCL_CCD_INTERPOLATION_INTERPOLATION_FACTORY_H

#include "fcl/data_types.h"
#include "fcl/ccd/interpolation/interpolation.h"

#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace fcl 
{

class InterpolationData;

class InterpolationFactory
{	
public:
  typedef boost::function<boost::shared_ptr<Interpolation>(
    const boost::shared_ptr<const InterpolationData>&, 
    FCL_REAL, FCL_REAL)> CreateFunction;

public:
  void registerClass(const InterpolationType type, const CreateFunction create_function);

  boost::shared_ptr<Interpolation> create(const boost::shared_ptr<const InterpolationData>& data,
    const FCL_REAL& start_value, const FCL_REAL& end_value);

public:
  static InterpolationFactory& instance();

private:
  InterpolationFactory();

  InterpolationFactory(const InterpolationFactory&)
  {}

  InterpolationFactory& operator = (const InterpolationFactory&)
  {
    return *this;
  }

private:
  std::map<InterpolationType, CreateFunction> creation_map_;
};

}

#endif

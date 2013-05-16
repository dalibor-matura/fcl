#include "fcl/articulated_model/link_bound.h"

#include "fcl/articulated_model/model.h"
#include "fcl/articulated_model/model_config.h"
#include "fcl/articulated_model/link.h"
#include "fcl/articulated_model/joint.h"
#include "fcl/articulated_model/joint_config.h"
#include "fcl/articulated_model/movement.h"

#include <boost/assert.hpp>

namespace fcl 
{

LinkBound::LinkBound(boost::shared_ptr<const Movement> movement, boost::shared_ptr<const Link> bounded_link) :
	model_(movement->getModel() ),
	movement_(movement),
	bounded_link_(bounded_link),
	start_time_(0.0),
	end_time_(1.0),
	last_start_time_(-1.0),
	last_end_time_(-1.0),
	start_time_cached_(false),
	end_time_cached_(false),
	cashed_joints_chain_motion_bound_(0.0)
{	
	initJointsChain();
}

void LinkBound::initJointsChain()
{
	joints_chain_ = model_->getJointsChainFromLastJoint(getLastJoint() );
}

boost::shared_ptr<Joint> LinkBound::getLastJoint() const
{
	return bounded_link_->getParentJoint();
}

const std::vector<boost::shared_ptr<const Joint> >& LinkBound::getJointsChain() const
{
	return joints_chain_;
}

FCL_REAL LinkBound::getMotionBound(FCL_REAL start_time, FCL_REAL end_time,
	const Vec3f& direction, const FCL_REAL max_distance_from_joint_center)
{
	FCL_REAL motion_bound = 0;
	
	sortTimes(start_time, end_time);

	setStartTime(start_time);
	setEndTime(end_time);
	setCurrentDirection(direction);	

	// add motion bound generated by joints
	if (areTimesCashed() )
	{
		motion_bound += cashed_joints_chain_motion_bound_;
	}
	else
	{
		resetAngularBoundAccumulation();
		cashed_joints_chain_motion_bound_= getJointsChainMotionBound();
		motion_bound += cashed_joints_chain_motion_bound_;
	}	

	// add motion bound for collision object (rigid body) fixed to the link of the last joint
	motion_bound += getObjectMotionBoundInLastJointFrame(max_distance_from_joint_center);

	return motion_bound;
}

FCL_REAL LinkBound::getNonDirectionalMotionBound(const FCL_REAL& start_time, 
	const FCL_REAL& end_time, const FCL_REAL max_distance_from_joint_center)
{
	return getMotionBound(start_time,end_time, Vec3f(0.0, 0.0, 0.0), max_distance_from_joint_center);
}

void LinkBound::sortTimes(FCL_REAL& start_time, FCL_REAL& end_time) const
{
	if (start_time > end_time)
	{
		std::swap(start_time, end_time);
	}
}

FCL_REAL LinkBound::getJointsChainMotionBound() const
{
	FCL_REAL motion_bound = 0;
	
	std::vector<boost::shared_ptr<const Joint> >::const_reverse_iterator 
		reverse_it = getJointsChain().rbegin();

	// skip root joint
	if (reverse_it != getJointsChain().rend() )
	{
		++reverse_it;
	}

	for ( ; reverse_it != getJointsChain().rend(); ++reverse_it)
	{
		const boost::shared_ptr<const Joint>& joint = *reverse_it;

		motion_bound += getMotionBoundInParentFrame(joint);
	}

	return motion_bound;
}

FCL_REAL LinkBound::getMotionBoundInParentFrame(const boost::shared_ptr<const Joint>& joint) const 
{
	boost::shared_ptr<const Joint> parent = model_->getJointParent(joint);

	BOOST_ASSERT(parent.use_count() != 0 && "Joint is not assigned.");

	if (isRoot(parent) && isCurrentDirectionValid() )
	{
		return getDirectionalMotionBoundInParentFrame(joint);
	}
	else
	{
		return getSimpleMotionBoundInParentFrame(joint);
	}
}

FCL_REAL LinkBound::getDirectionalMotionBoundInParentFrame(const boost::shared_ptr<const Joint>& joint) const 
{
	FCL_REAL motion_bound = 0;

	const boost::shared_ptr<const Joint> parent = model_->getJointParent(joint);

	motion_bound += (movement_->getLinearVelocityBound(
		parent, getStartTime(), getEndTime() ) * getCurrentDirection() ).length();
	motion_bound += getAccumulatedAngularBound(parent, true) * 
		movement_->getChildParentDistanceBound(joint, parent);

	return motion_bound;
}

FCL_REAL LinkBound::getSimpleMotionBoundInParentFrame(const boost::shared_ptr<const Joint>& joint) const 
{
	FCL_REAL motion_bound = 0;

	boost::shared_ptr<const Joint> parent = model_->getJointParent(joint);

	motion_bound += movement_->getAbsoluteLinearVelocityBound(
		parent, getStartTime(), getEndTime() );
	motion_bound +=  getAccumulatedAngularBound(parent) * 
		movement_->getChildParentDistanceBound(joint, parent);

	return motion_bound;
}

FCL_REAL LinkBound::getObjectMotionBoundInLastJointFrame(const FCL_REAL max_distance_from_joint_center) const
{
	FCL_REAL motion_bound = 0;

	boost::shared_ptr<const Joint> joint = getLastJoint();

	if (joint.use_count() == 0)
	{
		return 0;
	}

	motion_bound += movement_->getAbsoluteLinearVelocityBound(
		joint, getStartTime(), getEndTime() );
	motion_bound += getAccumulatedAngularBoundForLastJointFrame(joint) * max_distance_from_joint_center;

	return motion_bound;
}

bool LinkBound::isRoot(const boost::shared_ptr<const Joint>& joint) const
{
	boost::shared_ptr<const Joint> parent = model_->getJointParent(joint);	

	return (parent.use_count() == 0);
}

FCL_REAL LinkBound::getAccumulatedAngularBound(const boost::shared_ptr<const Joint>& joint, bool is_directional) const
{
	FCL_REAL angular_bound = 0;

	if (is_directional)
	{
		angular_bound = getCurrentDirection().cross(movement_->getAngularVelocityBound(
			joint, getStartTime(), getEndTime() )).length();
	}
	else
	{
		angular_bound = movement_->getAbsoluteAngularVelocityBound(
			joint, getStartTime(), getEndTime() );
	}

	addAngularBoundAccumulation(angular_bound);		

	return getAngularBoundAccumulation();
}

FCL_REAL LinkBound::getAccumulatedAngularBoundForLastJointFrame(const boost::shared_ptr<const Joint>& joint) const
{
	FCL_REAL angular_bound = movement_->getAbsoluteAngularVelocityBound(
		joint, getStartTime(), getEndTime() );	

	return getAngularBoundAccumulation() + angular_bound;
}

void LinkBound::setCurrentDirection(const Vec3f& direction)
{
	direction_ = direction;

	setLastDirection(direction);
}

Vec3f LinkBound::getCurrentDirection() const
{
	return direction_;
}

bool LinkBound::isCurrentDirectionValid() const
{
	return direction_[0] != 0.0 || direction_[1] != 0.0 || direction_[2] != 0.0;
}

void LinkBound::setStartTime(const FCL_REAL& time)
{
	start_time_ = time;

	setLastStartTime(time);
}

FCL_REAL LinkBound::getStartTime() const
{
	return start_time_;
}

void LinkBound::setEndTime(const FCL_REAL& time)
{
	end_time_ = time;

	setLastEndTime(time);
}

FCL_REAL LinkBound::getEndTime() const
{
	return end_time_;
}

void LinkBound::setLastStartTime(const FCL_REAL& time)
{
	start_time_cached_ = (time == last_start_time_);

	last_start_time_ = time;
}

void LinkBound::setLastEndTime(const FCL_REAL& time)
{
	end_time_cached_ = (time == last_end_time_);

	last_end_time_ = time;
}

void LinkBound::setLastDirection(const Vec3f& direction)
{
	direction_cached_ = (
		direction[0] == last_direction_[0] &&
		direction[1] == last_direction_[1] &&
		direction[2] == last_direction_[2]
	);

	last_direction_ = direction;
}

bool LinkBound::areTimesCashed() const
{
	return start_time_cached_ && end_time_cached_ && direction_cached_;
}

void LinkBound::resetAngularBoundAccumulation()
{
	accumulated_angular_bound_ = 0.0;
}

void LinkBound::addAngularBoundAccumulation(const FCL_REAL& accumulation) const
{
	accumulated_angular_bound_ += accumulation;
}

FCL_REAL LinkBound::getAngularBoundAccumulation() const
{
	return accumulated_angular_bound_;
}

Transform3f LinkBound::getBoundedLinkGlobalTransform(const FCL_REAL& time) const
{
	return movement_->getGlobalTransform(bounded_link_, time);
}

boost::shared_ptr<const Link> LinkBound::getBoundedLink() const
{
	return bounded_link_;
}

} /* namespace collection */
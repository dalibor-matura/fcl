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

LinkBound::LinkBound(boost::shared_ptr<const Model> model, boost::shared_ptr<const Movement> movement,
	boost::shared_ptr<const Link> bounded_link) :
	model_(model),
	movement_(movement),
	bounded_link_(bounded_link)
{	
	initJointsChain();
}

void LinkBound::initJointsChain()
{
	joints_chain_ = movement_->getJointsChainFromLastJoint(getLastJoint() );
}

boost::shared_ptr<Joint> LinkBound::getLastJoint() const
{
	return bounded_link_->getParentJoint();
}

const std::vector<boost::shared_ptr<const Joint> >& LinkBound::getJointsChain() const
{
	return joints_chain_;
}

FCL_REAL LinkBound::getMotionBound(const FCL_REAL& time, const Vec3f& direction, 
	const FCL_REAL max_distance_from_joint_center)
{
	FCL_REAL motion_bound = 0;
	
	setCurrentTime(time);
	setCurrentDirection(direction);
	
	resetAngularBoundAccumulation();

	// add motion bound generated by other joints
	motion_bound += getJointsChainMotionBound();

	// add motion bound for collision object (rigid body) fixed to the link of the last joint
	motion_bound += getObjectMotionBoundInLastJointFrame(max_distance_from_joint_center);

	return motion_bound;
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

	BOOST_ASSERT_MSG(parent.use_count() != 0, "Joint is not assigned.");

	if (isRoot(parent) )
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

	motion_bound += (movement_->getLinearVelocityBound(parent, getCurrentTime() ) * getCurrentDirection() ).length();
	motion_bound += getAccumulatedAngularBound(parent, true) * 
		movement_->getChildParentDistanceBound(joint, parent);

	return motion_bound;
}

FCL_REAL LinkBound::getSimpleMotionBoundInParentFrame(const boost::shared_ptr<const Joint>& joint) const 
{
	FCL_REAL motion_bound = 0;

	boost::shared_ptr<const Joint> parent = model_->getJointParent(joint);

	motion_bound += movement_->getAbsoluteLinearVelocityBound(parent, getCurrentTime() );
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

	motion_bound += movement_->getAbsoluteLinearVelocityBound(joint, getCurrentTime() );
	motion_bound += getAccumulatedAngularBound(joint) * max_distance_from_joint_center;

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
		angular_bound = getCurrentDirection().cross(movement_->getAngularVelocityBound(joint, getCurrentTime() )).length();
	}
	else
	{
		angular_bound = movement_->getAngularVelocityBound(joint, getCurrentTime() ).length();
	}

	addAngularBoundAccumulation(angular_bound);		

	return getAngularBoundAccumulation();
}

void LinkBound::setCurrentDirection(const Vec3f& direction)
{
	direction_ = direction;
}

Vec3f LinkBound::getCurrentDirection() const
{
	return direction_;
}

void LinkBound::setCurrentTime(const FCL_REAL& time)
{
	time_ = time;
}

FCL_REAL LinkBound::getCurrentTime() const
{
	return time_;
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

} /* namespace collection */
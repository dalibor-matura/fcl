#include "fcl/articulated_model/model_bound.h"

#include "fcl/articulated_model/model.h"
#include "fcl/articulated_model/model_config.h"
#include "fcl/articulated_model/link.h"
#include "fcl/articulated_model/joint.h"
#include "fcl/articulated_model/joint_config.h"
#include "fcl/articulated_model/joint_bound_info.h"

#include <boost/assert.hpp>

namespace fcl 
{

ModelBound::ModelBound(boost::shared_ptr<Model> model, 
	boost::shared_ptr<const ModelConfig> cfg_start, boost::shared_ptr<const ModelConfig> cfg_end) :

	model_(model),
	cfg_start_(cfg_start),
	cfg_end_(cfg_end),
	joint_bound_info_(new JointBoundInfo(model, cfg_start, cfg_end) )
{	
	initJointsParentTree();
}

void ModelBound::initJointsParentTree()
{
	std::map<std::string, std::string> link_parent_tree;

	model_->initTree(link_parent_tree);
	model_->initRoot(link_parent_tree);	

	boost::shared_ptr<const Link> root_link = model_->getRoot();

	constructParentTree(link_parent_tree, joint_parent_tree_, root_link);
}

void ModelBound::constructParentTree(const std::map<std::string, std::string>& link_parent_tree,
	std::map<std::string, std::string>& joint_parent_tree, boost::shared_ptr<const Link>& link)
{	
	std::vector<boost::shared_ptr<const Joint> > child_joints = link->getChildJoints();
	boost::shared_ptr<const Joint> parent_joint = link->getParentJoint();
	std::vector<boost::shared_ptr<const Joint> >::const_iterator it;

	for (it = child_joints.begin(); it != child_joints.end(); ++it)
	{
		boost::shared_ptr<const Joint> joint = (*it);
		boost::shared_ptr<const Link> child_link = joint->getChildLink();

		if (parent_joint.use_count() != 0)
		{
			joint_parent_tree[joint->getName()] = parent_joint->getName();
		}		

		if (child_link.use_count() != 0)
		{
			constructParentTree(link_parent_tree, joint_parent_tree_, child_link);
		}
	}
}

FCL_REAL ModelBound::getMotionBound(const std::string& link_name, const FCL_REAL& time, 
	const Vec3f& direction, const FCL_REAL max_distance_from_joint_center)
{
	FCL_REAL motion_bound = 0;
	
	joint_bound_info_->setCurrentTime(time);
	setCurrentDirection(direction);
	
	resetAngularBoundAccumulation();

	boost::shared_ptr<const Link> link = model_->getLink(link_name);
	BOOST_ASSERT_MSG(link.use_count() != 0.0, "Link with given name doesn't exist!");

	boost::shared_ptr<const Joint> last_joint = link->getParentJoint();
	if (last_joint.use_count() == 0.0)
	{
		// no joint's parent means motion bound equals 0.0
		return 0.0;
	}

	std::string last_joint_name = last_joint->getName();
	std::vector<boost::shared_ptr<const Joint> > joints_chain = getJointsChainFromLastJoint(last_joint_name);		

	// add motion bound generated by other joints
	motion_bound += getJointsChainMotionBound(joints_chain);

	// add motion bound for collision object (rigid body) fixed to the link of the last joint
	motion_bound += getObjectMotionBoundInJointFrame(last_joint, max_distance_from_joint_center);

	return motion_bound;
}

std::vector<boost::shared_ptr<const Joint> >
	ModelBound::getJointsChainFromLastJoint(const std::string& last_joint_name) const
{
	std::vector<boost::shared_ptr<const Joint> > joints_chain;

	boost::shared_ptr<const Joint> joint = model_->getJoint(last_joint_name);

	while (joint.use_count() != 0)
	{
		joints_chain.push_back(joint);

		joint = getJointParent(joint);
	}

	return joints_chain;
}

boost::shared_ptr<const Joint> ModelBound::getJointParent(const boost::shared_ptr<const Joint>& joint) const 
{
	std::string joint_name = joint->getName();
	std::string parent_name = getJointParentName(joint_name);

	return model_->getJoint(parent_name);
}

std::string ModelBound::getJointParentName(const std::string& joint_name) const
{
	std::map<std::string, std::string>::const_iterator it = joint_parent_tree_.find(joint_name);

	if (it == joint_parent_tree_.end() )
	{
		return "";
	}

	return it->second;
}

FCL_REAL ModelBound::getJointsChainMotionBound(std::vector<boost::shared_ptr<const Joint> >& joints_chain) const
{
	FCL_REAL motion_bound = 0;

	// remove root joint
	pop(joints_chain);

	std::vector<boost::shared_ptr<const Joint> >::iterator it;

	for (it = joints_chain.begin(); it != joints_chain.end(); ++it)
	{
		boost::shared_ptr<const Joint>& joint = *it;

		motion_bound += getMotionBoundInParentFrame(joint);
	}

	return motion_bound;
}

FCL_REAL ModelBound::getMotionBoundInParentFrame(boost::shared_ptr<const Joint>& joint) const 
{
	boost::shared_ptr<const Joint> parent = getJointParent(joint);

	if (isRoot(parent) )
	{
		return getDirectionalMotionBoundInParentFrame(joint);
	}
	else
	{
		return getSimpleMotionBoundInParentFrame(joint);
	}
}

FCL_REAL ModelBound::getDirectionalMotionBoundInParentFrame(boost::shared_ptr<const Joint>& joint) const 
{
	FCL_REAL motion_bound = 0;

	boost::shared_ptr<const Joint> parent = getJointParent(joint);

	motion_bound += (joint_bound_info_->getLinearVelocityBound(parent) * getCurrentDirection() ).length();
	motion_bound += getAccumulatedAngularBound(parent, true) * joint_bound_info_->getVectorLengthBound(parent, joint);

	return motion_bound;
}

FCL_REAL ModelBound::getSimpleMotionBoundInParentFrame(boost::shared_ptr<const Joint>& joint) const 
{
	FCL_REAL motion_bound = 0;

	boost::shared_ptr<const Joint> parent = getJointParent(joint);

	motion_bound += joint_bound_info_->getAbsoluteLinearVelocityBound(parent);
	motion_bound +=  getAccumulatedAngularBound(parent) * joint_bound_info_->getVectorLengthBound(parent, joint);

	return motion_bound;
}

FCL_REAL ModelBound::getObjectMotionBoundInJointFrame(boost::shared_ptr<const Joint>& joint,
	const FCL_REAL max_distance_from_joint_center)
{
	FCL_REAL motion_bound = 0;

	motion_bound += joint_bound_info_->getAbsoluteLinearVelocityBound(joint);
	motion_bound += getAccumulatedAngularBound(joint) * max_distance_from_joint_center;

	return motion_bound;
}

bool ModelBound::isRoot(const boost::shared_ptr<const Joint>& joint) const
{
	boost::shared_ptr<const Joint> parent = getJointParent(joint);	

	return (parent.use_count() == 0);
}

FCL_REAL ModelBound::getAccumulatedAngularBound(boost::shared_ptr<const Joint>& joint, bool is_directional) const
{
	FCL_REAL angular_bound = 0;

	if (is_directional)
	{
		angular_bound = getCurrentDirection().cross(joint_bound_info_->getAngularVelocityBound(joint)).length();
	}
	else
	{
		angular_bound = joint_bound_info_->getAngularVelocityBound(joint).length();
	}

	addAngularBoundAccumulation(angular_bound);		

	return getAngularBoundAccumulation();
}

boost::shared_ptr<const Joint> ModelBound::pop(std::vector<boost::shared_ptr<const Joint> >& vec) const
{
	boost::shared_ptr<const Joint> joint = vec.back();
	vec.pop_back();	 

	return joint;
}

void ModelBound::setCurrentDirection(const Vec3f& direction)
{
	direction_ = direction;
}

Vec3f ModelBound::getCurrentDirection() const
{
	return direction_;
}

void ModelBound::resetAngularBoundAccumulation()
{
	accumulated_angular_bound_ = 0.0;
}

void ModelBound::addAngularBoundAccumulation(const FCL_REAL& accumulation) const
{
	accumulated_angular_bound_ += accumulation;
}

FCL_REAL ModelBound::getAngularBoundAccumulation() const
{
	return accumulated_angular_bound_;
}

} /* namespace collection */
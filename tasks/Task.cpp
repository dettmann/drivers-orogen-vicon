#include "Task.hpp"

#include <rtt/FileDescriptorActivity.hpp>
#include <ViconDriver.hpp>

using namespace vicon;


namespace vicon {
class TaskImpl
{
public:
    Driver driver;
};
}

RTT::FileDescriptorActivity* Task::getFileDescriptorActivity()
{ return dynamic_cast< RTT::FileDescriptorActivity* >(getActivity().get()); }


Task::Task(std::string const& name)
    : TaskBase(name), impl( new TaskImpl() )
{
}


/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See Task.hpp for more detailed
// documentation about them.

bool Task::configureHook()
{
    return true;
}
bool Task::startHook()
{
    bool result = impl->driver.connect( _host.value(), _port.value() );
    return result;
}

void Task::updateHook()
{
    const base::Time timeout( base::Time::fromSeconds(0) );
    while( impl->driver.getFrame( timeout ) )
    {
	// origin is the origin2world transform for the neutral position/orientation
	Eigen::Transform3d C_world2origin( Eigen::Transform3d(_origin.value()).inverse() );

	base::samples::RigidBodyState rbs;
	rbs.time = impl->driver.getTimestamp();
	rbs.setTransform( C_world2origin * impl->driver.getSegmentTransform( _subject.value(), _segment.value() ) );
	_pose_samples.write( rbs );
    }
}

// void Task::errorHook()
// {
// }
void Task::stopHook()
{
    impl->driver.disconnect();
}
// void Task::cleanupHook()
// {
// }


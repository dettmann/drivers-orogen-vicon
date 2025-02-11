#include "Task.hpp"

#include <ViconDriver.hpp>

using namespace vicon;


namespace vicon {
class TaskImpl
{
public:
    Driver driver;
};
}

Task::Task(std::string const& name)
    : TaskBase(name), impl( new TaskImpl() )
{
}

::base::samples::RigidBodyState Task::getZeroOrigin()
{
    base::samples::RigidBodyState lrbs;
    lrbs.initUnknown();
    return lrbs;
}


/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See Task.hpp for more detailed
// documentation about them.

bool Task::configureHook()
{
    _origin.value().orientation.normalize();
    if (!_origin.value().hasValidPosition() || !_origin.value().hasValidOrientation() )
        _origin.set(getZeroOrigin());
    _body_reference.value().orientation.normalize();
    if (!_body_reference.value().hasValidPosition() || !_body_reference.value().hasValidOrientation() )
        _body_reference.set(getZeroOrigin());

    uncertainty.reset(new vicon::ViconUncertainty<Eigen::Matrix4d>(_uncertainty_samples.value()));

    return true;
}

bool Task::startHook()
{
    bool result = impl->driver.connect( _host.value(), _port.value() );
    if (result)
        impl->driver.setAxesDir( _xdir.value(), _ydir.value(), _zdir.value() );
    return result;
}

void Task::updateHook()
{
    const base::Time timeout( base::Time::fromSeconds(0) );
    while( impl->driver.getFrame( timeout ) )
    {
	// origin is the origin2world transform for the neutral position/orientation
	Eigen::Affine3d C_world2origin( Eigen::Affine3d(_origin.value()).inverse() );
        Eigen::Affine3d C_segment2body( Eigen::Affine3d(_body_reference.value()) );

	base::samples::RigidBodyState rbs;
	rbs.time = impl->driver.getTimestamp();
        rbs.sourceFrame = _source_frame.get();
        rbs.targetFrame = _target_frame.get();

        bool in_frame;
        Eigen::Affine3d segment_transform = impl->driver.getSegmentTransform(
        _subject.value(), _segment.value(), in_frame );

        _unlabeled_markers.write( impl->driver.getUnlabeledMarkers() );

        switch(impl->driver.getLastResult())
        {
            case Driver::INVALID_SUBJECT_NAME:
                RTT::log(RTT::Error) << "subject " << _subject.value() << " not found!"
                << RTT::endlog();
                return;
            case Driver::INVALID_SEGMENT_NAME:
                RTT::log(RTT::Error) << "segment " << _segment.value() << " not found!" 
                << RTT::endlog();
                return;
        }

        if (in_frame || !_invalidate_occluded.get())
        {
            /** Fill the Rbs transformation **/
            rbs.setTransform( C_world2origin * segment_transform * C_segment2body );

            /** Set uncertainty in the rbs **/
            if (_uncertainty_samples.value() > 0)
            {
                /** Push sample to the uncertainty **/
                uncertainty->push(rbs.getTransform().matrix());

                /** On line uncertainty **/
                Eigen::Matrix4d transform_uncertainty = uncertainty->getVariance();

                rbs.cov_position = transform_uncertainty.block<3,1>(0,3).asDiagonal();
                rbs.cov_orientation = transform_uncertainty.block<3,3>(0,0);
            }
        }
        else
            rbs.invalidate();

        if (in_frame || !_drop_occluded.get()) {
	    _pose_samples.write( rbs );

            //base::Vector3d root2frame(-0.317, 0.0, 0.27);
            //root2frame = _body_reference.value().orientation.inverse() * root2frame;
            rbs.setTransform(rbs.getTransform().inverse());
            //rbs.position = root2frame;
            _inverted_pose_samples.write(rbs);
        }
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


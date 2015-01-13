// Author : Antoine Hoarau <hoarau.robotics@gmail.com>
#include <stdio.h>
// ROS
#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <geometry_msgs/Point32.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <sensor_msgs/PointCloud2.h>
// MoveIt!
#include <pluginlib/class_loader.h>
#include <moveit/robot_model/robot_model.h>
#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit/planning_interface/planning_interface.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <moveit_msgs/PlanningScene.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit/planning_scene_monitor/current_state_monitor.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
// Eigen
#include <eigen_conversions/eigen_msg.h>
#include <Eigen/Eigen>

int main(int argc, char** argv)
{
    ros::Time::init();
    ros::init(argc, argv, "robot_model_to_pointcloud");
    
    ros::AsyncSpinner spinner(1);
    spinner.start();
    ros::NodeHandle nh("~");
    
    ros::Publisher cloud_pub = nh.advertise<sensor_msgs::PointCloud2>("robot_cloud", 1);
    
    ROS_INFO("Loading robot from the parameter server");

    std::string robot_description;
    if(! nh.getParam("/robot_description",robot_description))
    {
        ROS_ERROR("/robot_description is not broadcasted");
        return 0;
    }

    std::string joint_states_topic("/joint_states");
    if(! nh.getParam("/joint_states",joint_states_topic))
    {
        ROS_WARN("joint_states_topic will be set to default : %s",joint_states_topic.c_str());
    }

    planning_scene_monitor::PlanningSceneMonitor psm("/robot_description");
    psm.startStateMonitor(joint_states_topic);
    
    sensor_msgs::PointCloud cloud;
    sensor_msgs::PointCloud2 cloud2;
    cloud.header.frame_id=psm.getStateMonitor()->getRobotModel()->getRootLinkName();
    geometry_msgs::Point32 pt;
    
    while(ros::ok()){
        if(psm.getStateMonitor()->waitForCurrentState(1.0)){
            std::vector<robot_model::LinkModel*> links = psm.getStateMonitor()->getCurrentState()->getRobotModel()->getLinkModelsWithCollisionGeometry();
            cloud.points.clear();

            for (std::size_t i = 0 ; i < links.size() ; ++i)
            {
                ROS_DEBUG_STREAM(""<<links[i]->getName());
		// Get the link transform (base to link_i )
                const Eigen::Affine3d& Transform = psm.getStateMonitor()->getCurrentState()->getLinkState(links[i]->getName())->getGlobalCollisionBodyTransform();
                const shapes::ShapeConstPtr& shape = links[i]->getShape();

                ROS_DEBUG_STREAM(""<<Transform.matrix());

		// Meshes to pointcloud
                if(shape->type == shapes::MESH)
                {
                    const boost::shared_ptr<const shapes::Mesh> mesh = boost::static_pointer_cast<const shapes::Mesh>(shape);
                    for(std::size_t i=0;i<3*mesh->vertex_count;i=i+3)
                    {
                        Eigen::Vector3d vertice(mesh->vertices[i],mesh->vertices[i+1],mesh->vertices[i+2]);
			// Get the point location with respect to the base
                        Eigen::Vector3d vertice_transformed = Transform*vertice;

                        pt.x = vertice_transformed[0];
                        pt.y = vertice_transformed[1];
                        pt.z = vertice_transformed[2];

                        cloud.points.push_back(pt);
                    }
                }

            }
            cloud.header.stamp = ros::Time::now();
	    // PointCloud to PointCloud2
            sensor_msgs::convertPointCloudToPointCloud2(cloud,cloud2);
            cloud_pub.publish(cloud2);

        }else{
            ROS_INFO("Waiting for /joint_state to be broadcast");
        }
    }
    ros::shutdown();
    return 0;
}

 // ObstaclesLayer.h --- 
// 
// Filename: ObstaclesLayer.h
// Description: Dynamic mapping of obstacles with RGBD
//              and Laser sensors
// Author: Federico Boniardi
// Maintainer: boniardi@cs.uni-freiburg.de
// Created: Wed Nov 19 18:57:41 2014 (+0100)
// Version: 0.1.0
// Last-Updated: Fri Dec 5 17:57:27 2014 (+0100)
//           By: Federico Boniardi
//     Update #: 2
// URL: 
// Keywords: 
// Compatibility: 
//   ROS Hydro, ROS Indigo
// 

// Commentary: 
//   The code therein is an improvement of costmap_2d::VoxelLayer which
//   is distributed by the authors under BSD license, that is below reported
//   
//     /*********************************************************************
//      *
//      * Software License Agreement (BSD License)
//      *
//      *  Copyright (c) 2008, 2013, Willow Garage, Inc.
//      *  All rights reserved.
//      *
//      *  Redistribution and use in source and binary forms, with or without
//      *  modification, are permitted provided that the following conditions
//      *  are met:
//      *
//      *   * Redistributions of source code must retain the above copyright
//      *     notice, this list of conditions and the following disclaimer.
//      *   * Redistributions in binary form must reproduce the above
//      *     copyright notice, this list of conditions and the following
//      *     disclaimer in the documentation and/or other materials provided
//      *     with the distribution.
//      *   * Neither the name of Willow Garage, Inc. nor the names of its
//      *     contributors may be used to endorse or promote products derived
//      *     from this software without specific prior written permission.
//      *
//      *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//      *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//      *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//      *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//      *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//      *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//      *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//      *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//      *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//      *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//      *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//      *  POSSIBILITY OF SUCH DAMAGE.
//      *
//      * Author: Eitan Marder-Eppstein
//      *         David V. Lu!!
//      *********************************************************************/
//
//   Tested on: - ROS Hydro on Ubuntu 12.04
//              - ROS Indigo on Ubuntu 14.04
//   RGBD source: ASUS Xtion pro
//      
//      

// Code:

#ifndef SQUIRREL_NAVIGATION_OBSTACLESLAYER_H_
#define SQUIRREL_NAVIGATION_OBSTACLESLAYER_H_

#include <ros/ros.h>
#include <costmap_2d/layer.h>
#include <costmap_2d/layered_costmap.h>
#include <costmap_2d/observation_buffer.h>
#include <costmap_2d/VoxelGrid.h>
#include <nav_msgs/OccupancyGrid.h>
#include <sensor_msgs/LaserScan.h>
#include <laser_geometry/laser_geometry.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <tf/message_filter.h>
#include <message_filters/subscriber.h>
#include <dynamic_reconfigure/server.h>
#include <costmap_2d/VoxelPluginConfig.h>
#include <costmap_2d/obstacle_layer.h>
#include <voxel_grid/voxel_grid.h>

#include <map>
#include <cmath>

namespace squirrel_navigation {

class ObstaclesLayer : public costmap_2d::ObstacleLayer
{
public:
  ObstaclesLayer( void );
  virtual ~ObstaclesLayer( void );
  virtual void onInitialize( void );
  virtual void updateBounds( double, double, double, double*, double*, double*, double* );
  void updateOrigin( double, double );
  bool isDiscretized( void );
  virtual void matchSize( void );
  virtual void reset( void );

protected:
  virtual void setupDynamicReconfigure( ros::NodeHandle& );
  virtual void resetMaps( void );

private:
  void reconfigureCB( costmap_2d::VoxelPluginConfig& , uint32_t );
  void clearNonLethal( double, double, double, double, bool );
  virtual void raytraceFreespace( const costmap_2d::Observation&, double*, double*, double*, double* );

  dynamic_reconfigure::Server<costmap_2d::VoxelPluginConfig> *dsrv_;

  // time based costmap layer
  std::map<unsigned int, ros::Time> clearing_index_stamped_;
  
  ros::Publisher voxel_pub_;
  voxel_grid::VoxelGrid voxel_grid_;
  double z_resolution_, origin_z_;
  unsigned int unknown_threshold_, mark_threshold_, size_z_;
  ros::Publisher clearing_endpoints_pub_;
  sensor_msgs::PointCloud clearing_endpoints_;
  
  double robot_diameter_, robot_height_, platform_height_, tower_diameter_;
  double floor_threshold_,  obstacles_persistence_;
  
  inline bool worldToMap3DFloat( double wx, double wy, double wz, double& mx, double& my, double& mz )
  {
    if (wx < origin_x_ || wy < origin_y_ || wz < origin_z_) {
      return false;
    }

    mx = ((wx - origin_x_) / resolution_);
    my = ((wy - origin_y_) / resolution_);
    mz = ((wz - origin_z_) / z_resolution_);

    if (mx < size_x_ && my < size_y_ && mz < size_z_) {
      return true;
    }
    
    return false;
  }

  inline bool worldToMap3D( double wx, double wy, double wz, unsigned int& mx, unsigned int& my, unsigned int& mz )
  {
    if (wx < origin_x_ || wy < origin_y_ || wz < origin_z_) {
      return false;
    }
    
    mx = (int)((wx - origin_x_) / resolution_);
    my = (int)((wy - origin_y_) / resolution_);
    mz = (int)((wz - origin_z_) / z_resolution_);

    if (mx < size_x_ && my < size_y_ && mz < size_z_) {
      return true;
    }

    return false;
  }

  inline void mapToWorld3D( unsigned int mx, unsigned int my, unsigned int mz, double& wx, double& wy, double& wz )
  {
    wx = origin_x_ + (mx + 0.5) * resolution_;
    wy = origin_y_ + (my + 0.5) * resolution_;
    wz = origin_z_ + (mz + 0.5) * z_resolution_;
  }

  inline double dist( double x0, double y0, double z0, double x1, double y1, double z1 )
  {
    return std::sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0) + (z1-z0)*(z1-z0));
  }
};

}  // namespace squirrel_navigation

#endif  // SQUIRREL_NAVIGATION_OBSTACLESLAYER_H_

// 
// ObstaclesLayer.h ends here

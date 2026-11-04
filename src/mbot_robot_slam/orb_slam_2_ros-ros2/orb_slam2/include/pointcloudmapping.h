/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2016  <copyright holder> <email>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef POINTCLOUDMAPPING_H
#define POINTCLOUDMAPPING_H

#include "System.h"
#include <KeyFrame.h>
#include "Converter.h"
#include <pcl/common/transforms.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <condition_variable>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <opencv2/highgui/highgui.hpp>
typedef pcl::PointXYZRGBA PointT;
typedef pcl::PointCloud<PointT> PointCloud;
namespace ORB_SLAM2
{
class KeyFrame;

class CloudPoint
{
public:
    pcl::PointCloud<PointT>::Ptr pcE;
    Eigen::Isometry3d T;
    int pcID; 
};

class PointCloudMapping
{
public:
    PointCloudMapping( double resolution_,double meank_,double thresh_ );

    // 插入一个keyframe，会更新一次地图
    void insertKeyFrame( KeyFrame* kf, cv::Mat& color, cv::Mat& depth,int idk,vector<KeyFrame*> vpKFs );
    void shutdown();
    void viewer();
    int loopcount = 0;
    vector<KeyFrame*> currentvpKFs;
    bool cloudbusy;
    bool loopbusy;
    void updatecloud();
    bool bStop = false;
    pcl::PointCloud<PointT>::Ptr GetAllCloudPoints();
    bool SaveCloud(const string &filename);
protected:
    pcl::PointCloud<PointT>::Ptr generatePointCloud(KeyFrame* kf, cv::Mat& color, cv::Mat& depth);
    
    pcl::PointCloud<PointT>::Ptr globalMap;
    pcl::PointCloud<PointT>::Ptr globalMapCloud;
    shared_ptr<thread>  viewerThread;   
    
    bool    shutDownFlag    =false;
    mutex   shutDownMutex;  
    
    condition_variable  keyFrameUpdated;
    mutex               keyFrameUpdateMutex;
    vector<CloudPoint>     pointcloud;
    // data to generate point clouds
    vector<KeyFrame*>       keyframes;
    vector<cv::Mat>         colorImgs;
    vector<cv::Mat>         depthImgs;
    vector<cv::Mat>         colorImgks;
    vector<cv::Mat>         depthImgks;
    vector<int>             ids;
    mutex                   keyframeMutex;
    mutex                   mMutexCloud;   
    uint16_t                lastKeyframeSize =0;
    
    double resolution = 0.04;
    double meank = 50;
    double thresh = 1;
    pcl::VoxelGrid<PointT>  voxel;
    pcl::StatisticalOutlierRemoval<PointT> statistical_filter;

};
} // namespace ORB_SLAM2

#endif // POINTCLOUDMAPPING_H

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

#include "pointcloudmapping.h"
#include <pcl/visualization/cloud_viewer.h>

namespace ORB_SLAM2
{
int currentloopcount = 0;
PointCloudMapping::PointCloudMapping( double resolution_,double meank_,double thresh_ )
{
   
    this->resolution = resolution_;
    this->meank = meank_;
    this->thresh = thresh_;
    statistical_filter.setMeanK(meank);
    statistical_filter.setStddevMulThresh(thresh);
    globalMap = std::make_shared<PointCloud>();
    globalMapCloud = std::make_shared<PointCloud>();
    // pcl::PointCloud<PointT>::Ptr globalMap(new pcl::PointCloud<PointT>());
    
    viewerThread = make_shared<thread>( bind(&PointCloudMapping::viewer, this ) );
}



void PointCloudMapping::shutdown()
{
    {
        unique_lock<mutex> lck(shutDownMutex);
        shutDownFlag = true;
        keyFrameUpdated.notify_one();
    }
    viewerThread->join();
}


void PointCloudMapping::insertKeyFrame(KeyFrame* kf, cv::Mat& color, cv::Mat& depth,int idk,vector<KeyFrame*> vpKFs)
{
    //cout<<"receive a keyframe, id = "<<idk<<" 第"<<kf->mnId<<"个"<<endl;
    //cout<<color.size()<<","<<depth.size()<<endl;
    //if(color.empty()) cout<<"depth is empty"<<endl;
    //if(depth.empty()) cout<<"depth is empty"<<endl;
    //cout<<"vpKFs数量"<<vpKFs.size()<<endl;
    unique_lock<mutex> lck(keyframeMutex);
    keyframes.push_back( kf );
    currentvpKFs = vpKFs;
    //colorImgs.push_back( color.clone() );
    //depthImgs.push_back( depth.clone() );
    CloudPoint pointcloude;
    pointcloude.pcID = idk;
    pointcloude.T = ORB_SLAM2::Converter::toSE3Quat( kf->GetPose() );
    pointcloude.pcE = generatePointCloud(kf,color,depth);
    pointcloud.push_back(pointcloude);
    keyFrameUpdated.notify_one();
}

pcl::PointCloud<PointT>::Ptr PointCloudMapping::generatePointCloud(KeyFrame* kf, cv::Mat& color, cv::Mat& depth)//,Eigen::Isometry3d T
{
    PointCloud::Ptr tmp( new PointCloud() );
    // point cloud is null ptr
    for ( int m=0; m<depth.rows; m+=3 )
    { 
        for ( int n=0; n<depth.cols; n+=3 )
        {
            float d = depth.ptr<float>(m)[n];
            if (d < 0.01 || d>15)
                continue;
            PointT p;
            p.z = d;
            p.x = ( n - kf->cx) * p.z / kf->fx;
            p.y = ( m - kf->cy) * p.z / kf->fy;
            
            p.r = color.ptr<uchar>(m)[n*3];
            p.g = color.ptr<uchar>(m)[n*3+1];
            p.b = color.ptr<uchar>(m)[n*3+2];
                
            tmp->points.push_back(p);
        }
    }
    // PointCloud::Ptr cloud_voxel_tem(new PointCloud);
    tmp->is_dense = false;
    // voxel.setInputCloud( tmp );
    // voxel.setLeafSize( resolution, resolution, resolution);
    // voxel.filter( *cloud_voxel_tem );
    // return cloud_voxel_tem;
    // cout<<"generatePointCloud size:"<<tmp->points.size()<<endl;
    return tmp;
}


void PointCloudMapping::viewer()
{
    // pcl::visualization::CloudViewer viewer("viewer");
    cout<<"viewer start"<<endl;
    while(1)
    {
        
        {
            unique_lock<mutex> lck_shutdown( shutDownMutex );
            if (shutDownFlag)
            {
                break;
            }
        }
        {
            unique_lock<mutex> lck_keyframeUpdated( keyFrameUpdateMutex );
            keyFrameUpdated.wait( lck_keyframeUpdated );
        }
        
        // keyframe is updated 
        size_t N=0;
        {
            unique_lock<mutex> lck(keyframeMutex);
            N = keyframes.size();
        }
        // if(loopbusy || bStop)
        // {
        //  cout<<"loopbusy || bStop"<<endl;
        //  continue;
        // }
        // cout<<lastKeyframeSize<<"    "<<N<<endl;
        if(lastKeyframeSize == N){
            cloudbusy = false;
            continue;
        }            
        // cout<<"待处理点云个数 = "<<N<<endl;
        cloudbusy = true;
        PointCloud::Ptr p=nullptr;
        auto mgtmp=std::make_shared<PointCloud>();
        
        for ( size_t i=lastKeyframeSize; i<N ; i++ )
        {
            if(p==nullptr) p =std::make_shared<PointCloud>();
            	else p->clear();
            pcl::transformPointCloud( *(pointcloud[i].pcE), *p, pointcloud[i].T.inverse().matrix());
            if(p->empty()) continue;
            cout<<"point size:"<<p->points.size()<<endl;

            statistical_filter.setInputCloud(p);
            statistical_filter.filter(*mgtmp); 
  
            //cout<<"处理好第i个点云"<<i<<endl;
            {
                unique_lock<mutex> lock(mMutexCloud);
                *globalMap += *mgtmp;
            }}
        // PointCloud::Ptr tmp (new PointCloud);
        // pcl::PointCloud<PointT>::Ptr tmp (new PointCloud);

        voxel.setInputCloud( globalMap );
        voxel.setLeafSize( resolution, resolution, resolution);
        voxel.filter( *globalMapCloud );
        
        // tmp->swap( *globalMap );
        cout<<"show global map, size="<<N<<",point size="<<globalMapCloud->points.size()<<endl;
        lastKeyframeSize = N;
        cloudbusy = false;
    }
}
bool PointCloudMapping::SaveCloud(const string &filename)
{
    
    // cout<<"globalMap save"<<endl;
     {
        unique_lock<mutex> lock(mMutexCloud);
        return pcl::io::savePCDFile(filename+"result.pcd", *globalMapCloud);
     }

    
}

pcl::PointCloud<PointT>::Ptr PointCloudMapping::GetAllCloudPoints()
{
    unique_lock<mutex> lock(mMutexCloud);
    return globalMapCloud;
}

void PointCloudMapping::updatecloud()
{
	if(!cloudbusy)
	{
		loopbusy = true;
		cout<<"startloopmappoint"<<endl;
        PointCloud::Ptr tmp1(new PointCloud);
		for (int i=0;i<currentvpKFs.size();i++)
		{
		    for (int j=0;j<pointcloud.size();j++)
		    {   
				if(pointcloud[j].pcID==currentvpKFs[i]->mnFrameId) 
				{   
					Eigen::Isometry3d T = ORB_SLAM2::Converter::toSE3Quat(currentvpKFs[i]->GetPose() );
					PointCloud::Ptr cloud(new PointCloud);
					pcl::transformPointCloud( *pointcloud[j].pcE, *cloud, T.inverse().matrix());
					*tmp1 +=*cloud;

					cout<<"第pointcloud"<<j<<"与第vpKFs"<<i<<"匹配"<<endl;
					continue;
				}
			}
		}
        cout<<"finishloopmap"<<endl;
        PointCloud::Ptr tmp2(new PointCloud());
        voxel.setInputCloud( tmp1 );
        voxel.setLeafSize( resolution, resolution, resolution);
        voxel.filter( *tmp2 );
        globalMap->swap( *tmp2 );
        //viewer.showCloud( globalMap );
        loopbusy = false;
        //cloudbusy = true;
        loopcount++;

        //*globalMap = *tmp1;
	}
}
}//namespace ORB_SLAM


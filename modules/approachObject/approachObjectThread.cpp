/*
 * Copyright (C) 2006-2020 Istituto Italiano di Tecnologia (IIT)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "approachObjectThread.h"


YARP_LOG_COMPONENT(APPROACH_OBJECT_THREAD, "r1_obr.approachObject.approachObjectThread")


/****************************************************************/
ApproachObjectThread::ApproachObjectThread(double _period, yarp::os::ResourceFinder &rf):
    PeriodicThread(_period),
    m_rf(rf),
    m_ext_start(false),
    m_ext_stop(false),
    m_ext_resume(false)
{
    m_gaze_target_port_name         = "/approachObject/gaze_target:o";
    m_object_finder_rpc_port_name   = "/approachObject/object_finder/rpc";
    m_output_coordinates_port_name  = "/approachObject/output_coords:o";
    m_object_finder_result_port_name= "/approachObject/object_finder_result:i";
    m_camera_frame_id   = "depth_center";
    m_base_frame_id     = "base_link";
    m_world_frame_id    = "map";
    m_safe_distance     = 1.0;
    m_wait_for_search   = 4.0;
}


/****************************************************************/
bool ApproachObjectThread::threadInit()
{
    // ------------ Generic config ------------ //
    if(m_rf.check("camera_frame_id"))   {m_camera_frame_id = m_rf.find("camera_frame_id").asString();}
    if(m_rf.check("base_frame_id"))     {m_base_frame_id = m_rf.find("base_frame_id").asString();}
    if(m_rf.check("world_frame_id"))    {m_world_frame_id = m_rf.find("world_frame_id").asString();}
    if(m_rf.check("safe_distance"))     {m_safe_distance = m_rf.find("safe_distance").asFloat32();}
    if(m_rf.check("wait_for_search"))   {m_wait_for_search = m_rf.find("wait_for_search").asFloat32();}


    // ------------ Open ports ------------ //
    if(m_rf.check("gaze_target_port")) {m_gaze_target_port_name = m_rf.find("gaze_target_port").asString();}
    if(!m_gaze_target_port.open(m_gaze_target_port_name))
        yCError(APPROACH_OBJECT_THREAD) << "Cannot open port" << m_gaze_target_port_name; 
    else
        yCInfo(APPROACH_OBJECT_THREAD) << "opened port" << m_gaze_target_port_name;


    if(m_rf.check("object_finder_rpc_port")) {m_object_finder_rpc_port_name = m_rf.find("object_finder_rpc_port").asString();}
    if(!m_object_finder_rpc_port.open(m_object_finder_rpc_port_name))
        yCError(APPROACH_OBJECT_THREAD) << "Cannot open port" << m_object_finder_rpc_port_name; 
    else
        yCInfo(APPROACH_OBJECT_THREAD) << "opened port" << m_object_finder_rpc_port_name;


    if(m_rf.check("object_finder_result_port")) {m_object_finder_result_port_name = m_rf.find("object_finder_result_port").asString();}
    if(!m_object_finder_result_port.open(m_object_finder_result_port_name))
        yCError(APPROACH_OBJECT_THREAD) << "Cannot open port" << m_object_finder_result_port_name; 
    else
        yCInfo(APPROACH_OBJECT_THREAD) << "opened port" << m_object_finder_result_port_name;


    if(m_rf.check("output_coordinates_port")) {m_output_coordinates_port_name = m_rf.find("output_coordinates_port").asString();}
    if(!m_output_coordinates_port.open(m_output_coordinates_port_name))
        yCError(APPROACH_OBJECT_THREAD) << "Cannot open port" << m_output_coordinates_port_name; 
    else
        yCInfo(APPROACH_OBJECT_THREAD) << "opened port" << m_output_coordinates_port_name;
    
    
    // ------------ TransformClient config ------------ //
    Property tcProp;
    //default
    tcProp.put("device", "frameTransformClient");
    tcProp.put("ft_client_prefix", "/approachObject");
    tcProp.put("local_rpc", "/approachObject/ftClient.rpc");
    bool okTransformRf = m_rf.check("TRANSFORM_CLIENT");
    if(!okTransformRf)
    {
        yCWarning(APPROACH_OBJECT_THREAD,"TRANSFORM_CLIENT section missing in ini file Using default values");
        tcProp.put("filexml_option","ftc_yarp_only.xml");
    }
    else {
        Searchable &tf_config = m_rf.findGroup("TRANSFORM_CLIENT");
        if (tf_config.check("ft_client_prefix")) {
            tcProp.put("ft_client_prefix", tf_config.find("ft_client_prefix").asString());
        }
        if (tf_config.check("ft_server_prefix")) {
            tcProp.put("ft_server_prefix", tf_config.find("ft_server_prefix").asString());
        }
        if(tf_config.check("filexml_option") && !(tf_config.check("testxml_from") || tf_config.check("testxml_context")))
        {
            tcProp.put("filexml_option", tf_config.find("filexml_option").asString());
        }
        else if(!tf_config.check("filexml_option") && (tf_config.check("testxml_from") && tf_config.check("testxml_context")))
        {
            tcProp.put("testxml_from", tf_config.find("testxml_from").asString());
            tcProp.put("testxml_context", tf_config.find("testxml_context").asString());
        }
        else
        {
            yCError(APPROACH_OBJECT_THREAD,"TRANSFORM_CLIENT is missing information about the frameTransformClient device configuration. Check your config. RETURNING");
            return false;
        }
    }
    m_tcPoly.open(tcProp);
    if(!m_tcPoly.isValid())
    {
        yCError(APPROACH_OBJECT_THREAD,"Error opening PolyDriver check parameters");
        return false;
    }
    m_tcPoly.view(m_iTc);
    if(!m_iTc)
    {
        yCError(APPROACH_OBJECT_THREAD,"Error opening iFrameTransform interface. Device not available");
        return false;
    }


    // --------- Navigation2DClient config --------- //
    Property nav2DProp;
    if(!m_rf.check("NAVIGATION_CLIENT"))
    {
        yCWarning(APPROACH_OBJECT_THREAD,"NAVIGATION_CLIENT section missing in ini file. Using the default values");
    }
    Searchable& nav_config = m_rf.findGroup("NAVIGATION_CLIENT");
    nav2DProp.put("device", nav_config.check("device", Value("navigation2D_nwc_yarp")));
    nav2DProp.put("local", nav_config.check("local", Value("/approachObject/navClient/")));
    nav2DProp.put("navigation_server", nav_config.check("navigation_server", Value("/navigation2D_nws_yarp")));
    nav2DProp.put("map_locations_server", nav_config.check("map_locations_server", Value("/map2D_nws_yarp")));
    nav2DProp.put("localization_server", nav_config.check("localization_server", Value("/localization2D_nws_yarp")));
    
    m_nav2DPoly.open(nav2DProp);
    if(!m_nav2DPoly.isValid())
    {
        yCWarning(APPROACH_OBJECT_THREAD,"Error opening PolyDriver check parameters. Using the default values");
    }
    m_nav2DPoly.view(m_iNav2D);
    if(!m_iNav2D){
        yCError(APPROACH_OBJECT_THREAD,"Error opening INavigation2D interface. Device not available");
        return false;
    }


    // --------- RGBDSensor config --------- //
    Property rgbdProp;
    // Prepare default prop object
    rgbdProp.put("device",          "RGBDSensorClient");
    rgbdProp.put("localImagePort",  "/approachObject/clientRgbPort:i");
    rgbdProp.put("localDepthPort",  "/approachObject/clientDepthPort:i");
    rgbdProp.put("localRpcPort",    "/approachObject/clientRpcPort");
    rgbdProp.put("remoteImagePort", "/SIM_CER_ROBOT/depthCamera/rgbImage:o");
    rgbdProp.put("remoteDepthPort", "/SIM_CER_ROBOT/depthCamera/depthImage:o");
    rgbdProp.put("remoteRpcPort",   "/SIM_CER_ROBOT/depthCamera/rpc:i");
    rgbdProp.put("ImageCarrier",    "mjpeg");
    rgbdProp.put("DepthCarrier",    "fast_tcp");
    bool okRgbdRf = m_rf.check("RGBD_SENSOR_CLIENT");
    if(!okRgbdRf)
    {
        yCWarning(APPROACH_OBJECT_THREAD,"RGBD_SENSOR_CLIENT section missing in ini file. Using default values");
    }
    else
    {
        Searchable& rgbd_config = m_rf.findGroup("RGBD_SENSOR_CLIENT");
        if(rgbd_config.check("device")) {rgbdProp.put("device", rgbd_config.find("device").asString());}
        if(rgbd_config.check("localImagePort")) {rgbdProp.put("localImagePort", rgbd_config.find("localImagePort").asString());}
        if(rgbd_config.check("localDepthPort")) {rgbdProp.put("localDepthPort", rgbd_config.find("localDepthPort").asString());}
        if(rgbd_config.check("localRpcPort")) {rgbdProp.put("localRpcPort", rgbd_config.find("localRpcPort").asString());}
        if(rgbd_config.check("remoteImagePort")) {rgbdProp.put("remoteImagePort", rgbd_config.find("remoteImagePort").asString());}
        if(rgbd_config.check("remoteDepthPort")) {rgbdProp.put("remoteDepthPort", rgbd_config.find("remoteDepthPort").asString());}
        if(rgbd_config.check("remoteRpcPort")) {rgbdProp.put("remoteRpcPort", rgbd_config.find("remoteRpcPort").asString());}
        if(rgbd_config.check("ImageCarrier")) {rgbdProp.put("ImageCarrier", rgbd_config.find("ImageCarrier").asString());}
        if(rgbd_config.check("DepthCarrier")) {rgbdProp.put("DepthCarrier", rgbd_config.find("DepthCarrier").asString());}
    }

    m_rgbdPoly.open(rgbdProp);
    if(!m_rgbdPoly.isValid())
    {
        yCError(APPROACH_OBJECT_THREAD,"Error opening PolyDriver check parameters");
        return false;
    }
    m_rgbdPoly.view(m_iRgbd);
    if(!m_iRgbd)
    {
        yCError(APPROACH_OBJECT_THREAD,"Error opening iRGBD interface. Device not available");
        return false;
    }
   
    //get parameters data from the camera
    bool propintr  = m_iRgbd->getDepthIntrinsicParam(m_propIntrinsics);
    if(!propintr){
        return false;
    }
    yCInfo(APPROACH_OBJECT_THREAD) << "Depth Intrinsics:" << m_propIntrinsics.toString();
    m_intrinsics.fromProperty(m_propIntrinsics);


    return true;
}


/****************************************************************/
void ApproachObjectThread::threadRelease()
{
    if(m_tcPoly.isValid())
        m_tcPoly.close();
    
    if(m_nav2DPoly.isValid())
        m_nav2DPoly.close();
    
    if(m_rgbdPoly.isValid())
        m_rgbdPoly.close();
    
    if (m_object_finder_rpc_port.asPort().isOpen())
        m_object_finder_rpc_port.close();   

    if (!m_output_coordinates_port.isClosed())
        m_output_coordinates_port.close(); 
}


/****************************************************************/
void ApproachObjectThread::exec(Bottle& b) 
{
    m_object = b.get(0).asString();
    m_coords = b.get(1).asList();

    m_ext_start = true;
}   


/****************************************************************/
void ApproachObjectThread::run() 
{  
    if (m_ext_start && !m_ext_stop)
    {
        Map2DLocation loc;
        m_iNav2D->getCurrentPosition(loc);
        yCDebug(APPROACH_OBJECT_THREAD,) << "Current location:"<< loc.toString();
        
        yCInfo(APPROACH_OBJECT_THREAD,"Calculating approaching position");
        if (m_coords->size() == 2) //pixel coordinates in camera ref frame
        {
            double u = m_coords->get(0).asFloat32();
            double v = m_coords->get(1).asFloat32();

            //get depth image from camera
            ImageOf<float>  depth_image;  
            bool depth_ok = m_iRgbd->getDepthImage(depth_image);
            if (!depth_ok)
            {
                yCError(APPROACH_OBJECT_THREAD, "getDepthImage failed");
                m_ext_start = false;
                return;
            }
            if (depth_image.getRawImage()==nullptr)
            {
                yCError(APPROACH_OBJECT_THREAD, "invalid image received");
                m_ext_start = false;
                return;
            }

            //transforming pixel coordinates in space coordinates wrt camera frame
            Vector tempPoint(4,1.0);
            tempPoint[0] = (u - m_intrinsics.principalPointX) / m_intrinsics.focalLengthX * depth_image.pixel(u, v);
            tempPoint[1] = (v - m_intrinsics.principalPointY) / m_intrinsics.focalLengthY * depth_image.pixel(u, v);
            tempPoint[2] = depth_image.pixel(u, v);

            //computing the transformation matrix from the camera to the base reference frame
            Matrix transform_mtrx;
            bool base_frame_exists = m_iTc->getTransform(m_camera_frame_id, m_base_frame_id, transform_mtrx);
            if (!base_frame_exists)
            {
                yCError(APPROACH_OBJECT_THREAD, "unable to found transformation matrix (base)");
                m_ext_start = false;
                return;
            }

            //point in space coordinates in base ref frame
            Vector v_object_base = transform_mtrx*tempPoint;

            //define target at safe distance from point (on a line connecting robot and point) in base ref frame
            Vector v_target_base = v_object_base;
            v_target_base[0] = v_object_base[0] - m_safe_distance*cos(atan2(v_object_base[1], v_object_base[0]));
            v_target_base[1] = v_object_base[1] - m_safe_distance*sin(atan2(v_object_base[1], v_object_base[0]));

            //define navigation target in world ref frame
            bool world_frame_exists = m_iTc->getTransform(m_base_frame_id, m_world_frame_id, transform_mtrx);
            if (!world_frame_exists)
            {
                yCError(APPROACH_OBJECT_THREAD, "unable to found transformation matrix (world)");
                m_ext_start = false;
                return;
            }
 
            Vector v_target_world = transform_mtrx*v_target_base; //robot target position in world ref frame
            Vector v_object_world = transform_mtrx*v_object_base; //position of the object in world ref frame

            loc.theta = atan2((v_object_world[1]-loc.y), (v_object_world[0]-loc.x)) / M_PI * 180;
            loc.x = v_target_world[0];
            loc.y = v_target_world[1];
        }
        else if (m_coords->size() == 3) //absolute position of object
        {
            Vector v_object_world(2,1.0);
            v_object_world[0] = m_coords->get(0).asFloat32();
            v_object_world[1] = m_coords->get(1).asFloat32();
            
            double theta_rad = atan2((v_object_world[1]-loc.y), (v_object_world[0]-loc.x));
            loc.theta = theta_rad / M_PI * 180;
            loc.x = v_object_world[0] - m_safe_distance*cos(theta_rad);
            loc.y = v_object_world[1] - m_safe_distance*sin(theta_rad);
        }

        //navigation to target
        yCInfo(APPROACH_OBJECT_THREAD,"Approaching object");
        yCDebug(APPROACH_OBJECT_THREAD,) << "Approach location:"<< loc.toString();
        m_iNav2D->gotoTargetByAbsoluteLocation(loc);

        NavigationStatusEnum currentStatus;
        m_iNav2D->getNavigationStatus(currentStatus);
        while (currentStatus != navigation_status_goal_reached  && !m_ext_stop  )
        {
            Time::delay(0.2);
            m_iNav2D->getNavigationStatus(currentStatus);
        }

        //look again for object
        if (!m_ext_stop)
        {
            yCInfo(APPROACH_OBJECT_THREAD,"Approaching location reached. Looking for object again");
            if(lookAgain(m_object))
            {
                Bottle* finderResult = m_object_finder_result_port.read(false); 
                if(finderResult  != nullptr && !m_ext_stop)
                {
                    Bottle* new_coords = new Bottle;
                    if (!getObjCoordinates(finderResult, new_coords))
                    {
                        yCWarning(APPROACH_OBJECT_THREAD, "The Object Finder is not seeing any %s", m_object.c_str());
                    }
                    else
                    {
                        yCInfo(APPROACH_OBJECT_THREAD,"Object approached");
                        Bottle&  toSend = m_output_coordinates_port.prepare();
                        toSend.clear();
                        toSend = *new_coords;
                        m_output_coordinates_port.write();
                    }
                             
                }
            }
            else
            {
                yCError(APPROACH_OBJECT_THREAD,"I cannot find the object again");
            }
        }   
    } 
    else if (m_ext_resume && !m_ext_stop)
    {
        if (!m_ext_stop)
        {
            yCInfo(APPROACH_OBJECT_THREAD,"Looking for object again");
            if(lookAgain(m_object))
            {
                Bottle* finderResult = m_object_finder_result_port.read(false); 
                if(finderResult  != nullptr)
                {
                    m_coords = new Bottle;
                    if (!getObjCoordinates(finderResult, m_coords))
                    {
                        yCWarning(APPROACH_OBJECT_THREAD, "The Object Finder is not seeing any %s", m_object.c_str());
                    }
                    else
                    {
                        m_ext_start = true;
                    }
                }
                else
                {
                    yCError(APPROACH_OBJECT_THREAD,"Error getting new object coordinates");
                    m_ext_resume = false;
                }
            }
            else
            {
                yCError(APPROACH_OBJECT_THREAD,"I cannot find the object again");
            }
        }
    }


    if (m_ext_start && !m_ext_resume)
        m_ext_start = false;

    if (m_ext_stop)
        m_ext_stop = false;

    if (m_ext_resume)
        m_ext_resume = false;
}


/****************************************************************/
bool ApproachObjectThread::lookAgain(string object ) 
{
    vector<pair<double,double>> head_positions = {
        {0.0,  0.0 }  ,   //front
        {0.0,  20.0}  ,   //up
        {0.0, -20.0}  ,   //down
        {35.0, 0.0 }  ,   //left
        {-35.0,0.0 }  };  //right

    for (int i=0; i<(int)head_positions.size(); i++)
    {                        
        if(m_ext_stop) break;

        yarp::os::Bottle&  toSend1 = m_gaze_target_port.prepare();
        toSend1.clear();
        yarp::os::Bottle& targetTypeList = toSend1.addList();
        targetTypeList.addString("target-type");
        targetTypeList.addString("angular");
        yarp::os::Bottle& targetLocationList = toSend1.addList();
        targetLocationList.addString("target-location");
        yarp::os::Bottle& targetList = targetLocationList.addList();
        targetList.addFloat32(head_positions[i].first);
        targetList.addFloat32(head_positions[i].second);
        m_gaze_target_port.write(); //sending output command to gaze-controller 
        
        yarp::os::Time::delay(m_wait_for_search);  //waiting for the robot to tilt its head

        //search for object
        Bottle request, reply;
        request.addString("where");
        request.addString(object); 
        if (m_object_finder_rpc_port.write(request,reply))
        {
            if (reply.get(0).asString()!="not found")
            {
                yarp::os::Time::delay(0.5);
                return true;
            }
        }
        else
        {
            yCError(APPROACH_OBJECT_THREAD,"Unable to communicate with object finder");
            return false;
        }
        
    }

    return false;
}


/****************************************************************/
bool ApproachObjectThread::getObjCoordinates(Bottle* btl, Bottle* out)
{
    double max_conf = 0.0;
    double x = -1.0, y;
    for (int i=0; i<btl->size(); i++)
    {
        Bottle* b = btl->get(i).asList();
        if (b->get(0).asString() != m_object) //skip objects with another label
            continue;
        
        if(b->get(1).asFloat32() > max_conf) //get the object with the max confidence
        {
            max_conf = b->get(1).asFloat32();
            x = b->get(2).asFloat32();
            y = b->get(3).asFloat32();
        }
    }
    if (x<0)
        return false;
    
    out->addFloat32(x);
    out->addFloat32(y);
    return true;
}

/****************************************************************/
bool ApproachObjectThread::externalStop()
{
    yCInfo(APPROACH_OBJECT_THREAD,"Stopping approaching actions");
    m_ext_stop = true;

    NavigationStatusEnum currentStatus;
    m_iNav2D->getNavigationStatus(currentStatus);
    if (currentStatus == navigation_status_moving)
        m_iNav2D->stopNavigation();

    return true;

} 


/****************************************************************/
bool ApproachObjectThread::externalResume()
{
    yCInfo(APPROACH_OBJECT_THREAD,"Resuming approaching actions");
    m_ext_stop = false;
    m_ext_resume = true;

    return true;
}
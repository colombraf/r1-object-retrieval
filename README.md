# r1-Object-Retrieval

Repository to manage the work on the object retrieval with the robot R1.
   
# Modules
You can find here a list of the main modules in this repo and a brief description of their purpose

| Module                                                | Description                                                           |
| ----------------------------------------------------- | --------------------------------------------------------------------- |
| [nextLocPlanner](modules/nextLocPlanner/README.md) | Planner that define the next location the robot should inspect        |
| [lookForObject](modules/lookForObject/README.md) | Module that manages the motions the robot should perfom once arrived in a location to search for the requested object (tilt head and turn)        |
| [goAndFindIt](modules/goAndFindIt/README.md) | FSM that invokes the two previous modules to navigate the robot in the map and search for objects |
| [approachObject](modules/approachObject/README.md) | Plug-in module for the search positive outcome: the robot navigates nearer to the found object  |
| [disappointedPose](modules/disappointmentPose/README.md) | Plug-in module for the search negative outcome: the robot assumes a predefined pose  |
| [yarpMdetr](modules/yarpMdetr/README.MD) / [yarpYolo](modules/yarpYolo/README.MD) | Object Detection modules: detect objects in an input image |
| [r1Obr-orchestrator](modules/r1Obr-orchestrator/README.md) | Orchestrator of the whole object retrieval application: manages the search, the speech interaction, the integration of the sensor network, and other possible actions that the robot could perform|
| [sensorNetworkReceiver](modules/sensorNetworkReceiver/README.md) | Interaction module between the r1Obr-orchestrator and the Sensor Network |
| [look_and_point](modules/look_and_point/README.md) | Just a simple module to use the positive output of the search and give it as an iput to the handPointing module (cer repo) |
| [micActivation](modules/micActivation/)| Module that starts/stops the audioRecorder device when a joystick button is pressed/released |

# Requirements
* [yarp](https://github.com/robotology/yarp)
* [icub-main](https://github.com/robotology/icub-main.git)
* [icub-contrib](https://github.com/robotology/icub-contrib-common.git)
* [cer](docker pull colombraf/r1images:r1ObjectRetrieval)
* [yarp-devices-llm](https://github.com/robotology/yarp-devices-llm)
* [tour-guide-robot](https://github.com/colombraf/tour-guide-robot)

# Installation
### Using Docker
Build your docker container using the provided Dockerfiles in the [docker](docker) folder, or just pull the images:
```bash
docker pull colombraf/r1images:r1ObjectRetrieval      # to work with the actual R1 robot
docker pull colombraf/r1images:r1ObjectRetrievalSim   # to work in a simulated environment
```

### From source
* On Linux:
```bash
git clone https://github.com/colombraf/r1-object-retrieval 
cd r1-object-retrieval && mkdir build && cd build && cmake .. && make -j11
export PYTHONPATH=$PYTHONPATH:${user1_home}/r1-object-retrieval
export YARP_DATA_DIRS=$YARP_DATA_DIRS:${user1_home}/r1-object-retrieval/build/share/R1_OBJECT_RETRIEVAL
export PATH=${PATH}:${user1_home}/.local/bin
export PATH=$PATH:${user1_home}/r1-object-retrieval/build/bin
```


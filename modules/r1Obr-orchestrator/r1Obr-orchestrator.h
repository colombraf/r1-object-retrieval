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

#ifndef R1OBR_ORCHESTRATOR_H
#define R1OBR_ORCHESTRATOR_H

#include <yarp/os/all.h>
#include <yarp/dev/AudioPlayerStatus.h>

#include "orchestratorThread.h"
#include "speechSynthesizer.h"
#include "storyTeller.h"

using namespace yarp::os;
using namespace yarp::dev;
using namespace std;

class Orchestrator : public RFModule, public TypedReaderCallback<Bottle>
{
private:

    //Callback thread
    OrchestratorThread*         m_inner_thread;

    //RPC Server
    RpcServer                   m_rpc_server_port;
    string                      m_rpc_server_port_name;
    
    //Input command port
    BufferedPort<Bottle>        m_input_port;
    string                      m_input_port_name;
    
    //Feedback port from positive outcome of the search
    BufferedPort<Bottle>        m_positive_feedback_port;
    string                      m_positive_feedback_port_name;

    //Status port
    BufferedPort<Bottle>        m_status_port;
    string                      m_status_port_name;

    //additional Speech Synthesizer
    SpeechSynthesizer*          m_additional_speaker;

    //storyTeller
    StoryTeller*                m_story_teller;

    //Others
    double                      m_period;
    RpcClient                   m_audiorecorderRPCPort;
    BufferedPort<Bottle>        m_audioPlayPort;

public:
    Orchestrator();
    virtual bool configure(ResourceFinder &rf);
    virtual bool close();
    virtual double getPeriod();
    virtual bool updateModule();

    //Port inherited from TypedReaderCallback
    using TypedReaderCallback<Bottle>::onRead;
    void onRead(Bottle& b) override;

    bool respond(const Bottle &cmd, Bottle &reply);
    bool say(string toSay);
};

#endif

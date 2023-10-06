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

#include "chatBot.h"

YARP_LOG_COMPONENT(CHAT_BOT_ORCHESTRATOR, "r1_obr.orchestrator.chatBot")


// ****************************************************** //
bool ChatBot::configure(ResourceFinder &rf)
{
    // Defaults
    string voiceCommandPortName     = "/r1Obr-orchestrator/voice_command:i";
    string chatBotRPCPortName       = "/r1Obr-orchestrator/chatBot:rpc";
    string device_chatBot_nwc       = "chatBot_nwc_yarp";
    string local_chatBot_nwc        = "/r1Obr-orchestrator/chatBot";
    string remote_chatBot_nwc       = "/chatBot_nws";
    string m_language_chatbot       = "it-IT";

    // ------------------  out ------------------ //  
    if(!m_chatBotRPCPort.open(chatBotRPCPortName))
    {
        yCError(CHAT_BOT_ORCHESTRATOR, "Unable to open Chat Bot RPC port");
        return false;
    }

    // ------------------  in  ------------------ //
    if(!rf.check("CHAT_BOT"))
    {
        yCWarning(CHAT_BOT_ORCHESTRATOR,"CHAT_BOT section missing in ini file. Using the default values");
    }
    
    Searchable& config = rf.findGroup("CHAT_BOT");
    if(config.check("voice_command_port")) {voiceCommandPortName = config.find("voice_command_port").asString();}
    
    // Voice Command Port
    if (!m_voiceCommandPort.open(voiceCommandPortName))
    {
        yCError(CHAT_BOT_ORCHESTRATOR, "Unable to open voice command port");
        return false;
    }
    m_voiceCommandPort.useCallback(*this);

    // iChatBot
    m_chatBot_active = config.check("chatbot_active") ? !(config.find("chatbot_active").asString() == "false") : true;
    
    if(m_chatBot_active)
    {
        Property chatBotProp;
        chatBotProp.put("device", config.check("device", Value(device_chatBot_nwc)));
        chatBotProp.put("local",  config.check("local", Value(local_chatBot_nwc)));
        chatBotProp.put("remote", config.check("remote", Value(remote_chatBot_nwc)));
        m_PolyCB.open(chatBotProp);
        if(!m_PolyCB.isValid())
        {
            yCWarning(CHAT_BOT_ORCHESTRATOR,"Error opening PolyDriver check parameters. Using the default values");
        }
        m_PolyCB.view(m_iChatBot);
        if(!m_iChatBot){
            yCError(CHAT_BOT_ORCHESTRATOR,"Error opening IChatBot interface. Device not available");
            return false;
        }
        
        m_iChatBot->resetBot();
        string msgIn,msgOut;
        m_iChatBot->interact(msgIn = "skip_language_set", msgOut);
        yCInfo(CHAT_BOT_ORCHESTRATOR,"ChatBot: wrote: %s . replied: %s",msgIn.c_str(),msgOut.c_str());
        
        if(config.check("language")) 
            m_language_chatbot = config.find("language").asString();
        m_iChatBot->setLanguage(m_language_chatbot);
        

    }

    // --------- SpeechSynthesizer config --------- //
    m_speaker = new SpeechSynthesizer();
    if(!m_speaker->configure(rf, ""))
    {
        yCError(CHAT_BOT_ORCHESTRATOR,"SpeechSynthesizer configuration failed");
        return false;
    }

    return true;
}

    
// ****************************************************** //
void ChatBot::close()
{    
    if(!m_voiceCommandPort.isClosed())
        m_voiceCommandPort.close();

    if (m_chatBotRPCPort.asPort().isOpen())
        m_chatBotRPCPort.close(); 
    
    if(m_PolyCB.isValid())
        m_PolyCB.close();

    m_speaker->close();
    delete m_speaker;
}


// ****************************************************** //
void ChatBot::onRead(Bottle& b)
{
    if(!m_chatBot_active)
    {
        yCWarning(CHAT_BOT_ORCHESTRATOR, "Chat Bot not active. Use RPC port to write commands");
        return;
    }

    string b_str = b.toString();

    if(b_str == "")
        return;

    interactWithChatBot(b_str);
    
}


// ****************************************************** //
void ChatBot::interactWithChatBot(const string& msgIn)
{
    if(m_chatBot_active)
    {
        yCInfo(CHAT_BOT_ORCHESTRATOR,"ChatBot received: %s",msgIn.c_str());
        
        string msgOut;
        m_iChatBot->interact(msgIn, msgOut); //msgOut should be something like "(say <....>) (<other command>)"
        yCInfo(CHAT_BOT_ORCHESTRATOR,"ChatBot replied: %s",msgOut.c_str());

        Bottle msg_btl; msg_btl.fromString(msgOut);

        for (int i=0; i<(int)msg_btl.size(); i++)
        {     
            Bottle* cmd=msg_btl.get(i).asList();
            
            if(cmd->get(0).asString()=="dialogRestarted")
            {
                m_iChatBot->resetBot();
                m_iChatBot->setLanguage(m_language_chatbot);
                Bottle req{"reset_home"}, rep;
                m_chatBotRPCPort.write(req,rep);
                yCInfo(CHAT_BOT_ORCHESTRATOR, "Replied from orchestrator: %s", rep.toString().c_str() );
            }
            else if(cmd->get(0).asString()=="say")
            {
                string toSay = cmd->tail().toString();
                yCInfo(CHAT_BOT_ORCHESTRATOR, "Saying: %s", toSay.c_str());
                m_speaker->say(toSay);
            }
            else if(cmd->get(0).asString()=="setLanguage")
            {
                string lang = cmd->get(1).asString();
                m_iChatBot->setLanguage(lang);
                m_speaker->setLanguage(lang);
                m_language_chatbot=lang;
                yCInfo(CHAT_BOT_ORCHESTRATOR, "Language set to: %s", lang.c_str());
            }
            else
            {
                Bottle reply;
                m_chatBotRPCPort.write(msg_btl,reply);
                yCInfo(CHAT_BOT_ORCHESTRATOR, "Replied from orchestrator: %s", reply.toString().c_str() );
            }
        }
    }
    else
        yCWarning(CHAT_BOT_ORCHESTRATOR, "Chat Bot not active. Use RPC port to write commands");
        
}
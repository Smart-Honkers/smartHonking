/*
 * neighborDiscovery.cc
 *
 *
 *
 *  Created on: Oct 13, 2020
 *      Author: Sharath Baliga
 *
 *
 */

#include <omnetpp.h>
#include "inet/smartHonk/source/ambulance/ambulance.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/lifecycle/NodeOperations.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include <fstream>


namespace inet{

Define_Module(ambulance);

/*
* Purpose:This function initializes the parameters required for initial condition in the network
*
* Functionality: To setup the nodes with required parameter before it starts. The first node is set as
*                transmitter and then other nodes are receiver until it receives a message from its neighbor
*                Reception of a message triggers other host to transmit message.
*/
void ambulance::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numSent = 0;
        numReceived = 0;
        WATCH(numSent);
        WATCH(numReceived);
        listModified = false;
        localPort = par("localPort");
        destPort = par("destPort");
        startTime = par("startTime");
        stopTime = par("stopTime");
        setReceptionGap= par("setReceptionGap");
        startIndex = par("startIndex");
        packetName = getParentModule()->getName();
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");


        /* Initailizing only one node with transmission in the start and others as reception, just to avoid many transmissions
         * in the start. so all hosts go with reception mode except one node in the start
         */

        localIndex = getParentModule()->getIndex();
        if (localIndex == 0)
        {
            startNode = true;
        }
        //char packetTitle[10];
        //sprintf(packetTitle,"%s[%d]",getParentModule()->getName(),getParentModule()->getIndex());
       // packetName = packetTitle;
        if (startNode == true)
        {
            EV_INFO<<"Initializing for the host in starthosts category"<<endl;
            selfMsg = new cMessage("sendTimer");
            transmitPerm = true;
        }
        else
        {
            EV_INFO<<"Initializing for the host in other hosts category"<<endl;
            selfMsg = new cMessage("UDPSinkTimer");
           // selfMsg = new cMessage("sendTimer");
            transmitPerm = false;
            otherNode = true;
            discovered = false;
        }


    }
}

/*
 * This function is called when the simulation is ended
 */
void ambulance::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    if(startNode == true)
    {
        char resultsheet[100];
        sprintf(resultsheet,"Network_Discovery_host[%d].csv",localIndex);
        std::ofstream output(resultsheet,std::ios::out);
        output << "Network Discovery Protocol Resulted with " << neighborAddresses.size() << " members" <<endl;
        output << "Host Address,Host Index,Radial Distance(m)"<<endl;

        std::set<int>::iterator itIndex;
           /* for (itIndex=neighborIndexList.begin();itIndex != neighborIndexList.end() ;itIndex++)
            {
                output << *itIndex;
                output << ",";
            }
            output <<endl;
            output <<"Radial Distance,";
            for (int i=0; i<100;i++)
            {

                output << hostDistanceList[i];
                output <<",";
            }
            */
            std::set<L3Address>::iterator itAddress;
            for (itAddress=neighborAddresses.begin();itAddress != neighborAddresses.end() ;itAddress++)
            {
                cModule *mod = L3AddressResolver().findHostWithAddress(L3Address(*itAddress));
                int resultIndex = mod->getIndex();
                output << *itAddress;
                output << ",";
                output << resultIndex;
                output << ",";
                output << hostDistanceList[resultIndex];
                output << endl;
            }
            output.close();
            // Command to open the spreadsheet
            system(resultsheet);
    }
    ApplicationBase::finish();
}

/*
 * This function useful to set up the socket of the hosts. Joining the multicast group is done here
 */
void ambulance::setSocketOptions()
{
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);


    int typeOfService = par("typeOfService");
    if (typeOfService != -1)
        socket.setTypeOfService(typeOfService);

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0]) {
        IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie)
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    }

      bool receiveBroadcast = par("receiveBroadcast");
      if (receiveBroadcast)
        socket.setBroadcast(true);

    bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
    if (joinLocalMulticastGroups) {
        MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
        socket.joinLocalMulticastGroups(mgl);
    }

   // socket.joinMulticastGroup(multicastAddress);
    socket.setCallback(this);
}


/*
 * This function is used to set up a udp packet and send it to the required address
 */
void ambulance::sendPacket()
{
    std::ostringstream str;
    str << packetName <<"["<<getParentModule()->getIndex()<<"]"<< "-" << numSent;
    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(numSent);
    auto creationTimeTag = payload->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());
    packet->insertAtBack(payload);
    emit(packetSentSignal, packet);
    socket.sendTo(packet, nodeDiscAddress, destPort);
    numSent++;

}



void ambulance::sendAmbulanceAlert(L3Address BroadCastAddress)
{
    EV_INFO<<"Sending Ambulance Alert"<<endl;
    std::ostringstream str;
    str << "Ambulance alert by host["<<getParentModule()->getIndex()<<"]"<< "-" << respNumSent;
    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(respNumSent);
    auto creationTimeTag = payload->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());
    packet->insertAtBack(payload);
    packet->addPar("AMBULANCE_ALERT");
    emit(packetSentSignal, packet);
    socket.sendTo(packet, BroadCastAddress, destPort);
    respNumSent++;
    EV_INFO<<"Ambulance Alert Sent"<<endl;
}


/*
 *  This function configures the hosts by setting up the socket options and few other features.
 */
void ambulance::processStart()
{
    socket.setOutputGate(gate("socketOut"));
    const char *localAddress = par("localAddress");
    socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
    setSocketOptions();

    // Send the message on start
    selfMsg->setKind(SEND);
    processSend();




    if(stopTime >= SIMTIME_ZERO)
    {
           selfMsg->setKind(STOP);
           scheduleAt(stopTime, selfMsg);

    }
}

/*
 *  This function triggers the send packet function to set up the packet.
 */
void ambulance::processSend()
{
        // Message interval in a periodic cycle

        simtime_t messageInterval = simTime() + par("broadCastInterval");
        EV_INFO<<"Ambulance Alert Broadcast"<<endl;
        // Broadcast Pairing Request To Cars In Proximity
        sendAmbulanceAlert(BroadCastAddress);

        if (stopTime < SIMTIME_ZERO || messageInterval < stopTime) {
            selfMsg->setKind(SEND);
            scheduleAt(messageInterval, selfMsg);
        }
        else {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        }

}


void ambulance::processStop()
{
    socket.leaveMulticastGroup(multicastAddress);
    socket.close();
}


/*
 * When a message or self message is arrived, this function will be triggered and suitable option
 * is been choosen according to the type of message arrived.
 */
void ambulance::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        ASSERT(msg == selfMsg);
        switch (selfMsg->getKind()) {
            case START:
                processStart();
                break;

            case SEND:
                processSend();
                break;

            case STOP:
                processStop();
                break;

            default:
                throw cRuntimeError("Invalid kind %d in self message", (int)selfMsg->getKind());
        }
    }
    else if (msg->arrivedOn("socketIn"))
    {
        socket.processMessage(msg);
    }
    else
    {
        throw cRuntimeError("Unknown incoming gate: '%s'", msg->getArrivalGate()->getFullName());
    }


}


/*
 * Once the packet is received, it has to be read to extract information regarding
 * address of the sender.
 */
void ambulance::processPacket(Packet *pk)
{
    // Delete the received packet
    delete pk;
}



void ambulance::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    // process incoming packet
    processPacket(packet);
}

void ambulance::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}



bool ambulance::handleNodeStart(IDoneCallback *doneCallback)
{
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);

    }
    return true;
}

bool ambulance::handleNodeShutdown(IDoneCallback *doneCallback)
{
    if (selfMsg)
        cancelEvent(selfMsg);
    //TODO if(socket.isOpened()) socket.close();
    return true;
}

void ambulance::handleNodeCrash()
{
    if (selfMsg)
        cancelEvent(selfMsg);
}


}



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
#include "inet/smartHonk/source/paring/pairNeighborCars.h"
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

Define_Module(pairNeighborCars);

/*
* Purpose:This function initializes the parameters required for initial condition in the network
*
* Functionality: To setup the nodes with required parameter before it starts. The first node is set as
*                transmitter and then other nodes are receiver until it receives a message from its neighbor
*                Reception of a message triggers other host to transmit message.
*/
void pairNeighborCars::initialize(int stage)
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
void pairNeighborCars::finish()
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
void pairNeighborCars::setSocketOptions()
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
 * This function is useful when the network hold mobile hosts (move) , This function helps to refresh the
 * list of neighbors who has already moved out of the range.
 */
void pairNeighborCars::checkNeighorListRefresh()
{
    int i;
    for(i=0; i<100; i++)
    {
        //timeStampList[hostIndex] = simTime().trunc(SIMTIME_MS);
        simtime_t timeGap = (simTime().trunc(SIMTIME_MS) - timeStampList[i]);
        timeGap = timeGap * 1000;
        if(timeGap >= 200 && timeStampList[i] != 0)
        {
            std::set<int>::iterator indexIterator = neighborIndexList.find(i);
            if( indexIterator != neighborIndexList.end())
            {
                neighborIndexList.erase(indexIterator);
                EV_INFO<<"The host with index:"<<i<<" is no more a neighbor"<<*indexIterator<<endl;
            }
        }
    }

    strcpy(neighborsList,"");
    for(std::set<int>::iterator indexIt=neighborIndexList.begin(); indexIt != neighborIndexList.end();indexIt++)
    {
        sprintf(neighbors,"host[%d]:",*indexIt);
        sprintf(distancevalue,"%.1f \b",hostDistanceList[*indexIt]);
        strcat(neighborsList,neighbors);
        strcat(neighborsList,distancevalue);

   }
}

/*
 * This function is used to set up a udp packet and send it to the required address
 */
void pairNeighborCars::sendPacket()
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

/*
 * This function implements the message sending procedure for the start node basically
 */
void pairNeighborCars::sendResponseToSender(L3Address respDestAddress,bool pair_accepted)
{
    EV_INFO<<"Sending Pairing Decision"<<endl;
    std::ostringstream str;
    if(pair_accepted)
    {
        str << "Paring Accepted by host["<<getParentModule()->getIndex()<<"]"<< "-" << respNumSent;
    }
    else
    {
        str << "Paring Rejected by host["<<getParentModule()->getIndex()<<"]"<< "-" << respNumSent;
    }
    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(respNumSent);
    auto creationTimeTag = payload->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());
    packet->insertAtBack(payload);
    if(pair_accepted)
    {
        packet->addPar("PAIR_ACCEPTED");
    }
    else
    {
        packet->addPar("PAIR_REJECTED");
    }

    emit(packetSentSignal, packet);
    socket.sendTo(packet, respDestAddress, destPort);
    respNumSent++;
    EV_INFO<<"Response sent"<<endl;
}


void pairNeighborCars::sendPairRequest(L3Address BroadCastAddress)
{
    EV_INFO<<"Sending Pairing Request"<<endl;
    std::ostringstream str;
    str << "Pair Request by host["<<getParentModule()->getIndex()<<"]"<< "-" << respNumSent;
    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(respNumSent);
    auto creationTimeTag = payload->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());
    packet->insertAtBack(payload);
    packet->addPar("PAIR_REQUEST");
    emit(packetSentSignal, packet);
    socket.sendTo(packet, BroadCastAddress, destPort);
    respNumSent++;
    EV_INFO<<"Pair Request sent"<<endl;
}

/*
 *  Indicating that the message from host is already been received and to stop sending any further messages
 */
void pairNeighborCars::indicateStopMessage(L3Address stopDestAddress)
{
    cModule *stopHost = L3AddressResolver().findHostWithAddress(L3Address(stopDestAddress));
    int stopHostIndex = stopHost->getIndex();
    EV_INFO<<"Indicating host["<<stopHostIndex<<"] to stop transmission"<<endl;
    std::ostringstream str;
    str << "Indication to host["<<stopHostIndex<<"]-" << stopMsgNumSent;
    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(stopMsgNumSent);
    auto creationTimeTag = payload->addTag<CreationTimeTag>();
    creationTimeTag->setCreationTime(simTime());
    packet->insertAtBack(payload);
    packet->addPar("stopSignal");
    emit(packetSentSignal, packet);
    socket.sendTo(packet, stopDestAddress, destPort);
    stopMsgNumSent++;
    EV_INFO<<"Indication sent to host["<<stopHostIndex<<"]"<<endl;
}

/*
 *  This function configures the hosts by setting up the socket options and few other features.
 */
void pairNeighborCars::processStart()
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
void pairNeighborCars::processSend()
{
        // Message interval in a periodic cycle
        simtime_t messageInterval = simTime() + par("responseDelay");
        EV_INFO<<"Request Mode:"<<response_mode<<endl;
        // Send Response to the car which requested for pairing
        if(response_mode == true)
        {
            EV_INFO<<"Responding To Pair Request"<<endl;
            // Send Response TO Sender Based With Acceptance/Rejection To Pair
            sendResponseToSender(ResponseAddress,pair_acceptance);
            request_pair_mode = false;

        }
        else
        {
            request_pair_mode = true;
        }

        if(request_pair_mode == true)
        {
            messageInterval = simTime() + par("broadCastInterval");
            EV_INFO<<"Car Pairing Request"<<endl;
            // Broadcast Pairing Request To Cars In Proximity
            sendPairRequest(BroadCastAddress);
        }




        if (stopTime < SIMTIME_ZERO || messageInterval < stopTime) {
            selfMsg->setKind(SEND);
            scheduleAt(messageInterval, selfMsg);
        }
        else {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        }

}


void pairNeighborCars::processStop()
{
    socket.leaveMulticastGroup(multicastAddress);
    socket.close();
}


/*
 * When a message or self message is arrived, this function will be triggered and suitable option
 * is been choosen according to the type of message arrived.
 */
void pairNeighborCars::handleMessageWhenUp(cMessage *msg)
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
 * Function to find the position co-ordinates of the host. The returned value will be in meters
 */
Coord pairNeighborCars::getHostPosition(cModule *hostmodule)
{
    // find the co-ordinates of receiving node and sender node and then calculate the radial distance
      cModule *host = getContainingNode(hostmodule);
      IMobility *mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
      std::ostringstream mypos;
      mypos << mobility->getCurrentPosition();
      //EV_INFO << "The position read:" << mypos.str().c_str() << endl;

      return mobility->getCurrentPosition();
}

/*
 * Function to calculate the radial distance by using the obtained position co-ordinates of the sender and receiver
 */
float pairNeighborCars::getRadialDistance(Coord sendercord,Coord receivercord)
{
    float distanceX = (sendercord.x - receivercord.x);
    float distanceY = (sendercord.y - receivercord.y);

    float distance = sqrt((distanceX * distanceX) + (distanceY * distanceY));

    return distance;


}

float pairNeighborCars::getSenderPosition(Coord sendercord,Coord receivercord)
{
    float distanceX = (sendercord.x - receivercord.x);
    float distanceY = (sendercord.y - receivercord.y);

    /*
     * 00 is left and front
     * 01 is left and back
     * 10 is right and front
     * 11 is right and back
     */
    int X_Pos;
    int Y_Pos;
    /*
    if(distanceX > 0) //right side
    {
        X_Pos = 1;
    }
    else // left side
    {
        X_Pos = -1;
    }
    */
    if(distanceY > 0) // front side
    {
        Y_Pos = 0;
    }
    else // back side
    {
        Y_Pos = 1;
    }


    return Y_Pos;


}

/*
 * Once the packet is received, it has to be read to extract information regarding
 * address of the sender.
 */
void pairNeighborCars::processPacket(Packet *pk)
{
    emit(packetReceivedSignal, pk);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
    L3Address MsgSourceAddress = pk->getTag<L3AddressInd>()->getSrcAddress();
    EV_INFO << "Sender Address is:" << MsgSourceAddress << endl;

    // If the message is of type pair request
    if(pk->hasPar("PAIR_REQUEST"))
    {
        pair_request_message = true;
        response_mode = true;
        request_pair_mode = false;

        EV_INFO << "Pair Request Received" <<endl;
    }

    // If the message is of type pair acceptance
    if(pk->hasPar("PAIR_ACCEPTED"))
    {
        pair_request_message = false;
        neighborAcceptedCars.insert(MsgSourceAddress);
        EV_INFO << "Pair Request Accepted" <<endl;
    }

    // If the message is of type pair rejection
    if(pk->hasPar("PAIR_REJECTED"))
    {
        pair_request_message = false;
        /*
        std::set<int>::iterator neighborIter = neighborAcceptedCars.find(MsgSourceAddress);
        if(neighborIter != neighborAcceptedCars.end())
        {
            neighborAcceptedCars.erase(neighborIter);
            EV_INFO << "Deleted Already Existing Car In The List" <<endl;
        }
        */
        EV_INFO << "Pair Request Rejected" <<endl;
    }



    // Delete the received packet
    delete pk;

    /* uncomment this to check which IP address is received
     *
    std::ostringstream mystr;
    mystr << MsgSourceAddress;
    bubble(mystr.str().c_str());

    */

    // finding the host details by using the host address read from the message frame
    cModule *mod = L3AddressResolver().findHostWithAddress(L3Address(MsgSourceAddress));
    SenderhostIndex = mod->getIndex();
    timeStampList[SenderhostIndex] = simTime().trunc(SIMTIME_MS);

    // Finding the position information of the sender and hence calculating the radial distance
    if (mod != NULL && pair_request_message == true)
    {
        pair_request_message = false;
    // To filter the self name from the list of neighbors found, since self messages may arrive
        if (MsgSourceAddress != localHostAddr && SenderhostIndex != localIndex )
        {
            // Adding the source addresses in the set containers in order to have a unique list
            EV_INFO<<"The container size before addition is:"<<neighborAddresses.size()<<endl;
           // EV_INFO<<"The index list size before addition is:"<<neighborIndexList.size()<<endl;
            Coord sendercord = getHostPosition(mod);
            EV_INFO<<"Sender Co-ordinates: "<<sendercord<<endl;
            Coord receivercord = getHostPosition(this);
            EV_INFO<<"Receiver Co-ordinates: "<<receivercord<<endl;
            float senderDis=getRadialDistance(sendercord,receivercord);
            float senderPosition=getSenderPosition(sendercord,receivercord);
            EV_INFO<<"Sender distance is: "<<senderDis<<endl;
            if(senderPosition == 0)
            {
                EV_INFO<<"Sender Is Behind"<<endl;
                EV_INFO<<"Pair Request Accepted"<<endl;
                pair_acceptance = true;
                ResponseAddress=MsgSourceAddress;
                sendResponseToSender(ResponseAddress,pair_acceptance);
            }
            else
            {

               EV_INFO<<"Sender Is Ahead"<<endl;
               EV_INFO<<"Pair Request Rejected"<<endl;
               pair_acceptance = false;
               ResponseAddress=MsgSourceAddress;
               sendResponseToSender(ResponseAddress,pair_acceptance);

            }

            hostDistanceList[SenderhostIndex] = senderDis;

           // std::set<L3Address>::iterator HostIterator = neighborAddresses.find(MsgSourceAddress);

           /*
           std::set<int>::iterator HostIterator = neighborIndexList.find(SenderhostIndex);
           if(HostIterator == neighborIndexList.end())
           {
              neighborAddresses.insert(MsgSourceAddress);
              neighborIndexList.insert(SenderhostIndex);
              EV_INFO<<"The container size after addition is:"<<neighborAddresses.size()<<endl;
             // EV_INFO<<"The index list size after addition is:"<<neighborIndexList.size()<<endl;

              sprintf(neighbors,"host[%d]:",mod->getIndex());
              sprintf(distancevalue,"%.1f \b",hostDistanceList[mod->getIndex()]);
              strcat(neighborsList,neighbors);
              strcat(neighborsList,distancevalue);

           }
           */


       }// Not local address
      }//ModNull




    // Respond to the sender, so that the sender will count this host as a part of the network.
    //if (otherNode == true && discovered == false)
    //{

        simtime_t d = simTime() + par("responseDelay");

        if (stopTime < SIMTIME_ZERO || d < stopTime) {
           cancelEvent(selfMsg);
           selfMsg->setKind(SEND);
           scheduleAt(d, selfMsg);
        }
        else {
           selfMsg->setKind(STOP);
           scheduleAt(stopTime, selfMsg);
        }

       // selfMsg->setKind(SEND);
       // processSend();
    //}


    numReceived++;
}



void pairNeighborCars::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    // process incoming packet
    processPacket(packet);
}

void pairNeighborCars::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void pairNeighborCars::refreshDisplay() const
{
    char buf[1000000];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks \n neighbors:%s", numReceived, numSent,neighborsList);
    getDisplayString().setTagArg("t", 0, buf);

}

bool pairNeighborCars::handleNodeStart(IDoneCallback *doneCallback)
{
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);

    }
    return true;
}

bool pairNeighborCars::handleNodeShutdown(IDoneCallback *doneCallback)
{
    if (selfMsg)
        cancelEvent(selfMsg);
    //TODO if(socket.isOpened()) socket.close();
    return true;
}

void pairNeighborCars::handleNodeCrash()
{
    if (selfMsg)
        cancelEvent(selfMsg);
}


}



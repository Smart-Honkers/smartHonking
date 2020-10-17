/*
 * pairNeighborCars.h
 *
 *  Created on: Oct 13, 2020
 *      Author: Sharath Baliga
 *
 *
 */

#ifndef __INET_AMBULANCE_H
#define __INET_AMBULANCE_H

#include <vector>
#include <string.h>
#include <math.h>
#include "inet/common/INETDefs.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"


namespace inet {

/**
 * UDP application. See NED for more info.
 */
class INET_API ambulance : public ApplicationBase, public UdpSocket::ICallback
{

protected:
    enum SelfMsgKinds { START = 1, SEND, STOP };

    // parameters


    std::set<L3Address> neighborAcceptedCars;
    //bool pair_request_message = false;

    L3Address nodeDiscAddress = Ipv4Address("10.0.255.255");
    L3Address localHostAddr = Ipv4Address("127.0.0.1");
    L3Address multicastAddress = nodeDiscAddress;
    L3Address BroadCastAddress = Ipv4Address("10.0.255.255");
    int localPort = -1, destPort = -1;
    simtime_t startTime;
    simtime_t stopTime;
    simtime_t auto_send_time;
    const char *packetName = nullptr;






    std::set<L3Address> neighborAddresses;  // Collected addresses about neighbors
    std::set<int> neighborIndexList ; // collection of all the neighbor index
    int numOfHosts;
    simtime_t timeStampList[100] = {};
    float hostDistanceList[100] = {};

    L3Address ResponseAddress;
    char neighborsList[1000000] = "";
    char neighbors[100] = "";
    char distancevalue[100] = "";



    //bool initialMsg = true;
    int receptionGap;
    int setReceptionGap = 1;
    int startIndex = 0;
    bool transmitPerm;
    bool listModified;
    bool discovered = false;
    // state
    UdpSocket socket;
    cMessage *selfMsg = nullptr;
    // statistics
    int numSent = 0;
    int numReceived = 0;
    int respNumSent = 0;
    int stopMsgNumSent = 0;

    // Host indices
    int SenderhostIndex;
    int localIndex;

    // Node Categories
    bool startNode = false;
    bool otherNode = false;

    // flags
    bool request_pair_mode = true;
    bool response_mode = false;
    bool pair_request_message = false;
    bool pair_acceptance = false;


protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void sendPacket();
    virtual void processPacket(Packet *msg);
    void sendMessage(L3Address destDiscAddress);
    void receiveMessage();




    void sendAmbulanceAlert(L3Address BroadCastAddress);

    virtual void setSocketOptions();

    virtual void processStart();
    virtual void processSend();
    virtual void processStop();

    virtual bool handleNodeStart(IDoneCallback *doneCallback) override;
    virtual bool handleNodeShutdown(IDoneCallback *doneCallback) override;
    virtual void handleNodeCrash() override;

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
};
}
#endif

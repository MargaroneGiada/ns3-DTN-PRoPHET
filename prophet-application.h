#ifndef PROPHET_APP_H
#define PROPHET_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/ipv4-address.h"
#include "ns3/timer.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/double.h"
#include "arpa/inet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include <map>
#include <unordered_set>
#include <iostream>
#include "prophet-header.h"

using namespace ns3;

#define P_INIT 0.75
#define GAMMA 0.98 
#define BETA 0.25

struct TableEntry {
    double deliveryProb;
    double lastUpdateTime;
};

struct BufferedMessage {
    Ptr<Packet> packet;      
    ProphetHeader header;   
};

class ProphetApplication : public Application {
private:
    uint32_t m_nodeId;                               
    std::map<uint32_t, TableEntry> m_probTable;    
    std::unordered_set<uint32_t> m_historyTable;     
    Ptr<Socket> m_socket;                            
    EventId m_beaconTimer;  
    std::list<BufferedMessage> m_buffer;
    uint32_t m_maxBufferSize;
    double m_threshold;
    uint32_t m_numNodes;
    uint32_t m_localMsgCount; 
    EventId m_trafficTimer;
    EventId m_cleanupEvent; 
    Time m_ttl;           
    
    void CleanupExpiredPackets(); 
    void SendBeacon ();
    void ReceivePacket (Ptr<Socket> socket);
    TracedCallback<uint32_t, uint32_t> m_txTrace; // trasmissione   
    TracedCallback<uint32_t, uint32_t, double> m_rxTrace; // recezione

public:
    ProphetApplication ();
    virtual ~ProphetApplication ();

    static TypeId GetTypeId (void);

    void SetThreshold (double t);
    void SetTTL (Time m_ttl);
    void SetMaxBufferSize (uint32_t size);

    void GenerateTraffic (); 
    void SetNumNodes (uint32_t n); 


protected:
    virtual void StartApplication (void) override;
    virtual void StopApplication (void) override;
};

#endif // PROPHET_APP_H
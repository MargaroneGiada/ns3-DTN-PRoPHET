#include "prophet-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ProphetApplication");
NS_OBJECT_ENSURE_REGISTERED (ProphetApplication);

TypeId ProphetApplication::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::ProphetApplication")
        .SetParent<Application> ()
        .SetGroupName ("Simulazione")
        .AddConstructor<ProphetApplication> ()
        .AddTraceSource ("TxData", 
                         "Trace per i messaggi appena generati",
                         MakeTraceSourceAccessor (&ProphetApplication::m_txTrace),
                         "ns3::TracedCallback<uint32_t, uint32_t>") 
                         
        .AddTraceSource ("RxData", 
                         "Trace per i messaggi arrivati a destinazione",
                         MakeTraceSourceAccessor (&ProphetApplication::m_rxTrace),
                         "ns3::TracedCallback<uint32_t, uint32_t, double>");
    return tid;
}

// costruttore
ProphetApplication::ProphetApplication () {}
ProphetApplication::~ProphetApplication () {}


void ProphetApplication::SetThreshold (double t) {
    m_threshold = t;
}

void ProphetApplication::SetMaxBufferSize (uint32_t size) {
    m_maxBufferSize = size; 
}

void ProphetApplication::SetTTL (Time ttl) {
    m_ttl = ttl; 
}

// Inizializza il nodo, il socket UDP e i timer periodici
void ProphetApplication::StartApplication() {
    m_nodeId = GetNode()->GetId();

    TableEntry selfEntry;
    selfEntry.deliveryProb = 1; // Probabilità verso sé stessi è sempre 1
    selfEntry.lastUpdateTime = Simulator::Now().GetSeconds();

    m_probTable[m_nodeId] = selfEntry;

    m_socket = Socket::CreateSocket (GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(), 9000);
    m_socket->Bind (local);
    m_socket->SetAllowBroadcast (true);

    m_socket->SetRecvCallback (MakeCallback (&ProphetApplication::ReceivePacket, this));

    // aggiunge un quantitativo casuale di millisecondi prima di inviare beacon
    Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable> ();
    Time delay = MilliSeconds (jitter->GetInteger(0, 50));
    m_beaconTimer = Simulator::Schedule (delay, &ProphetApplication::SendBeacon, this);

    m_localMsgCount = 0; 

    // genera traffico una volta ogni 10 secondi
    m_trafficTimer = Simulator::Schedule(Seconds(10.0), &ProphetApplication::GenerateTraffic, this);

    // pulisce i pacchetti deprecati una volta al secondo
    m_cleanupEvent = Simulator::Schedule(Seconds(1.0), &ProphetApplication::CleanupExpiredPackets, this);
    
    NS_LOG_INFO ("Nodo " << m_nodeId << " acceso. In ascolto sulla porta 9000.");
}

// rimuove pacchetti con ttl scaduto
void ProphetApplication::CleanupExpiredPackets() {
    Time now = Simulator::Now();
    
    auto it = std::remove_if(m_buffer.begin(), m_buffer.end(),
        [&](const BufferedMessage& entry) {
            Time lifeTime = now - MilliSeconds(entry.header.GetTimestamp());
            return (lifeTime > m_ttl); 
        });

    if (it != m_buffer.end()) {
        m_buffer.erase(it, m_buffer.end());
        NS_LOG_INFO("Buffer pulito: rimossi pacchetti scaduti");
    }

    m_cleanupEvent = Simulator::Schedule(Seconds(1.0), &ProphetApplication::CleanupExpiredPackets, this);
}

void ProphetApplication::StopApplication (void) {
    if (m_socket) {
        m_socket->Close ();
    }
    Simulator::Cancel (m_beaconTimer);
}

// invio pacchetto contenente la propria tabella
void ProphetApplication::SendBeacon(){
    uint32_t numEntries = m_probTable.size();
    uint32_t payloadSize = 4 + (numEntries * 12); // 4 di int per la size + (4 + 8) * numEntries
    uint8_t* buffer = new uint8_t[payloadSize];
    uint32_t offset = 0;

    uint32_t netEntries = htonl(numEntries); //host to network
    memcpy(buffer + offset, &netEntries, 4);
    offset +=4;

    for (auto const& entry : m_probTable) {
        
        uint32_t nodeId = entry.first;
        double prob = entry.second.deliveryProb; 
        
        uint32_t netId = htonl(nodeId);
        memcpy(buffer + offset, &netId, 4);
        offset += 4;

        memcpy(buffer + offset, &prob, 8);
        offset += 8;
    }
    

    Ptr<Packet> packet = Create<Packet> (buffer, payloadSize);

    ProphetHeader header;
    header.SetSource(Ipv4Address::GetAny());
    header.SetDestination(Ipv4Address("255.255.255.255")); // broadcast
    header.SetMessageId(0); 
    header.SetTimestamp(Simulator::Now().GetMilliSeconds());

    packet->AddHeader(header);

    InetSocketAddress broadcastAddr (Ipv4Address::GetBroadcast (), 9000);

    m_socket->SendTo(packet, 0, broadcastAddr);

    delete[] buffer;

    Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable> ();
    Time nextDelay = MilliSeconds (500) + MilliSeconds (jitter->GetInteger(0, 50));
    m_beaconTimer = Simulator::Schedule (nextDelay, &ProphetApplication::SendBeacon, this);
}

// recezione di un pacchetto
void ProphetApplication::ReceivePacket(Ptr<Socket> socket){
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);

    ProphetHeader header;
    packet->RemoveHeader (header);

    Time packetAge = Simulator::Now() - MilliSeconds(header.GetTimestamp());
    
    // scarto pacchetti vecchi
    if (packetAge > m_ttl) {
        NS_LOG_INFO("Pacchetto scaduto! Droppato.");
        return; 
    }
    

    if (header.GetMessageId() == 0) { // beacon
        Ipv4Address senderIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
        uint32_t ipValue = senderIp.Get(); 
        uint32_t neighborId = (ipValue & 0x000000FF) - 1;


        if (neighborId == m_nodeId) return;

        double currentTime = Simulator::Now().GetSeconds();

        // invecchiamento
        if (m_probTable.find(neighborId) != m_probTable.end()) {
            double oldProb = m_probTable[neighborId].deliveryProb;
            double timeDiff = currentTime - m_probTable[neighborId].lastUpdateTime;
            
            int k = (int)timeDiff; 
            oldProb = oldProb * std::pow(GAMMA, k); 
            m_probTable[neighborId].deliveryProb = oldProb;
        } else {
            TableEntry newEntry;
            newEntry.deliveryProb = 0.0;
            m_probTable[neighborId] = newEntry;
        }

        // aggiorna probabilità di incontro diretto
        double oldProb = m_probTable[neighborId].deliveryProb;
        double pEncounter = oldProb + (1.0 - oldProb) * P_INIT;

        m_probTable[neighborId].deliveryProb = pEncounter;
        m_probTable[neighborId].lastUpdateTime = currentTime;

        // consegna dei pacchetti se incontro il destinatario diretto
        for (auto it = m_buffer.begin(); it != m_buffer.end(); ) {
            
            uint32_t msgDestId = (it->header.GetDestination().Get() & 0x000000FF) - 1;

            if (msgDestId == neighborId) {
                Ptr<Packet> packetToSend = it->packet->Copy();
                packetToSend->AddHeader(it->header);

                std::string neighborIpStr = "10.1.1." + std::to_string(neighborId + 1);
                InetSocketAddress neighborAddr = InetSocketAddress(Ipv4Address(neighborIpStr.c_str()), 9000);
                
                m_socket->SendTo(packetToSend, 0, neighborAddr);
                it = m_buffer.erase(it); 
                
                NS_LOG_INFO ("Nodo " << m_nodeId << " ha fatto una CONSEGNA DIRETTA al destinatario " << neighborId);
            } else {
                ++it; 
            }
        }

        // estrazione tabella ricevuta dal vicino
        uint32_t payloadSize = packet->GetSize();
        uint8_t* buffer = new uint8_t[payloadSize];
        packet->CopyData(buffer, payloadSize);

        uint32_t offset = 0;

        uint32_t netEntries;
        memcpy(&netEntries, buffer + offset, 4);
        uint32_t numEntries = ntohl(netEntries);
        offset += 4;

        // aggiornamento propria tabella in base a quella ricevuta dal vicino (transitività)
        for (uint32_t i = 0; i < numEntries; i++) {
            uint32_t destIdNet;
            double destProb;

            memcpy(&destIdNet, buffer + offset, 4);
            offset += 4;
            memcpy(&destProb, buffer + offset, 8);
            offset += 8;

            uint32_t destId = ntohl(destIdNet);

            if (destId == m_nodeId || destId == neighborId) continue;

            // invecchiamento
            if (m_probTable.find(destId) != m_probTable.end()) {
                double myOldProbDest = m_probTable[destId].deliveryProb;
                double timeDiffDest = currentTime - m_probTable[destId].lastUpdateTime;
                myOldProbDest = myOldProbDest * std::pow(GAMMA, (int)timeDiffDest);
                m_probTable[destId].deliveryProb = myOldProbDest;
            } else {
                TableEntry newDestEntry;
                newDestEntry.deliveryProb = 0.0;
                m_probTable[destId] = newDestEntry;
            }
            
            double myOldProbDest = m_probTable[destId].deliveryProb;
            double pTransitive = myOldProbDest + (1.0 - myOldProbDest) * pEncounter * destProb * BETA;
            
            m_probTable[destId].deliveryProb = pTransitive;
            m_probTable[destId].lastUpdateTime = currentTime;

            // vede se può inoltrare qualcosa al vicino
            for (auto it = m_buffer.begin(); it != m_buffer.end(); ++it) {
                
                uint32_t msgDestId = (it->header.GetDestination().Get() & 0x000000FF) - 1;

                if (msgDestId == destId) {
    
                    double currentMessageRecord = it->header.GetRecord();

                    double requiredJump = m_threshold * (1.0 - currentMessageRecord);
                    double targetToBeat = currentMessageRecord + requiredJump;

                    if (destProb >= targetToBeat) {
                        it->header.SetRecord(destProb);

                        Ptr<Packet> packetToSend = it->packet->Copy();
                        packetToSend->AddHeader(it->header);

                        std::string neighborIpStr = "10.1.1." + std::to_string(neighborId + 1);
                        InetSocketAddress neighborAddr = InetSocketAddress(Ipv4Address(neighborIpStr.c_str()), 9000);
                        
                        m_socket->SendTo(packetToSend, 0, neighborAddr);

                        NS_LOG_INFO ("Nodo " << m_nodeId << " inoltra msg " << it->header.GetMessageId() 
                                    << " al vicino " << neighborId << " (Nuovo Record: " << destProb << ")");
                    }
                }
            }
        }

        delete[] buffer;

    }else{ // dati ricevuti

        uint32_t msgId = header.GetMessageId();
        Ipv4Address destIp = header.GetDestination();
        uint32_t destId = (destIp.Get() & 0x000000FF) - 1; 

        // per evitare duplicati
        if (m_historyTable.find(msgId) != m_historyTable.end()) {
            return; 
        }
        
        m_historyTable.insert(msgId);
        
        // se è destinato a se stesso
        if (destId == m_nodeId) {
        
            double delay = Simulator::Now().GetMilliSeconds() - header.GetTimestamp();
            m_rxTrace(m_nodeId, msgId, delay);
            NS_LOG_INFO ("CONSEGNATO! Nodo " << m_nodeId << " ha ricevuto il msg " << msgId 
                         << " in " << delay << " ms.");
            
            return;
        }

        BufferedMessage newMsg;
        newMsg.packet = packet; 
        newMsg.header = header;

        // se il buffer è pieno, scarta quello con probabilità di consegna minore

        if (m_buffer.size() >= m_maxBufferSize) {
            
            std::list<BufferedMessage>::iterator worstMsgIt = m_buffer.begin();
            double lowestProb = 1.0; 

            for (auto it = m_buffer.begin(); it != m_buffer.end(); ++it) {
                uint32_t targetId = (it->header.GetDestination().Get() & 0x000000FF) - 1;
                
                double currentProb = 0.0;
                if (m_probTable.find(targetId) != m_probTable.end()) {
                    currentProb = m_probTable[targetId].deliveryProb;
                }

                if (currentProb < lowestProb) {
                    lowestProb = currentProb;
                    worstMsgIt = it;
                }
            }

            m_buffer.erase(worstMsgIt);
            NS_LOG_INFO ("Nodo " << m_nodeId << " ha droppato un messaggio per buffer pieno.");
        }

        m_buffer.push_back(newMsg);

        NS_LOG_INFO ("Nodo " << m_nodeId << " ha ricevuto il DATO " << header.GetMessageId());

    }
}

void ProphetApplication::SetNumNodes (uint32_t n) {
    m_numNodes = n;
}

// genera pacchetto nuovo
void ProphetApplication::GenerateTraffic(){
    Ptr<UniformRandomVariable> chance = CreateObject<UniformRandomVariable>();
    double roll = chance->GetValue(0.0, 1.0);

    if(roll <= 0.05){
        Ptr<UniformRandomVariable> destChooser = CreateObject<UniformRandomVariable>();
        uint32_t targetId = destChooser->GetValue(0, m_numNodes - 1); // scelta casuale del destinatario

        if(targetId != m_nodeId){
            m_localMsgCount++;
            uint32_t globalMsgId = (m_nodeId << 16 ) | m_localMsgCount;

            Ptr<Packet> packet = Create<Packet>(500);
            
            ProphetHeader header;
            header.SetMessageId(globalMsgId);

            std::string destIpStr = "10.1.1." + std::to_string(targetId + 1);
            header.SetDestination(Ipv4Address(destIpStr.c_str()));

            header.SetTimestamp(Simulator::Now().GetMilliSeconds());

            double myProb = 0.0;
            if (m_probTable.find(targetId) != m_probTable.end()) {
                myProb = m_probTable[targetId].deliveryProb;
            }
            header.SetRecord(myProb);

            BufferedMessage newMsg;
            newMsg.packet = packet;
            newMsg.header = header;

            m_historyTable.insert(globalMsgId);

            if (m_buffer.size() < m_maxBufferSize) {
                m_buffer.push_back(newMsg);
                m_txTrace(m_nodeId, globalMsgId);
                NS_LOG_INFO("Nodo " << m_nodeId << " ha GENERATO il messaggio " << globalMsgId
                            << " per il nodo " << targetId);
            } else {
                NS_LOG_INFO("Nodo " << m_nodeId << " voleva generare un messaggiom ma ha il buffer PIENO.");
            }

            
        }
    }

    // aggiunta di tempo scelta casualmente
    Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable> ();
    Time nextCheck = Seconds(5.0) + MilliSeconds(jitter->GetInteger(0, 500));
    m_trafficTimer = Simulator::Schedule(nextCheck, &ProphetApplication::GenerateTraffic, this);
}

// ============================================================================
// Progetto ProPHET con microzone
// ============================================================================

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "prophet-header.h"
#include "prophet-application.h"
#include "ns3/netanim-module.h" 
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("prophet");

// variabili globali
uint32_t g_totalGenerated = 0;
uint32_t g_totalDelivered = 0;
double g_cumulativeDelay = 0.0;

// callback
void PacketGeneratedCallback(uint32_t nodeId, uint32_t msgId) {
    g_totalGenerated++;
}

void PacketDeliveredCallback(uint32_t nodeId, uint32_t msgId, double delay) {
    g_totalDelivered++;
    g_cumulativeDelay += delay;
}

struct ZoneBounds {
    double xMin;
    double xMax;
    double yMin;
    double yMax;
};

// fuznione per ottenere i confini di una microzona
ZoneBounds GetZoneBounds (uint32_t zoneId, uint32_t zoneSize, uint32_t numCols) 
{
    ZoneBounds bounds;
    
    uint32_t col = zoneId % numCols;
    uint32_t row = zoneId / numCols;
    
    bounds.xMin = col * zoneSize;
    bounds.yMin = row * zoneSize;
    
    bounds.xMax = bounds.xMin + zoneSize;
    bounds.yMax = bounds.yMin + zoneSize;
    
    return bounds;
}

int main(int argc, char** argv){

    uint32_t numNodes = 20;
    uint32_t bufferSize = 50;
    double threshold = 0.20;
    double simTime = 3600.0;
    double localRatio = 0.70;
    uint32_t seed = 1; 
    double ttl = 600;
    bool enableAnim = false; 
    uint32_t areaSize = 1500;
    uint32_t microzoneSize = 300;

    // variabili modificabili da command line
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Numero totale di nodi nello scenario", numNodes);
    cmd.AddValue ("bufferSize", "Capacita' massima del buffer", bufferSize);
    cmd.AddValue ("threshold", "Soglia per il Delegation Forwarding", threshold);
    cmd.AddValue ("simTime", "Durata della simulazione (in secondi)", simTime);
    cmd.AddValue ("localRatio", "Percentuale di nodi a spostamento ristretto", localRatio);
    cmd.AddValue ("seed", "Seme globale per la riproducibilita'", seed);
    cmd.AddValue ("ttl", "Time To Live massimo di ogni packetto", ttl);
    cmd.AddValue ("enableAnim", "Attiva (1) o disattiva (0) la generazione del video NetAnim", enableAnim);
    cmd.Parse (argc, argv);

    NS_LOG_INFO (" --- Inizio Simulazione ProPHET ---");
    NS_LOG_INFO ("Nodi: " << numNodes << " | Buffer: " << bufferSize << " | Threshold: " << threshold << " | Simulation Time: " << simTime 
        << " | Local Ratio: " << localRatio << " | Seed: " << seed << " | TTL: " << ttl << " | Animation Enabled: " << enableAnim);

    uint32_t numLocals = numNodes * localRatio;
    uint32_t numWides = numNodes - numLocals;
    uint32_t numCols = areaSize / microzoneSize; 
    uint32_t totalZones = numCols * numCols;

    // impostazione seed per il random number generator
    RngSeedManager::SetSeed (seed);
    Config::SetDefault ("ns3::ArpCache::PendingQueueSize", UintegerValue (500));
    Config::SetDefault ("ns3::ArpCache::MaxRetries", UintegerValue (10));

    // container per i nodi
    NodeContainer nodes;
    nodes.Create(numNodes);

    NS_LOG_INFO ("Popolazione: " << numLocals << " spostamenti locali, " << numWides << " spostamenti ampi.");

    // mobilità dei nodi
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::WaypointMobilityModel");
    mobility.Install (nodes);

    // generazione dei random number generator per ogni singola variabile
    Ptr<UniformRandomVariable> randZone = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randSpeed = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randPause = CreateObject<UniformRandomVariable> ();

    for (uint32_t i = 0; i < numNodes; ++i) {
        
        Ptr<Node> currentNode = nodes.Get(i); 
        Ptr<WaypointMobilityModel> waypointModel = currentNode->GetObject<WaypointMobilityModel> ();

        std::vector<uint32_t> myRing;

        // mobilità nodi detti locali (spostamenti limitati)
        if (i < numLocals) {
            
            uint32_t startZone = randZone->GetInteger(0, totalZones - 1);
            uint32_t col = startZone % numCols;
            uint32_t row = startZone / numCols;

            int stepX = (col == numCols - 1) ? -1 : 1; // in caso di posizione sul bordo della macro-area
            int stepY = (row == numCols - 1) ? -numCols : numCols; // in caso di posizione sul bordo della macro-area

            myRing.push_back(startZone);                      
            myRing.push_back(startZone + stepX);               
            myRing.push_back(startZone + stepY + stepX);       
            myRing.push_back(startZone + stepY);               
            
        } else {   // mobilità nodi detti ampi (spostamenti lunghi)         
            
            uint32_t halfCols = numCols / 2;
            std::vector<uint32_t> tempRing;

            uint32_t r1 = randZone->GetInteger(0, halfCols - 1);
            uint32_t c1 = randZone->GetInteger(0, halfCols - 1);
            tempRing.push_back(r1 * numCols + c1); 

            uint32_t r2 = randZone->GetInteger(0, halfCols - 1);
            uint32_t c2 = randZone->GetInteger(numCols - halfCols, numCols - 1);
            tempRing.push_back(r2 * numCols + c2);

            uint32_t r3 = randZone->GetInteger(numCols - halfCols, numCols - 1);
            uint32_t c3 = randZone->GetInteger(numCols - halfCols, numCols - 1);
            tempRing.push_back(r3 * numCols + c3);

            uint32_t r4 = randZone->GetInteger(numCols - halfCols, numCols - 1);
            uint32_t c4 = randZone->GetInteger(0, halfCols - 1);
            tempRing.push_back(r4 * numCols + c4);

            uint32_t startIndex = randZone->GetInteger(0, 3);
            bool isClockwise = randSpeed->GetValue(0.0, 1.0) >= 0.5; //scegliere se seguire le tappe in senso orario o antiorario

            for (int j = 0; j < 4; j++) {
                
                uint32_t targetIndex;

                if (isClockwise) {
                    targetIndex = (startIndex + j) % 4;
                } else {
                    targetIndex = (startIndex - j + 4) % 4;
                }

                myRing.push_back(tempRing[targetIndex]);
            }
                            
        }

        // si sceglie un punto si inizio randomico
        ZoneBounds startBounds = GetZoneBounds(myRing[0], microzoneSize, numCols);
        double startX = randX->GetValue(startBounds.xMin, startBounds.xMax);
        double startY = randY->GetValue(startBounds.yMin, startBounds.yMax);
        Vector currentPos(startX, startY, 0.0);
        waypointModel->SetPosition(currentPos);

        double currentTime = 0.0;
        uint32_t nextStopIndex = 1;

        while (currentTime < simTime) {
            // momento di stazionamento di durata casuale
            double pause = randPause->GetValue(5.0, 30.0);
            currentTime += pause;
            if (currentTime >= simTime) break;
            waypointModel->AddWaypoint(Waypoint(Seconds(currentTime), currentPos));

            // punto casuale all'interno della zona
            uint32_t targetZone = myRing[nextStopIndex];
            ZoneBounds targetBounds = GetZoneBounds(targetZone, microzoneSize, numCols);
            double destX = randX->GetValue(targetBounds.xMin, targetBounds.xMax);
            double destY = randY->GetValue(targetBounds.yMin, targetBounds.yMax);
            Vector nextPos(destX, destY, 0.0);

            // velocità e tempo di percorrenza
            double distance = CalculateDistance(currentPos, nextPos);
            double speed = randSpeed->GetValue(5.0, 30.0);
            double travelTime = distance / speed;
            
            currentTime += travelTime;
            if (currentTime >= simTime) break;
            
            waypointModel->AddWaypoint(Waypoint(Seconds(currentTime), nextPos));

            currentPos = nextPos;
            nextStopIndex = (nextStopIndex + 1) % myRing.size(); 
        }
    }

    // helper per canale, livello fisico e wifi 
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.0));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    WifiHelper wifi; 
    wifi.SetStandard(WIFI_STANDARD_80211b);
    
    NetDeviceContainer netDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // assegnazione degli indirizzi ipv4
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(netDevices);

    ApplicationContainer dtnApps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        // applicazione creata ad hoc
        Ptr<ProphetApplication> app = CreateObject<ProphetApplication> ();

        app->SetThreshold (threshold);
        app->SetMaxBufferSize (bufferSize);
        app->SetTTL(Seconds(ttl));
        app->SetNumNodes (numNodes);

        app->TraceConnectWithoutContext ("TxData", MakeCallback (&PacketGeneratedCallback));
        app->TraceConnectWithoutContext ("RxData", MakeCallback (&PacketDeliveredCallback));

        app->SetStartTime (Seconds (1.0));
        app->SetStopTime (Seconds (simTime));

        nodes.Get (i)->AddApplication (app);
        
        dtnApps.Add (app);
    }
    
    // nodi fantoccio per ottenere in NetAnim le dimenzioni giuste della macro-zona
    NodeContainer dummyNodes;
    dummyNodes.Create(4);

    MobilityHelper dummyMobility;
    dummyMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    dummyMobility.Install(dummyNodes);

    dummyNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    dummyNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(1500.0, 0.0, 0.0));
    dummyNodes.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 1500.0, 0.0));
    dummyNodes.Get(3)->GetObject<MobilityModel>()->SetPosition(Vector(1500.0, 1500.0, 0.0));

    Simulator::Stop(Seconds(simTime));

    // relativo a NetAnim
    AnimationInterface* anim = nullptr;

    if (enableAnim) {
        anim = new AnimationInterface("prophet-animazione.xml");

        anim->EnablePacketMetadata(true); 

        for (uint32_t i = 0; i < numNodes; ++i) {
            anim->UpdateNodeColor (nodes.Get(i), 150, 150, 150); 
            anim->UpdateNodeSize (nodes.Get(i), 10.0, 10.0); 
        }

        // nodi fantoccio sono resi invisibili
        for(int i = 0; i < 4; i++){
            anim->UpdateNodeSize (dummyNodes.Get(i), 0, 0); 
            anim->UpdateNodeColor (dummyNodes.Get(i), 0, 0, 0); 
        }
    }
    

    // inizio simulazione
    Simulator::Run();

    double deliveryRatio = 0.0;
    if (g_totalGenerated > 0) {
        deliveryRatio = (double)g_totalDelivered / g_totalGenerated;
    }

    double avgDelay = 0.0;
    if (g_totalDelivered > 0) {
        avgDelay = g_cumulativeDelay / g_totalDelivered;
    }

    std::cout << "\n--- RISULTATI SIMULAZIONE ---" << std::endl;
    std::cout << "NumNodes \t Threshold \t BufferSize \t TTL \t LocalRatio \t Generated \t Delivered \t DeliveryRatio \t AvgDelay(ms) \n " 
              << numNodes << "\t" 
              << threshold << "\t" 
              << bufferSize << "\t" 
              << ttl << "\t" 
              << localRatio << "\t" 
              << g_totalGenerated << "\t" 
              << g_totalDelivered << "\t" 
              << deliveryRatio << "\t" 
              << avgDelay << std::endl;

    // stampa dei risultati in un file .csv
    std::stringstream stream;
    stream << std::fixed << std::setprecision(1) << threshold;
    std::string thresholdStr = stream.str(); 

    std::string filename = "risultati_prophet_time" + std::to_string((int)simTime) 
                        + "_thr" + thresholdStr + "_LocalRatio.csv";

    std::ofstream csvFile;
    csvFile.open(filename, std::ios_base::app);
    csvFile << numNodes << "\t" 
            << threshold << "\t" 
            << bufferSize << "\t" 
            << ttl << "\t" 
            << localRatio << "\t" 
            << g_totalGenerated << "\t" 
            << g_totalDelivered << "\t" 
            << deliveryRatio << "\t" 
            << avgDelay << "\n";

    csvFile.close();

    // fine della simulazione e distruzione di quanto creato
    Simulator::Destroy();

    if (anim != nullptr) {
        delete anim;
    }

    return 0;
}

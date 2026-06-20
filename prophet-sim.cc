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
// #include "ns3/netanim-module.h" // per NETANIM

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("prophet");

struct ZoneBounds {
    double xMin;
    double xMax;
    double yMin;
    double yMax;
};

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

    double areaSize = 1500.0;
    uint32_t microzoneSize = 300;

    uint32_t numLocals = numNodes * localRatio;
    uint32_t numWides = numNodes - numLocals;
    uint32_t numCols = areaSize / microzoneSize; 
    uint32_t totalZones = numCols * numCols;

    CommandLine cmd;
    cmd.AddValue ("numNodes", "Numero totale di nodi nello scenario", numNodes);
    cmd.AddValue ("bufferSize", "Capacita' massima del buffer", bufferSize);
    cmd.AddValue ("threshold", "Soglia per il Delegation Forwarding", threshold);
    cmd.AddValue ("simTime", "Durata della simulazione (in secondi)", simTime);
    cmd.AddValue ("localRatio", "Percentuale di nodi a spostamento ristretto", localRatio);
    cmd.AddValue ("seed", "Seme globale per la riproducibilita'", seed);
    cmd.Parse (argc, argv);

    NS_LOG_INFO (" --- Inizio Simulazione ProPHET ---");
    NS_LOG_INFO ("Nodi: " << numNodes << " | Buffer: " << bufferSize << " | Threshold: " << threshold << "| Seed: " << seed);

    RngSeedManager::SetSeed (seed);

    NodeContainer nodes;
    nodes.Create(numNodes);

    NS_LOG_INFO ("Popolazione: " << numLocals << " spostamenti locali, " << numWides << " spostamenti ampi.");

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::WaypointMobilityModel");
    mobility.Install (nodes);

    Ptr<UniformRandomVariable> randZone = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randSpeed = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> randPause = CreateObject<UniformRandomVariable> ();

    for (uint32_t i = 0; i < numNodes; ++i) {
        
        Ptr<Node> currentNode = nodes.Get(i); 
        Ptr<WaypointMobilityModel> waypointModel = currentNode->GetObject<WaypointMobilityModel> ();

        std::vector<uint32_t> myRing;

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
            
        } else {            
            
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
            bool isClockwise = randSpeed->GetValue(0.0, 1.0) >= 0.5;

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

   
        ZoneBounds startBounds = GetZoneBounds(myRing[0], microzoneSize, numCols);
        double startX = randX->GetValue(startBounds.xMin, startBounds.xMax);
        double startY = randY->GetValue(startBounds.yMin, startBounds.yMax);
        Vector currentPos(startX, startY, 0.0);
        waypointModel->SetPosition(currentPos);

        double currentTime = 0.0;
        uint32_t nextStopIndex = 1;

        while (currentTime < simTime) {
            
            double pause = randPause->GetValue(5.0, 30.0);
            currentTime += pause;
            if (currentTime >= simTime) break;
            waypointModel->AddWaypoint(Waypoint(Seconds(currentTime), currentPos));

            uint32_t targetZone = myRing[nextStopIndex];
            ZoneBounds targetBounds = GetZoneBounds(targetZone, microzoneSize, numCols);
            double destX = randX->GetValue(targetBounds.xMin, targetBounds.xMax);
            double destY = randY->GetValue(targetBounds.yMin, targetBounds.yMax);
            Vector nextPos(destX, destY, 0.0);

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

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(netDevices);

    ApplicationContainer dtnApps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        // 1. Creiamo un'istanza della nostra applicazione personalizzata
        Ptr<ProphetApplication> app = CreateObject<ProphetApplication> ();

        // 2. CONFIGURAZIONE DINAMICA: Passiamo le variabili lette da CommandLine!
        app->SetThreshold (threshold);
        app->SetMaxBufferSize (bufferSize);

        // 3. Associazioni di sistema: diciamo all'app di quanti millisecondi impostare lo stop se serve
        app->SetStartTime (Seconds (1.0));
        app->SetStopTime (Seconds (simTime));

        // 4. Installiamo fisicamente l'applicazione all'interno del nodo i-esimo
        nodes.Get (i)->AddApplication (app);
        
        // Aggiungiamo l'app al contenitore generale
        dtnApps.Add (app);
    }
    

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

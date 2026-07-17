
# 📡 PRoPHET DTN Simulation in ns-3
Questo repository contiene l'implementazione e la simulazione del protocollo di routing PRoPHET (Probabilistic Routing Protocol using History of Encounters and Transitivity) per reti DTN (Delay Tolerant Networks).
Il progetto è stato sviluppato sul simulatore di rete ad eventi discreti ns-3 (versione 3.46) e introduce alcune ottimizzazioni come il Delegation Forwarding tramite soglia dinamica e una gestione del buffer basata sul Time-To-Live (TTL).
#### 🛠️ Requisiti e Installazione
- Per eseguire questa simulazione, è necessario avere un ambiente configurato con [ns-3.46](https://www.nsnam.org/releases/ns-3-46/), e i relativi prerequisiti (es. g++, cmake).

- Copiare i file sorgente di questo repository all'interno della cartella scratch/ dell'ambiente ns-3. 
- Ritornare nella root di ns-3.46 e lanciare  la compilazione: ```./ns3 build```

#### Esecuzione e Variabili da Linea di Comando
La simulazione è altamente parametrizzabile grazie all'uso degli argomenti da linea di comando. Questo permette di avviare test in batch per analizzare diversi scenari senza ricompilare il codice.
Un esempio di esecuzione è il seguente: ```./ns3 run "prophet-sim --numNodes=40 --bufferSize=100 --threshold=0.2 --simTime=1800 --ttl=600" ```
##### Parametri disponibili:
| Argomento | Descrizione | Default |
|:---|:---|:---|
|--numNodes | numero totale dei nodi nella simulazione | 20 |
|--bufferSize |capacità massima del buffer di ogni nodo (numero di pacchetti) | 50 |
|--threshold | soglia per il Delegation Forwarding (da 0.0 a 1.0) | 0.2 |
|--simTime | durata totale della simulazione in secondi | 3600 |
|--seed | seme per il generatore di numeri pseudo-casuali | 1 | 
|--ttl | Time-To-Live assoluto dei pacchetti in secondi| 600 |
|--localRatio | percentuale di nodi con mobilità locale (es. 0.7 = 70%) | 0.7 |
|--enableAnim | abilita o disabilita la generazione del file .xml per NetAnim | false |

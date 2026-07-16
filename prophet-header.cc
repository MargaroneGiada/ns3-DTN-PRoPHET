#include "prophet-header.h"

using namespace ns3 ;
NS_OBJECT_ENSURE_REGISTERED (ProphetHeader);

// costruttore
ProphetHeader::ProphetHeader () {
    m_messageId = 0;
    m_timestamp = 0;
    m_record = 0.0;
}

// setter e getter 
void ProphetHeader::SetMessageId (uint32_t id) { m_messageId = id; }
uint32_t ProphetHeader::GetMessageId () const { return m_messageId; }

void ProphetHeader::SetSource (Ipv4Address src) { m_source = src; }
Ipv4Address ProphetHeader::GetSource () const { return m_source; }

void ProphetHeader::SetDestination (Ipv4Address dst) { m_destination = dst; }
Ipv4Address ProphetHeader::GetDestination () const { return m_destination; }

void ProphetHeader::SetTimestamp (uint32_t ts) { m_timestamp = ts; }
uint32_t ProphetHeader::GetTimestamp () const { return m_timestamp; }

void ProphetHeader::SetRecord (double rc) { m_record = rc; }
double ProphetHeader::GetRecord () const { return m_record; }

TypeId ProphetHeader::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::ProphetHeader")
        .SetParent<Header> ()
        .SetGroupName ("Simulazione")
        .AddConstructor<ProphetHeader> ();
    return tid;
}

TypeId ProphetHeader::GetInstanceTypeId (void) const {
    return GetTypeId ();
}

// stampa il contenuto dell'header 
void ProphetHeader::Print (std::ostream &os) const {
    os << "MsgId=" << m_messageId 
       << " Src=" << m_source 
       << " Dst=" << m_destination 
       << " Time=" << m_timestamp
       << " Record=" << m_record;
}

// dimensione totale dell'header in byte
uint32_t ProphetHeader::GetSerializedSize (void) const {
    return 24;
}

// converte i dati dell'header in una sequenza di byte per la trasmissione
void ProphetHeader::Serialize (Buffer::Iterator start) const {
    start.WriteHtonU32 (m_messageId);
    start.WriteHtonU32 (m_source.Get ());
    start.WriteHtonU32 (m_destination.Get ());
    start.WriteHtonU32 (m_timestamp);
    
    uint8_t buf[8];
    memcpy(buf, &m_record, 8);
    start.Write(buf, 8);
}

// legge la sequenza di byte ricevuta e ricostruisce i dati dell'header
uint32_t ProphetHeader::Deserialize (Buffer::Iterator start) {
    m_messageId = start.ReadNtohU32 ();
    m_source = Ipv4Address (start.ReadNtohU32 ()); 
    m_destination = Ipv4Address (start.ReadNtohU32 ());
    m_timestamp = start.ReadNtohU32 ();
    
    uint8_t buf[8];
    start.Read(buf, 8);
    memcpy(&m_record, buf, 8);
    
    return GetSerializedSize ();
}
#ifndef PROPHET_HEADER_H
#define PROPHET_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

class ProphetHeader : public Header {
private:
    uint32_t m_messageId;      
    Ipv4Address m_source;      
    Ipv4Address m_destination; 
    uint32_t m_timestamp;   
    double m_record;

public:
    ProphetHeader ();

    void SetMessageId (uint32_t id);
    uint32_t GetMessageId () const;

    void SetSource (Ipv4Address src);
    Ipv4Address GetSource () const;

    void SetDestination (Ipv4Address dst);
    Ipv4Address GetDestination () const;

    void SetTimestamp (uint32_t ts);
    uint32_t GetTimestamp () const;

    void SetRecord (double rc);
    double GetRecord () const;

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const override;
    virtual void Print (std::ostream &os) const override;
    
    virtual uint32_t GetSerializedSize (void) const override;
    virtual void Serialize (Buffer::Iterator start) const override;
    virtual uint32_t Deserialize (Buffer::Iterator start) override;
};


#endif // PROPHET_HEADER_H
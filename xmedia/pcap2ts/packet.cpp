
#include "packet.hpp"

#include <inttypes.h>

#include "xlog.hpp"
#include "comm.hpp"

#define ETHERNET_TYPE_IPv4  (0x0800)
#define ETHERNET_TYPE_IPv6  (0x86dd)
#define ETHERNET_TYPE_ARP   (0x0806)

bool SharedPacketData::valid() const
{
    if (data)
    {
        if (offset < data->size() && size <= data->size())
        {
            if (data->size() - size >= offset)
            {
                return true;
            }
        }
    }
    return false;
}

const char *IPacket::typeName(PacketType packet_type)
{
    const char *str[] = {
        "None",
        "Ethernet",
        "IPv4",
        "IPv6",
        "Butt",
    };

    const std::size_t str_size = sizeof(str)/sizeof(str[0]);
    const std::size_t i_packet_type = (std::size_t)(packet_type);

    static_assert(str_size == (int)PacketType::Butt + 1, "Size check failed");

    if (i_packet_type < str_size)
    {
        return str[i_packet_type];
    }
    return "";
}

/*

| mac_dst(6) | mac_src(6) | type(2) | sub_data |

*/
int EthernetPacket::assign(SharedPacketData data)
{
    bool error = false;
    do 
    {
        EthernetStructure eth{};

        if (!data.valid())
        {
            xlog_err("Invalid data");
            error = true;
            break;
        }

        uint8_t *pdata = data.tdata();
        std::size_t size = data.tsize();

        std::size_t ethernet_header_size = eth.mac_dst.size()
            + eth.mac_src.size()
            + sizeof(eth.type);

        if (size < ethernet_header_size)
        {
            xlog_err("Size error");
            error = true;
            break;
        }

        // mac_dst
        net_copy(eth.mac_dst.data(), eth.mac_dst.size(),
            pdata, size);
        
        // mac_src
        pdata += eth.mac_dst.size();
        size -= eth.mac_dst.size();
        net_copy(eth.mac_src.data(), eth.mac_src.size(),
            pdata, size);

        // type
        pdata += eth.mac_src.size();
        size -= eth.mac_src.size();
        eth.type = net_u16(pdata, size);

        // copy
        SharedPacketData data_tmp = data;
        data_tmp.offset += ethernet_header_size;
        data_tmp.size -= ethernet_header_size;

        if (data_tmp.offset < data.offset
            || data_tmp.size > data.size)
        {
            xlog_err("Inner error");
            error = true;
            break;
        }

        _data = data_tmp;
        _eth = eth;
    }
    while (0);

    if (error)
    {
        return -1;
    }
    return 0;
}

PacketType EthernetPacket::type() const
{
    return PacketType::Ethernet;
}

SharedPacketData EthernetPacket::data() const
{
    if (!_data.valid())
    {
        return SharedPacketData();
    }
    
    return _data;
}

const EthernetStructure& EthernetPacket::eth() const
{
    return _eth;
}

EthernetSubType EthernetPacket::convertEthType(uint16_t eth_type)
{
    EthernetSubType type = EthernetSubType::None;
    switch (eth_type)
    {
        case ETHERNET_TYPE_IPv4:
        {
            type = EthernetSubType::IPv4;
            break;
        }
        case ETHERNET_TYPE_IPv6:
        {
            type = EthernetSubType::IPv6;
            break;
        }
        case ETHERNET_TYPE_ARP:
        {
            type = EthernetSubType::ARP;
            break;
        }
        default:
        {
            break;
        }
    }
    return type;
}

const char *EthernetPacket::subTypeName(EthernetSubType subtype)
{
    const char *str[] = {
        "None",
        "ARP",
        "IPv4",
        "IPv6",
        "Butt",
    };

    const std::size_t str_size = sizeof(str)/sizeof(str[0]);
    const std::size_t i_sub_type = (std::size_t)(subtype);

    static_assert(str_size == (int)EthernetSubType::Butt + 1, "Size check failed");

    if (i_sub_type < str_size)
    {
        return str[i_sub_type];
    }
    return "";
}

int IPv4EthernetPacket::assign(SharedPacketData data)
{
    bool error = false;

    do 
    {
        IPv4Structure ipv4{};

        if (!data.valid())
        {
            xlog_err("Invalid data");
            error = true;
            break;
        }

        uint8_t *pdata = data.tdata();
        std::size_t size = data.tsize();

        const std::size_t head_size_without_options = sizeof(ipv4.version_and_len)
            + sizeof(ipv4.differentialted_services_field) + sizeof(ipv4.total_length) + sizeof(ipv4.identification)
            + sizeof(ipv4.flag_and_fragment_offset) + sizeof(ipv4.time_to_live)
            + sizeof(ipv4.protocol) + sizeof(ipv4.header_checksum)
            + ipv4.ip_addr_src.size() + ipv4.ip_addr_dst.size();

        if (size < head_size_without_options)
        {
            xlog_err("Size check failed(%zu < %zu)", size, head_size_without_options);
            error = true;
            break;
        }

        static_assert(sizeof(ipv4.version_and_len) == 1, "");
        ipv4.version_and_len = net_u8(pdata, size);
        pdata += sizeof(ipv4.version_and_len);
        size -= sizeof(ipv4.version_and_len);

        ipv4.version = ipv4_version(ipv4.version_and_len);
        ipv4.len = ipv4_len(ipv4.version_and_len);

        if (ipv4.version != 4) // version = 4 --> IPv4
        {
            xlog_err("Verson check failed(%" PRIu8 ")", ipv4.version);
            error = true;
            break;
        }

        if (ipv4.len < head_size_without_options)
        {
            xlog_err("Length check failed(%" PRIu8 ", %zu)", ipv4.len, head_size_without_options);
            error = true;
            break;
        }

        if (size < ipv4.len)
        {
            xlog_err("Size check failed(%zu < %" PRIu8 ")", size, ipv4.len);
            error = true;
            break;
        }
        
        std::size_t option_len = ipv4.len - head_size_without_options;

        static_assert(sizeof(ipv4.differentialted_services_field) == 1, "");
        ipv4.differentialted_services_field = net_u8(pdata, size);
        pdata += sizeof(ipv4.differentialted_services_field);
        size -= sizeof(ipv4.differentialted_services_field);

        static_assert(sizeof(ipv4.total_length) == 2, "");
        ipv4.total_length = net_u16(pdata, size);
        pdata += sizeof(ipv4.total_length);
        size -= sizeof(ipv4.total_length);

        if (ipv4.total_length > data.tsize())
        {
            xlog_err("Total length check failed(%" PRIu16 "> %zu)", ipv4.total_length, data.tsize());
            error = true;
            break;
        }

        ipv4.padding_size = data.tsize() - ipv4.total_length;

        static_assert(sizeof(ipv4.identification) == 2, "");
        ipv4.identification = net_u16(pdata, size);
        pdata += sizeof(ipv4.identification);
        size -= sizeof(ipv4.identification);

        static_assert(sizeof(ipv4.flag_and_fragment_offset) == 2, "");
        ipv4.flag_and_fragment_offset = net_u16(pdata, size);
        pdata += sizeof(ipv4.flag_and_fragment_offset);
        size -= sizeof(ipv4.flag_and_fragment_offset);

        ipv4.flag = ipv4_flag(ipv4.flag_and_fragment_offset);
        ipv4.fragment_offset = ipv4_fragment_offset(ipv4.flag_and_fragment_offset);

        static_assert(sizeof(ipv4.time_to_live) == 1, "");
        ipv4.time_to_live = net_u8(pdata, size);
        pdata += sizeof(ipv4.time_to_live);
        size -= sizeof(ipv4.time_to_live);

        static_assert(sizeof(ipv4.protocol) == 1, "");
        ipv4.protocol = net_u8(pdata, size);
        pdata += sizeof(ipv4.protocol);
        size -= sizeof(ipv4.protocol);

        static_assert(sizeof(ipv4.header_checksum) == 2, "");
        ipv4.header_checksum = net_u16(pdata, size);
        pdata += sizeof(ipv4.header_checksum);
        size -= sizeof(ipv4.header_checksum);

        net_copy(ipv4.ip_addr_src.data(), ipv4.ip_addr_src.size(), 
            pdata, size);
        pdata += ipv4.ip_addr_src.size();
        size -= ipv4.ip_addr_src.size();

        net_copy(ipv4.ip_addr_dst.data(), ipv4.ip_addr_dst.size(), 
            pdata, size);
        pdata += ipv4.ip_addr_dst.size();
        size -= ipv4.ip_addr_dst.size();

        // skip ipv4 options
        if (option_len > 0)
        {
            pdata += option_len;
            size -= option_len;
        }

        // EthernetPacketData = IPv4Packet + [padding]
        SharedPacketData data_tmp = data;
        data_tmp.offset += ipv4.len;
        data_tmp.size = ipv4.total_length;
        if (data_tmp.offset < data.offset
            || data_tmp.size > data.size)
        {
            xlog_err("Inner error");
            error = true;
            break;
        }

        // copy
        _data = data_tmp;
        _ipv4 = ipv4;
    }
    while (0);

    if (error)
    {
        return -1;
    }

    return 0;
}

PacketType IPv4EthernetPacket::type() const
{
    return PacketType::IPv4;
}

SharedPacketData IPv4EthernetPacket::data() const
{
    if (!_data.valid())
    {
        return SharedPacketData{};
    }

    return _data;
}

const IPv4Structure& IPv4EthernetPacket::ipv4() const
{
    return _ipv4;
}

uint8_t IPv4EthernetPacket::ipv4_version(uint8_t version_and_len)
{
    return (version_and_len >> 4) & 0x0f;
}

uint8_t IPv4EthernetPacket::ipv4_len(uint8_t version_and_len)
{
    return (version_and_len & 0x0f) * 4;
}

uint16_t IPv4EthernetPacket::ipv4_flag(uint16_t flag_and_fragment_offset)
{
    return (flag_and_fragment_offset >> 5) & 0x03;
}

uint16_t IPv4EthernetPacket::ipv4_fragment_offset(uint16_t flag_and_fragment_offset)
{
    return (flag_and_fragment_offset & 0x1f);
}

Protocol IPv4EthernetPacket::convertProtocol(uint8_t ipv4_protocol)
{
    Protocol protocol = Protocol::None;
    switch (ipv4_protocol)
    {
        case 6:
        {
            protocol = Protocol::TCP;
            break;   
        }
        case 17:
        {
            protocol = Protocol::UDP;
            break;
        }
        default:
        {
            xlog_err("Unknown protocol");
            break;
        }
    }
    return protocol;
}

#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "blockchain-node.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("BlockchainNode");
    NS_OBJECT_ENSURE_REGISTERED(BlockchainNode);

    TypeId
    BlockchainNode::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::BlockchainNode")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<BlockchainNode>()
        .AddAttribute("Local",
                        "The Address on which to Bind the rx socket." ,
                        AddressValue(),
                        MakeAddressAccessor(&BlockchainNode::m_local),
                        MakeAddressChecker())
        .AddAttribute("Protocol",
                        "The type id of the protocol to us for the rx socket",
                        TypeIdValue (UdpSocketFactory::GetTypeId()),
                        MakeTypeIdAccessor (&BlockchainNode::m_tid),
                        MakeTypeIdChecker())
        .AddAttribute("InvTimeoutMinutes",
                        "The timeout of inv messages in minutes",
                        TimeValue(Minutes(20)),
                        MakeTimeAccessor(&BlockchainNode::m_invTimeoutMinutes),
                        MakeTimeChecker())
        .AddTraceSource("Rx",
                        "A packet has been received",
                        MakeTraceSourceAccessor(&BlockchainNode::m_rxTrace),
                        "ns3::Packet::AddressTracedCallback")
        ;
        return tid;
    }

    BlockchainNode::BlockchainNode (void) : m_isMiner(false), m_averageTransacionSize(522.4), m_transactionIndexSize(2), m_blockchainPort(8333), m_secondsPerMin(60), 
                                            m_countBytes(4), m_blockchainMessageHeader(90), m_inventorySizeBytes(36), m_getHeaderSizeBytes(72),
                                            m_headersSizeBytes(81), m_blockHeadersSizeBytes (81)
    {
        NS_LOG_FUNCTION(this);
        m_socket = 0;
        m_meanBlockReceiveTime = 0;
        m_previousBlockReceiveTime = 0;
        m_meanBlockPropagationTime = 0;
        m_meanBlockSize = 0;
        m_numberOfPeers = m_peersAddresses.size();
    }

    BlockchainNode::~BlockchainNode(void)
    {
        NS_LOG_FUNCTION(this);
    }

    Ptr<Socket>
    BlockchainNode::GetListeningSocket(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_socket;
    }

    std::vector<Ipv4Address>
    BlockchainNode::GetPeerAddress(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_peersAddresses;
    }

    void
    BlockchainNode::SetPeersAddresses (const std::vector<Ipv4Address> &peers)
    {
        NS_LOG_FUNCTION(this);
        m_peersAddresses = peers;
        m_numberOfPeers = m_peersAddresses.size();
    }

    void
    BlockchainNode::SetPeersDownloadSpeeds(const std::map<Ipv4Address, double> &peersDownloadSpeeds)
    {
        NS_LOG_FUNCTION(this);
        m_peersDownloadSpeeds = peersDownloadSpeeds;
    }

    void
    BlockchainNode::SetPeersUploadSpeeds(const std::map<Ipv4Address, double> &peersUploadSpeeds)
    {
        NS_LOG_FUNCTION(this);
        m_peersUploadSpeeds = peersUploadSpeeds;
    }

    void
    BlockchainNode::SetNodeInternetSpeeds(const nodeInternetSpeed &internetSpeeds)
    {
        NS_LOG_FUNCTION(this);

        m_downloadSpeed = internetSpeeds.downloadSpeed*1000000/8;
        m_uploadSpeed = internetSpeeds.uploadSpeed*1000000/8;
    }

    void
    BlockchainNode::SetNodeStats (nodeStatistics *nodeStats)
    {
        NS_LOG_FUNCTION(this);
        m_nodeStats = nodeStats;
    }

    void
    BlockchainNode::SetProtocolType(enum ProtocolType protocolType)
    {
        NS_LOG_FUNCTION(this);
        m_protocolType = protocolType;
    }

    void
    BlockchainNode::DoDispose(void)
    {
        NS_LOG_FUNCTION(this);
        m_socket = 0;

        Application::DoDispose();
    }

    void
    BlockchainNode::StartApplication()
    {
        NS_LOG_FUNCTION(this);

        srand(time(NULL) + GetNode()->GetId());
        NS_LOG_INFO("Node" << GetNode()->GetId() << ": download speed = " << m_downloadSpeed << "B/s");
        NS_LOG_INFO("Node" << GetNode()->GetId() << ": upload speed = " << m_uploadSpeed << "B/s");
        NS_LOG_INFO("Node" << GetNode()->GetId() << ": m_numberOfPeers = " << m_numberOfPeers);
        NS_LOG_INFO("Node" << GetNode()->GetId() << ": m_protocolType = " << getProtocolType(m_protocolType));

        NS_LOG_INFO("Node" << GetNode()->GetId() << ": My peers are");

        for(auto it = m_peersAddresses.begin(); it != m_peersAddresses.end(); it++)
        {
            NS_LOG_INFO("\t" << *it);
        }

        if(!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), m_tid);
            m_socket->Bind(m_local);
            m_socket->Listen();
            m_socket->ShutdownSend();
            if(addressUtils::IsMulticast(m_local))
            {
                Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
                if(udpSocket)
                {
                    udpSocket->MulticastJoinGroup(0, m_local);
                }
                else
                {
                    NS_FATAL_ERROR("Error : joining multicast on a non-UDP socket");
                }
            }
        }

        m_socket->SetRecvCallback(MakeCallback(&BlockchainNode::HandleRead, this));
        m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                    MakeCallback(&BlockchainNode::HandleAccept, this));
        m_socket->SetCloseCallbacks(MakeCallback(&BlockchainNode::HandlePeerClose, this),
                                    MakeCallback(&BlockchainNode::HandlePeerError, this));
        
        NS_LOG_DEBUG("Node" << GetNode()->GetId() << ":Before creating sockets");
        for(std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
        {
            m_peersSokcets[*i] = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
            m_peersSokcets[*i]->Connect(InetSocketAddress(*i, m_blockchainPort));
        }
        NS_LOG_DEBUG("Node" << GetNode()->GetId()<<": After creating sockets");

        m_nodeStats->nodeId = GetNode()->GetId();
        m_nodeStats->meanBlockReceiveTime = 0;
        m_nodeStats->meanBlockPropagationTime = 0;
        m_nodeStats->meanBlockSize = 0;
        m_nodeStats->totalBlocks = 0;
        m_nodeStats->miner = 0;
        m_nodeStats->minerGeneratedBlocks = 0;
        m_nodeStats->minerAverageBlockGenInterval = 0;
        m_nodeStats->minerAverageBlockSize = 0;
        m_nodeStats->hashRate = 0;
        m_nodeStats->blockReceivedByte = 0;
        m_nodeStats->blockSentByte = 0;
        m_nodeStats->longestFork = 0;
        m_nodeStats->blocksInForks = 0;
        m_nodeStats->connections = m_peersAddresses.size();

    }

    void
    BlockchainNode::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        for(std::vector<Ipv4Address>::iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
        {
            m_peersSokcets[*i]->Close();
        }

        if(m_socket)
        {
            m_socket->Close();
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }

        NS_LOG_WARN("\n\nBLOCKCHAIN NODE " <<GetNode()->GetId() << ":");
        //NS_LOG_WARN("Current Top Block is \n"<<*(m_blockchain.GetCurrentTopBlock()));
        //NS_LOG_WARN("Current Blockchain is \n" << m_blockchain);

        NS_LOG_WARN("Mean Block Receive Time = " << m_meanBlockReceiveTime << " or "
                    << static_cast<int>(m_meanBlockReceiveTime)/m_secondsPerMin << "min and "
                    << m_meanBlockReceiveTime - static_cast<int>(m_meanBlockReceiveTime)/m_secondsPerMin * m_secondsPerMin << "s");
        NS_LOG_WARN("Mean Block Propagation Time = " << m_meanBlockPropagationTime << "s");
        NS_LOG_WARN("Mean Block Size = " << m_meanBlockSize << "Bytes");
        NS_LOG_WARN("Total Block = " << m_blockchain.GetTotalBlocks());
        NS_LOG_WARN("Received But Not Validataed size : " << m_receivedNotValidated.size());
        NS_LOG_WARN("m_sendBlockTime size = " <<m_receiveBlockTimes.size());
        
    }

    void
    BlockchainNode::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_INFO(this << socket);
        Ptr<Packet> packet;
        Address from;

        //double newBlockReceiveTime = Simulator::Now().GetSeconds();

        while((packet = socket->RecvFrom(from)))
        {
            if(packet->GetSize() == 0)
            {
                break;
            }

            if(InetSocketAddress::IsMatchingType(from))
            {
                /*
                 * We may receive more than one packets simultaneously on the socket,
                 * so we have to parse each one of them.
                 */
                std::string delimiter = "#";
                std::string parsedPacket;
                size_t pos = 0;
                char *packetInfo = new char[packet->GetSize() + 1];
                std::ostringstream totalStream;
                
                packet->CopyData(reinterpret_cast<uint8_t *>(packetInfo), packet->GetSize());
                packetInfo[packet->GetSize()] = '\0';

                totalStream << m_bufferedData[from] << packetInfo;
                std::string totalReceivedData(totalStream.str());
                NS_LOG_INFO("Node " << GetNode()->GetId() << "Total Received Data : " << totalReceivedData);

                while((pos = totalReceivedData.find(delimiter)) != std::string::npos)
                {
                    parsedPacket = totalReceivedData.substr(0,pos);
                    NS_LOG_INFO("Node " << GetNode()->GetId() << " Parsed Packet: " << parsedPacket);
                    
                    rapidjson::Document d;
                    d.Parse(parsedPacket.c_str());

                    if(!d.IsObject())
                    {
                        NS_LOG_WARN("The parsed packet is corrupted");
                        totalReceivedData.erase(0, pos + delimiter.length());
                        continue;
                    }

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    d.Accept(writer);

                    NS_LOG_INFO("At time " << Simulator::Now().GetSeconds()
                                << "s Blockchain node " << GetNode()->GetId() << " received"
                                << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                << " port " << InetSocketAddress::ConvertFrom(from).GetPort()
                                << " with info = " << buffer.GetString());

                    switch(d["message"].GetInt())
                    {
                        case INV:
                        {
                            NS_LOG_INFO("INV");
                            unsigned int j;
                            std::vector<std::string>            requestBlocks;
                            std::vector<std::string>::iterator  block_it;

                            m_nodeStats->invReceivedBytes += m_blockchainMessageHeader + m_countBytes + d["inv"].Size()*m_inventorySizeBytes; 

                            for(j = 0; j < d["inv"].Size() ; j++)
                            {
                                std::string invDlimiter = "/";
                                std::string parsedInv = d["inv"][j].GetString();
                                size_t invPos = parsedInv.find(invDlimiter);
                                EventId timeout;

                                int height = atoi(parsedInv.substr(0, invPos).c_str());
                                int minerId = atoi(parsedInv.substr(invPos+1, parsedInv.size()).c_str());

                                if(m_blockchain.HasBlock(height, minerId) || m_blockchain.IsOrphan(height, minerId) || ReceivedButNotValidated(parsedInv))
                                {
                                    NS_LOG_INFO("INV : Blockchain node " << GetNode()->GetId()
                                                << " has already received the block with height = "
                                                << height << " and minerId = " << minerId);
                                }
                                else
                                {
                                    NS_LOG_INFO("INV : Blockchain node " << GetNode()->GetId()
                                                << " does not have the block with height = "
                                                << height << " and minerId = " << minerId);

                                    /*
                                     * check if we have already requested the block
                                     */
                                    if(m_invTimeouts.find(parsedInv) == m_invTimeouts.end())
                                    {
                                        NS_LOG_INFO("INV: Blockchain node " << GetNode()->GetId()
                                                    << " has not requested the block yet");
                                        requestBlocks.push_back(parsedInv);
                                        timeout = Simulator::Schedule(m_invTimeoutMinutes, &BlockchainNode::InvTimeoutExpired, this, parsedInv);
                                        m_invTimeouts[parsedInv] = timeout;
                                    }
                                    else
                                    {
                                        NS_LOG_INFO("INV : Blockchain node " << GetNode()->GetId()
                                                    << " has already requested the block");
                                    }

                                    m_queueInv[parsedInv].push_back(from);
                                }
                            }

                            
                            if(!requestBlocks.empty())
                            {
                                rapidjson::Value value;
                                rapidjson::Value array(rapidjson::kArrayType);
                                d.RemoveMember("inv");

                                for(block_it = requestBlocks.begin(); block_it < requestBlocks.end(); block_it++)
                                {
                                    value.SetString(block_it->c_str(), block_it->size(), d.GetAllocator());
                                    array.PushBack(value, d.GetAllocator());
                                }

                                d.AddMember("blocks", array, d.GetAllocator());

                                SendMessage(INV, GET_HEADERS, d, from );
                                SendMessage(INV, GET_DATA, d, from );
                            }
                            
                            break;
                        }
                        case TRANSACTION:
                        {
                            
                            break;
                        }
                        case GET_HEADERS:
                        {
                            unsigned int j;
                            std::vector<Block>              requestHeaders;
                            std::vector<Block>::iterator    block_it;

                            m_nodeStats->getHeadersReceivedBytes += m_blockchainMessageHeader + m_getHeaderSizeBytes;

                            for(j =0 ; j < d["blocks"].Size(); j++)
                            {
                                std::string invDlimiter = "/";
                                std::string blockHash = d["blocks"][j].GetString();
                                size_t      invPos = blockHash.find(invDelimiter);

                                int height = atoi(blockHash.substr(0, invPos).c_str());
                                int minerId = atoi(blockHash.substr(invPos+1, blockHash.size()).c_str());

                                if(m_blockchain.HasBlock(height, minerId) || m_blockchain.IsOrphan(height, minerId))
                                {
                                    NS_LOG_INFO("GET_HEADERS: Blockchain node " << GetNode()->GetId()
                                                << " has the block with height = " << height
                                                << " and minerId = " << minerId);
                                    Block newBlock(m_blockchain.ReturnBlock(height, minerId));
                                    requestHeaders.push_back(newBlock);

                                } 
                                else if (ReceivedButNotValidated(blockHash))
                                {
                                    NS_LOG_INFO("GET_HEADERS: Blockchain node " << GetNode()->GetId()
                                                << " has received but not yet validated the block with height = "
                                                << height << " and minerId = " << minerId);
                                    requestHeaders.push_back(m_receivedNotValidated[blockHash]);
                                }
                                else
                                {
                                    NS_LOG_INFO("GET_HEADERS: Blockchain node " << GetNode()->GetId()
                                                << " does not have the full block with height = "
                                                << height << " and minerId = " << minerId);
                                }
                            }

                            if(!requestHeaders.empty())
                            {
                                rapidjson::Value value;
                                rapidjson::Value array(rapidjson::kArrayType);

                                d.RemoveMember("blocks");

                                for(block_it = requestHeaders.begin() ; block_it < requestHeaders.end(); block_it++)
                                {
                                    rapidjson::Value blockInfo(rapidjson::kObjectType);
                                    NS_LOG_INFO("In requestHeaders " << *block_it);

                                    value = block_it->GetBlockHeight();
                                    blockInfo.AddMember("height", value, d.GetAllocator());
                                    
                                    value = block_it->GetMinerId();
                                    blockInfo.AddMember("minerId", value, d.GetAllocator());

                                    value = block_it->GetNonce();
                                    blockInfo.AddMember("nonce", value, d.GetAllocator());

                                    value = block_it->GetParentBlockMinerId();
                                    blockInfo.AddMember("parentBlockMinerId", value, d.GetAllocator());

                                    value = block_it->GetBlockSizeBytes();
                                    blockInfo.AddMember("size", value, d.GetAllocator());

                                    value = block_it->GetTimeStamp();
                                    blockInfo.AddMember("timeStamp", value, d.GetAllocator());

                                    value = block_it->GetTimeReceived();
                                    blockInfo.AddMember("timeReceived", value, d.GetAllocator());

                                    array.PushBack(blockInfo, d.GetAllocator());
                                
                                }

                                d.AddMember("blocks", array, d.GetAllocator());

                                SendMessage(GET_HEADERS, HEADERS, d, from);

                            }

                            break;
                        }
                        case HEADERS:
                        {
                            
                            break;
                        }
                        case GET_BLOCK:
                        {
                            
                            break;
                        }            
                        case BLOCK:
                        {
                            NS_LOG_INFO("BLOCK");
                            int blockMessageSize = 0;
                            double receiveTime;
                            double eventTime = 0;
                            double minSpeed = std::min(m_downloadSpeed, m_peersUploadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4()]*1000000/8);

                            blockMessageSize += m_blockchainMessageHeader;

                            for(unsigned int j = 0; j < d["blocks"].Size(); j++)
                            {
                                blockMessageSize += d["blocks"][j]["size"].GetInt();
                            }

                            m_nodeStats->blockReceivedByte += blockMessageSize;

                            rapidjson::StringBuffer blockInfo;
                            rapidjson::Writer<rapidjson::StringBuffer> blockWriter(blockInfo);
                            d.Accept(blockWriter);

                            NS_LOG_INFO("BLOCK: At time " << Simulator::Now().GetSeconds()
                                        << " Node " << GetNode()->GetId()
                                        << " received a block message " << blockInfo.GetString());
                            NS_LOG_INFO(m_downloadSpeed << " " 
                                        << m_peersUploadSpeeds[InetSocketAddress::ConvertFrom(from). GetIpv4()] * 1000000/8 << " " << minSpeed);

                            std::string help = blockInfo.GetString();

                            if(m_receiveBlockTimes.size() == 0 || Simulator::Now().GetSeconds() > m_receiveBlockTimes.back())
                            {
                                receiveTime = blockMessageSize / m_downloadSpeed;
                                eventTime = blockMessageSize / minSpeed;
                            }
                            else
                            {
                                receiveTime = blockMessageSize / m_downloadSpeed + m_receiveBlockTimes.back() - Simulator::Now().GetSeconds();
                                eventTime = blockMessageSize / minSpeed + m_receiveBlockTimes.back() - Simulator::Now().GetSeconds();
                            }

                            m_receiveBlockTimes.push_back(Simulator::Now().GetSeconds()+receiveTime);

                            Simulator::Schedule(Seconds(eventTime), &BlockchainNode::ReceivedBlockMessage, this, help, from);
                            Simulator::Schedule(Seconds(receiveTime), &BlockchainNode::RemoveReceiveTime, this);
                            NS_LOG_INFO("BLOCK: Node " << GetNode()->GetId() << " will receive the full block message at "
                                        << Simulator::Now().GetSeconds() + eventTime);

                            break;
                        }
                        case GET_DATA:
                        {
                            break;
                        }
                    }

                    totalReceivedData.erase(0, pos + delimiter.length());

                }

                m_bufferedData[from] = totalReceivedData;
                delete[] packetInfo;

            }
            else if(InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_INFO("At time " << Simulator::Now().GetSeconds()
                            << " s blockchain node " << GetNode()->GetId() << " received"
                            << packet->GetSize() << " bytes from"
                            << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                            << " port" << Inet6SocketAddress::ConvertFrom(from).GetPort());
            }
            m_rxTrace(packet, from);
        }
        
    }

    void
    BlockchainNode::HandleAccept(Ptr<Socket> socket, const Address& from)
    {

    }

    void
    BlockchainNode::HandlePeerClose(Ptr<Socket> socket)
    {

    }

    void
    BlockchainNode::HandlePeerError(Ptr<Socket> socket)
    {

    }

    void
    BlockchainNode::ReceivedBlockMessage(std::string &blockInfo, Address &from)
    {

    }

    void
    BlockchainNode::ReceiveBlock(const Block &newBlock)
    {

    }

    void
    BlockchainNode::SendBlock(std::string packetInfo, Address &from)
    {

    }

    void
    BlockchainNode::ReceivedHigherBlock(const Block &newBlock)
    {

    }

    void
    BlockchainNode::ValidateBlock(const Block &newBlock)
    {

    }

    void
    BlockchainNode::AfterBlockValidation(const Block &newBlock)
    {
        
    }

    void
    BlockchainNode::ValidateOrphanChildren(const Block &newBlock)
    {

    }

    void
    BlockchainNode::AdvertiseNewBlock(const Block &newBlock)
    {

    }

    void
    BlockchainNode::SendMessage(enum Messages receivedMessage, enum Messages responseMessage, rapidjson::Document &d, Ptr<Socket> outgoingSocket)
    {

    }

    void
    BlockchainNode::SendMessage(enum Messages receivedMessage, enum Messages responseMessage, rapidjson::Document &d, Address &outgoingAddres)
    {

    }

    void
    BlockchainNode::SendMessage(enum Messages receivedMessage, enum Messages responseMessage, std::string packet, Address &outgoingAddress)
    {

    }

    void
    BlockchainNode::InvTimeoutExpired(std::string blockHash)
    {

    }

    bool
    BlockchainNode::ReceivedButNotValidated(std::string blockHash)
    {
        return true;
    }

    void
    BlockchainNode::RemoveReceivedButNotvalidated(std::string blockHash)
    {

    }

    bool
    BlockchainNode::OnlyHeadersReceived (std::string blockHash)
    {
        return true;
    }

    void
    BlockchainNode::RemoveSendTime()
    {

    }

    void
    BlockchainNode::RemoveCompressedBlockSendTime()
    {

    }

    void
    BlockchainNode::RemoveReceiveTime()
    {

    }

    void 
    BlockchainNode::RemoveCompressedBlockReceiveTime()
    {

    }


}
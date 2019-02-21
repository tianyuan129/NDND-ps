#include <ndn-cxx/face.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <chrono>
#include <ndn-cxx/util/sha256.hpp>
#include "nd-packet-format.h"
#include "nfd-command-tlv.h"
#include <ndn-cxx/encoding/tlv.hpp>
using namespace ndn;
using namespace ndn::ndnd;
using namespace std;


class NDNDClient{
public:
  
  void send_rib_register_interest(const Name& route_name, int face_id) {
    Interest interest(Name("/localhost/nfd/rib/register"));
    interest.setInterestLifetime(5_s);
    interest.setMustBeFresh(true);
    Block control_params = make_rib_register_interest_parameter(route_name, face_id);
    const_cast<Name&>(interest.getName()).append(control_params);
    interest.setParameters(m_buffer, m_len);
    interest.setNonce(4);
    interest.setCanBePrefix(false);

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const_cast<Name&>(interest.getName()).appendNumber(now);
    const_cast<Name&>(interest.getName()).appendNumber(now);
    
    m_keyChain.sign(interest);
    
    std::cerr << "Name of interest sent:" << std::endl;
    std::cerr << interest.getName().toUri() << std::endl;
    
    m_face.expressInterest(
      interest, 
      bind(&NDNDClient::onRegisterRouteDataReply, this,  _1, _2),
      bind(&NDNDClient::onNack, this,  _1, _2),
      bind(&NDNDClient::onTimeout, this,  _1));  
  }

  
  void run(){
    Interest interest(Name("/ndn/nd"));
    interest.setInterestLifetime(30_s);
    interest.setMustBeFresh(true);
    make_NDND_interest_parameter();
    interest.setParameters(m_buffer, m_len);
    interest.setNonce(4);
    interest.setCanBePrefix(false);

    name::Component parameterDigest = name::Component::fromParametersSha256Digest(
      util::Sha256::computeDigest(interest.getParameters().wire(), interest.getParameters().size()));

    const_cast<Name&>(interest.getName()).append(parameterDigest);

    m_face.expressInterest(
      interest, 
      bind(&NDNDClient::onData, this,  _1, _2),
      bind(&NDNDClient::onNack, this,  _1, _2),
      bind(&NDNDClient::onTimeout, this,  _1));

    m_face.processEvents();
  }

// private:
  void onData(const Interest& interest, const Data& data){
    std::cout << data << std::endl;

    size_t dataSize = data.getContent().value_size();
    auto pResult = reinterpret_cast<const RESULT*>(data.getContent().value());
    int iNo = 1;
    Name name;

    while((uint8_t*)pResult < data.getContent().value() + dataSize){
      m_len = sizeof(RESULT);
      printf("-----%2d-----\n", iNo);
      printf("IP: %s\n", inet_ntoa(*(in_addr*)(pResult->IpAddr)));
      printf("Port: %hu\n", ntohs(pResult->Port));
      printf("Subnet Mask: %s\n", inet_ntoa(*(in_addr*)(pResult->SubnetMask)));

      auto result = Block::fromBuffer(pResult->NamePrefix, data.getContent().value() + dataSize - pResult->NamePrefix);
      name.wireDecode(std::get<1>(result));
      printf("Name Prefix: %s\n", name.toUri().c_str());
      m_len += std::get<1>(result).size();

      pResult = reinterpret_cast<const RESULT*>(((uint8_t*)pResult) + m_len);
      iNo ++;
    }

    Name name_test("test");
    send_rib_register_interest(name_test, 259);
  }

  void onNack(const Interest& interest, const lp::Nack& nack){
    std::cout << "received Nack with reason " << nack.getReason()
              << " for interest " << interest << std::endl;
  }

  void onTimeout(const Interest& interest){
    std::cout << "Timeout " << interest << std::endl;
  }


  Block make_rib_register_interest_parameter(const Name& route_name, int face_id) {
    
    auto block = makeEmptyBlock(CONTROL_PARAMETERS);
    
    Block route_name_block = route_name.wireEncode();    
    
    Block face_id_block =
      makeNonNegativeIntegerBlock(FACE_ID, face_id);

    Block origin_block =
      makeNonNegativeIntegerBlock(ORIGIN, 0xFF);

    Block cost_block =
      makeNonNegativeIntegerBlock(COST, 0);

    Block flags_block =
      makeNonNegativeIntegerBlock(FLAGS, 0x01);
    
    block.push_back(route_name_block);
    block.push_back(face_id_block);
    block.push_back(origin_block);
    block.push_back(cost_block);
    block.push_back(flags_block);
    
    std::cerr << "Route name block:" << std::endl;
    std::cerr << route_name_block << std::endl;
    std::cerr << "Face id block:" << std::endl;
    std::cerr << face_id_block << std::endl;
    
    std::cerr << "Control parameters block:" << std::endl;
    std::cerr << block << std::endl;
    
    block.encode();  

    return block;
    
  }
  
  void make_NDND_interest_parameter(){
    auto pParam = reinterpret_cast<PPARAMETER>(m_buffer);
    m_len = sizeof(PARAMETER);

    pParam->V4 = 1;
    memcpy(pParam->IpAddr, &m_IP, sizeof(in_addr_t));
    pParam->Port = m_port;
    memcpy(pParam->SubnetMask, &m_submask, sizeof(in_addr_t));
    pParam->TTL = 30 * 1000; //ms
    pParam->TimeStamp = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    Block block = m_namePrefix.wireEncode();
    memcpy(pParam->NamePrefix, block.begin().base(), block.end() - block.begin());
    m_len += block.end() - block.begin();
  }

  void onRegisterRouteDataReply(const Interest& interest, const Data& data){
    Block response_block = data.getContent().blockFromValue();
    response_block.parse();
    
    std::cerr << response_block << std::endl;

    Block status_code_block = response_block.get(STATUS_CODE);
    Block status_text_block = response_block.get(STATUS_TEXT);
    short response_code = readNonNegativeIntegerAs<int>(status_code_block);
    char response_text[1000] = {0};
    memcpy(response_text, status_text_block.value(), status_text_block.value_size());
      
    if (response_code == OK) {

      Block control_params = response_block.get(CONTROL_PARAMETERS);
      control_params.parse();
      
      Block name_block = control_params.get(ndn::tlv::Name);
      Name route_name(name_block);
      Block face_id_block = control_params.get(FACE_ID);
      int face_id = readNonNegativeIntegerAs<int>(face_id_block);
      Block origin_block = control_params.get(ORIGIN);
      int origin = readNonNegativeIntegerAs<int>(origin_block);
      Block route_cost_block = control_params.get(COST);
      int route_cost = readNonNegativeIntegerAs<int>(route_cost_block);
      Block flags_block = control_params.get(FLAGS);
      int flags = readNonNegativeIntegerAs<int>(flags_block);
      
      std::cout << "\nRegistration of route succeeded:" << std::endl;
      std::cout << "Status text: " << response_text << std::endl;

      std::cout << "Route name: " << route_name.toUri() << std::endl;
      std::cout << "Face id: " << face_id << std::endl;
      std::cout << "Origin: " << origin << std::endl;
      std::cout << "Route cost: " << route_cost << std::endl;
      std::cout << "Flags: " << flags << std::endl; 
      
    }
    else {
      std::cout << "\nRegistration of route failed." << std::endl;
      std::cout << "Status text: " << response_text << std::endl;
      
    }

  }

  void onAddFaceDataReply(const Interest& interest, const Data& data) {
    short response_code;
    char response_text[1000] = {0};
    char buf[1000]           = {0};   // For parsing
    int face_id;                      // Store faceid for deletion of face
    Block response_block = data.getContent().blockFromValue();
    response_block.parse();

    Block status_code_block =       response_block.get(STATUS_CODE);
    Block status_text_block =       response_block.get(STATUS_TEXT);
    Block status_parameter_block =  response_block.get(CONTROL_PARAMETERS);
    memcpy(buf, status_code_block.value(), status_code_block.value_size());
    response_code = *(short *)buf;
    memcpy(response_text, status_text_block.value(), status_text_block.value_size());

    // Get FaceId for future removal of the face
    status_parameter_block.parse();
    Block face_id_block = status_parameter_block.get(FACE_ID);
    memset(buf, 0, sizeof(buf));
    memcpy(buf, face_id_block.value(), face_id_block.value_size());
    face_id = ntohs(*(int *)buf);

    std::cout << response_code << " " << response_text << ": Added Face (FaceId: " 
              << face_id << ")" << std::endl;
  }

  void onDestroyFaceDataReply(const Interest& interest, const Data& data) {
    short response_code;
    char response_text[1000] = {0};
    char buf[1000]           = {0};   // For parsing
    int face_id;
    Block response_block = data.getContent().blockFromValue();
    response_block.parse();

    Block status_code_block =       response_block.get(STATUS_CODE);
    Block status_text_block =       response_block.get(STATUS_TEXT);
    Block status_parameter_block =  response_block.get(CONTROL_PARAMETERS);
    memcpy(buf, status_code_block.value(), status_code_block.value_size());
    response_code = *(short *)buf;
    memcpy(response_text, status_text_block.value(), status_text_block.value_size());

    status_parameter_block.parse();
    Block face_id_block = status_parameter_block.get(FACE_ID);
    memset(buf, 0, sizeof(buf));
    memcpy(buf, face_id_block.value(), face_id_block.value_size());
    face_id = ntohs(*(int *)buf);

    std::cout << response_code << " " << response_text << ": Destroyed Face (FaceId: " 
              << face_id << ")" << std::endl;
  }

  void addFace(string uri) {
    Name n("/localhost/nfd/faces/create");
    auto control_block = makeEmptyBlock(CONTROL_PARAMETERS);
    control_block.push_back(makeStringBlock(URI, uri));
    control_block.encode();
    n.append(control_block);
    Interest interest(n);

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const_cast<Name&>(interest.getName()).appendNumber(now);
    const_cast<Name&>(interest.getName()).appendNumber(now);
    
    m_keyChain.sign(interest);

    m_face.expressInterest(
      interest, 
      bind(&NDNDClient::onAddFaceDataReply, this, _1, _2),
      bind(&NDNDClient::onNack, this, _1, _2),
      bind(&NDNDClient::onTimeout, this, _1));
  }

  void destroyFace(int face_id) {
    Name n("/localhost/nfd/faces/destroy");
    auto control_block = makeEmptyBlock(CONTROL_PARAMETERS);
    control_block.push_back(makeNonNegativeIntegerBlock(FACE_ID, face_id));
    control_block.encode();
    n.append(control_block);
    Interest interest(n);

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const_cast<Name&>(interest.getName()).appendNumber(now);
    const_cast<Name&>(interest.getName()).appendNumber(now + 1);
    
    m_keyChain.sign(interest);
    cout << interest.getName().toUri() << endl;

    m_face.expressInterest(
      interest, 
      bind(&NDNDClient::onDestroyFaceDataReply, this, _1, _2),
      bind(&NDNDClient::onNack, this, _1, _2),
      bind(&NDNDClient::onTimeout, this, _1));
  }

public:
  Face m_face;
  KeyChain m_keyChain;
  Name m_namePrefix;
  in_addr m_IP;
  in_addr m_submask;
  uint16_t m_port;
  uint8_t m_buffer[4096];
  size_t m_len;
};
NDNDClient *g_pClient;

int main(int argc, char *argv[]){
  g_pClient = new NDNDClient();

  inet_aton(argv[1], &g_pClient->m_IP);
  sscanf(argv[2], "%hu", &g_pClient->m_port);
  g_pClient->m_port = htons(g_pClient->m_port);
  inet_aton("255.255.255.0", &g_pClient->m_submask);
  g_pClient->m_namePrefix = Name("/test/01/02");

  try {
    g_pClient->run();
    //g_pClient->addFace(string("udp4://127.0.1.1:6363"));
    g_pClient->m_face.processEvents();
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  delete g_pClient;
  return 0;
}

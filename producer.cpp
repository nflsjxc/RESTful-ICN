#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <boost/asio/io_service.hpp>
#include <iostream>
#include <ctime>
#include <ndn-cxx/lp/pit-token.hpp>
#include "assist.hpp"
#include "aux.hpp"

// Enclosing code in ndn simplifies coding (can also use `using namespace ndn`)
namespace ndn {
// Additional nested namespaces should be used to prevent/limit name conflicts
namespace examples {


class Producer
{
public:
  void
  run()
  {
    auto name=Name("/example/testApp");
    m_face.setInterestFilter(name,
                             std::bind(&Producer::onInterest, this, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             std::bind(&Producer::onRegisterFailed, this, _1, _2));

    auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
    m_certServeHandle = m_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
                                                 [this, cert] (auto&&...) {
                                                   m_face.put(cert);
                                                 },
                                                 std::bind(&Producer::onRegisterFailed, this, _1, _2));
    m_ioService.run();
  }

private:
  void app_data()
  {
    //to be done...
  }
  void full_hello()
  {
    std::cout << "At reflect: \n";
    auto lpPitToken = m_interest.getTag<lp::PitToken>();
    if(lpPitToken == nullptr)
    {
      std::cout<<"no pit token\n";
      return ;
    }
    uint32_t pitToken = readPitToken(*lpPitToken);


    auto reflectInterest = Interest(Name("/client/material/9999",true));  //9999 means reflexive interest from producer to consumer
    auto lpPitToken2 = setPitToken(pitToken);
    reflectInterest.setTag(make_shared<lp::PitToken>(lpPitToken2));

    std::cout <<"reflect interest from producer: "<<reflectInterest << std::endl;
    m_face.expressInterest(
      reflectInterest,
      std::bind(&Producer::datacallback, this, _1, _2),
                std::bind(&Producer::nackcallback, this, _1, _2),
                std::bind(&Producer::timeoutcallback, this, _1));

    
  }

   void reflect2(const Interest& interest)
  {
    std::cout << "At reflect2: \n";
    auto lpPitToken = interest.getTag<lp::PitToken>();
    if(lpPitToken == nullptr)
    {
      std::cout<<"no pit token\n";
      return ;
    }
    uint32_t pitToken = readPitToken(*lpPitToken);


    auto reflectInterest = Interest(Name("/testApp/reflect2/9999",true));
    auto lpPitToken2 = setPitToken(pitToken);
    reflectInterest.setTag(make_shared<lp::PitToken>(lpPitToken2));

    std::cout <<"reflect interest from producer: "<<reflectInterest << std::endl;
    m_face.expressInterest(
      reflectInterest,
      std::bind(&Producer::datacallback, this, _1, _2),
                std::bind(&Producer::nackcallback, this, _1, _2),
                std::bind(&Producer::timeoutcallback, this, _1));

    
  }

  int check_round(Interest interest)
  {
    auto name = interest.getName();
    for(size_t i = 0; i < name.size(); i++)
    {
        if(name[i]==Name("hello")[0])
        {
            return 1;
        }
        if(name[i]==Name("connect")[0])
        {
            return 2;
        }
        if(name[i]==Name("SessionID")[0])
        {
            return 3;
        }
    }
    return -1;
  }

  void hello_reject()
  {
    auto data = std::make_shared<Data>();
    data->setName(m_interest.getName());
    data->setFreshnessPeriod(10_s);
    message msg;
    msg.decode("type=HELLO-REJECT;");
    data->setContent(msg.encode());
    //send data
    m_keyChain.sign(*data);
    m_face.put(*data);
  }

  void onInterest(const Interest& interest)
  {
    if(!flag)
    {
      std::cout << ">> Incoming interest: " << interest << std::endl;
      std::cout << interest<<'\n';
      std::cout<<"interest pit-token: "<<readInterestPitToken(interest)<<'\n';
      std::cout << "<< Outcomming Reflexive Intereset\n";
      
      m_interest=interest;

      //check which round?
      int stage = check_round(interest);
      switch(stage)
      {
        case 1:
            hello_reject();
        case 2:
            full_hello();
        case 3:
            app_data();
      }
    //   flag=true;
    }
    else
    {
      std::cout << ">> Incoming interest: " << interest << std::endl;
      std::cout << interest.getName()<<'\n';
      std::cout << "Won't respond...\n";
    }

  }

  //reply data for initial interest
  void replyData()
  {
    //Prepare final answer:
    auto data = std::make_shared<Data>();
    data->setName(m_interest.getName());
    data->setFreshnessPeriod(10_s);
    m_keyChain.sign(*data);
    std::cout << "<<Final Data(D1): " << *data << std::endl;
    m_face.put(*data);
  }

  void
  onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix '" << prefix
              << "' with the local forwarder (" << reason << ")\n";
    m_face.shutdown();
  }

   void datacallback(const Interest& interest, const Data& data)
    {
        std::cout << "Received Data: \n" << data << std::endl;
        auto msg = message();
        std::string content = readString(data.getContent());
        msg.decode(content);
        if(msg.data["type"]=="HELLO-REJECT")
        {
          hello_accept();
        }
    }
    void nackcallback(const Interest&, const lp::Nack& nack) const
    {
        std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
    }
    void timeoutcallback(const Interest& interest) const
    {
        std::cout << "Timeout for " << interest << std::endl;
    }
    void RIcallback(const Interest& interest, const Interest& Rinterest)
    {
        std::cout << "Received Reflexive Interest " << Rinterest << std::endl;
    }

    void hello_accept()
    {
      auto data = std::make_shared<Data>();
      data->setName(m_interest.getName());
      data->setFreshnessPeriod(10_s);

      auto msg = generate_hello_accept();
      data->setContent(msg.encode()); 

      m_keyChain.sign(*data);
      std::cout <<"At hello acccpt: "<<*data << std::endl;
      m_face.put(*data);
    }

private:
  boost::asio::io_service m_ioService;
  Face m_face{m_ioService};
  KeyChain m_keyChain;
  ScopedRegisteredPrefixHandle m_certServeHandle;
  Scheduler m_scheduler{m_ioService};
  Interest m_interest; //initial interest
  int m_incoming_RD=0;
  bool flag =false;
};

// class Producer
// {
// public:
//   void
//   run()
//   {
//     m_face.setInterestFilter("/example/testApp",
//                              std::bind(&Producer::onInterest, this, _2),
//                              nullptr, // RegisterPrefixSuccessCallback is optional
//                              std::bind(&Producer::onRegisterFailed, this, _1, _2));

//     auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
//     m_certServeHandle = m_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
//                                                  [this, cert] (auto&&...) {
//                                                    m_face.put(cert);
//                                                  },
//                                                  std::bind(&Producer::onRegisterFailed, this, _1, _2));
//     m_face.processEvents();
//   }

// private:
//   void
//   onInterest(const Interest& interest)
//   {
//     std::cout << ">> I: " << interest << std::endl;

//     sleep(1);
// /*
//     // Create Data packet
//     auto data = std::make_shared<Data>();
//     data->setName(interest.getName());
//     data->setFreshnessPeriod(10_s);
//     data->setContent("Hello, world!");

//     // In order for the consumer application to be able to validate the packet, you need to setup
//     // the following keys:
//     // 1. Generate example trust anchor
//     //
//     //         ndnsec key-gen /example
//     //         ndnsec cert-dump -i /example > example-trust-anchor.cert
//     //
//     // 2. Create a key for the producer and sign it with the example trust anchor
//     //
//     //         ndnsec key-gen /example/testApp
//     //         ndnsec sign-req /example/testApp | ndnsec cert-gen -s /example -i example | ndnsec cert-install -

//     // Sign Data packet with default identity
//     m_keyChain.sign(*data);


//     // m_keyChain.sign(*data, signingByIdentity(<identityName>));
//     // m_keyChain.sign(*data, signingByKey(<keyName>));
//     // m_keyChain.sign(*data, signingByCertificate(<certName>));
//     // m_keyChain.sign(*data, signingWithSha256());

//     // Return Data packet to the requester
//     std::cout << "<< D: " << *data << std::endl;
//     m_face.put(*data);
//   */
//   }

//   void
//   onRegisterFailed(const Name& prefix, const std::string& reason)
//   {
//     std::cerr << "ERROR: Failed to register prefix '" << prefix
//               << "' with the local forwarder (" << reason << ")\n";
//     m_face.shutdown();
//   }

//    void datacallback(const Interest& interest, const Data& data)
//     {
//         std::cout << "Received Data " << data << std::endl;
//     }
//     void nackcallback(const Interest&, const lp::Nack& nack) const
//     {
//         std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
//     }
//     void timeoutcallback(const Interest& interest) const
//     {
//         std::cout << "Timeout for " << interest << std::endl;
//     }
//     void RIcallback(const Interest& interest, const Interest& Rinterest)
//     {
//         std::cout << "Received Reflexive Interest " << Rinterest << std::endl;
//     }

// private:
//   Face m_face;
//   KeyChain m_keyChain;
//   ScopedRegisteredPrefixHandle m_certServeHandle;
// };

} // namespace examples
} // namespace ndn

int
main(int argc, char** argv)
{
  try {
    ndn::examples::Producer producer;
    producer.run();
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}

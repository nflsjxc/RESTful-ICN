#include<ndn-cxx/face.hpp>
#include<ndn-cxx/security/validator-config.hpp>
#include<iostream>
#include <ndn-cxx/lp/pit-token.hpp>
#include "assist.hpp"
#include "aux.hpp"

using namespace ndn;

class Consumer
{
public:
    Consumer()
    {
    }
    void run()
    {
        auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
        m_certServeHandle = m_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
                                                 [this, cert] (auto&&...) {
                                                   m_face.put(cert);
                                                 },
                                                 std::bind(&Consumer::onRegisterFailed, this, _1, _2));

        std::cout<<"Consumer: I am ready\n";

        //Round 1
        //Create Hello
        auto name=Name("/service/hello");
        auto helloMessage = generate_source_challenge();
        Interest interest(name);
        // interest.setApplicationParameters(helloMessage.encode());
        // auto lpPitToken = setPitToken(2345);
        // interest.setTag(make_shared<lp::PitToken>(lpPitToken));
        interest.setInterestLifetime(5_s); // 2 seconds
        interest.setMustBeFresh(true);
        std::cout<<interest<<'\n';

        // std::cout<<"pit-token set= "<<readInterestPitToken(interest)<<'\n';
        
        //The filter for RI
        m_face.setInterestFilter(Name("/service"),
                             std::bind(&Consumer::onInterest, this, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             std::bind(&Consumer::onRegisterFailed, this, _1, _2));

        m_face.expressInterest(interest,
                            bind(&Consumer::datacallback, this,  _1, _2),
                            bind(&Consumer::nackcallback, this, _1, _2),
                            bind(&Consumer::timeoutcallback, this, _1)
                            );

        std::cout << ">> I1: " << interest << std::endl;

        m_face.processEvents();
    }

    void full_hello()
    {
        //Round 2 /service/connect
        auto name=Name("/service/connect/1234",true); //Create reflexive name
        auto helloMessage = generate_source_challenge();
        Interest interest(name);
        auto lpPitToken = setPitToken(2345);
        
        
        interest.setTag(make_shared<lp::PitToken>(lpPitToken));
        interest.setInterestLifetime(5_s); 
        interest.setMustBeFresh(true);
        
        auto connect_challenge_message = generate_source_challenge();
        interest.setApplicationParameters(connect_challenge_message.encode());

        m_face.expressInterest(interest,
                            bind(&Consumer::datacallback, this,  _1, _2),
                            bind(&Consumer::nackcallback, this, _1, _2),
                            bind(&Consumer::timeoutcallback, this, _1)
                            );
    }

    void app_data()
    {
        std::cout<<"At app-data round!\n";
    }
    

private:
    KeyChain m_keyChain;
    ScopedRegisteredPrefixHandle m_certServeHandle;
    ndn::Face m_face;
    int round=0;
    Interest m_interest;

    void datacallback(const Interest& interest, const Data& data)
    {
        std::cout << "Received Data " << data << std::endl;
        std::string content = readString(data.getContent());
        auto msg = message();
        msg.decode(content);
        std::cout<<msg.data["type"]<<'\n';
        std::cout<<"------------------\n";

        if(msg.data["type"]=="HELLO-REJECT")
        {
            round=2;
            full_hello();
        }
        else if(msg.data["type"]=="HELLO-ACCEPT")
        {
            round=3;
            app_data();
        }
        else
        {
            round=-1;
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
    void onRegisterFailed(const Name& prefix, const std::string& reason)
    {
        std::cerr << "ERROR: Failed to register prefix '" << prefix
                << "' with the local forwarder (" << reason << ")\n";
        m_face.shutdown();
    }
    void onInterest(const Interest& interest)
    {
        std::cout << ">> Incoming interest: " << interest << std::endl;
        m_interest = interest;
        auto lpPitToken = interest.getTag<lp::PitToken>();
        if(lpPitToken==nullptr)
        {
            std::cout<<"No pit token for interest\n\n";
        }
        std::cout << interest<<"\n\n";
        std::cout << "<< Outcomming Reflexive Data\n";
        //reflexive data prepare pit-token
        uint32_t pitToken = readPitToken(*lpPitToken);
        std::cout<< "pit-token = " << pitToken << std::endl;

     

        auto interest_name = interest.getName();

        for(size_t i = 0; i < interest_name.size(); i++)
        {
            if(interest_name[i]==Name("material")[0])
            {
                reply_material();
            }
        }

    }

    void reply_material()
    {
        //prepare data
        auto data = std::make_shared<Data>();
        data->setName(m_interest.getName());
        data->setFreshnessPeriod(10_s);
        //set data content
        auto msg = generate_hello_reject();
        data->setContent(msg.encode()); 

        m_keyChain.sign(*data);
        std::cout << *data << std::endl;
        m_face.put(*data);
    }
};


int main()
{
    Consumer consumer;
    try
    {
        consumer.run();
    }
    catch(const std::exception& e)
    {
        std::cerr<< "ERROR: "<<e.what()<<'\n';
    }

    return 0;
}
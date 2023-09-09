#include<iostream>
#include<sstream>
#include<string>
#include<cstring>
#include<map>

class message
{
public:
    std::string encode()
    {
        std::stringstream ss;
        for(auto it=data.begin();it!=data.end();++it)
        {
            ss<<it->first<<"="<<it->second<<";";
        }
        return ss.str();
    }

    void decode(const std::string& encodedString)
    {
        std::stringstream ss(encodedString);
        std::string pair;
        while (std::getline(ss, pair, ';')) {
            std::size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                data[key] = value;
            }
        }
    }

public:
    std::map<std::string, std::string> data;
};

message generate_source_challenge()
{
    std::string hello = "type=SourceChallenge;value=Hello;";
    auto ret = message();
    ret.decode(hello);
    return ret;
}

message generate_hello_reject()
{
    std::string hello = "type=HELLO-REJECT;";
    auto ret = message();
    ret.decode(hello);
    return ret;
}

message generate_hello_accept()
{
    std::string hello = "type=HELLO-ACCEPT;";
    auto ret = message();
    ret.decode(hello);
    return ret;
}

// message generate_connect_challenge()
// {
//     std::string connect_challenge = "type=ConnectChallenge;value=ConnectChallenge;";
//     auto ret = message();
//     ret.decode(connect_challenge);
//     return ret;
// }
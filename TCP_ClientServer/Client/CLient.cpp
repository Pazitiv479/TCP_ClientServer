#include <iostream>
#include <net.h>

//���������������� ���� ���������
enum class CustomMsgTypes : uint32_t
{
	ServerAccept,
	ServerDeny,
	ServerPing,
	MessageAll,
	ServerMessage,
};

class CustomClien : public ClientInterface<CustomMsgTypes>
{

};

int main()
{
	CustomClien c;
	c.Connect("127.0.0.1", 60000);
	return 0;
} 
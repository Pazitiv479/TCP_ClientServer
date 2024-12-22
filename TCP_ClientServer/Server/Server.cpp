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

//���������������� ��������� �����, ����������� �� ���������� ����������
class CustomServer : public ServerInterface<CustomMsgTypes>
{
public:
	//��������� ����� ����� � ������ ��������� ���������
	CustomServer(uint16_t nPort) : ServerInterface<CustomMsgTypes>(nPort)
	{

	}

protected:
	virtual bool OnClientConnect(std::shared_ptr<connection<CustomMsgTypes>> client)
	{
		messageBody<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::ServerAccept;
		client->Send(msg);
		return true;
	}

	// ����������, ����� �������, ��� ������ ����������
	virtual void OnClientDisconnect(std::shared_ptr<connection<CustomMsgTypes>> client)
	{
		std::cout << "Removing client [" << client->GetID() << "]\n";
	}

	// ���������� ��� ����������� ���������
	virtual void OnMessage(std::shared_ptr<connection<CustomMsgTypes>> client, messageBody<CustomMsgTypes>& msg)
	{
		switch (msg.header.id)
		{
		case CustomMsgTypes::ServerPing:
		{
			std::cout << "[" << client->GetID() << "]: Server Ping\n";
			client->Send(msg);
		}
		break;

		case CustomMsgTypes::MessageAll:
		{
			std::cout << "[" << client->GetID() << "]: Message All\n";

			messageBody<CustomMsgTypes> msg;
			msg.header.id = CustomMsgTypes::ServerMessage;
			msg << client->GetID();
			MessageAllClients(msg, client);
		}
		break;
		}
	}
};

int main()
{
	CustomServer server(60000);
	server.Start();

	while (1)
	{
		server.Update(-1, true);
	}

	return 0;
}
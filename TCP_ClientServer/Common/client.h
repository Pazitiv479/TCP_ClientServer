#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"
#include "connection.h"

template <typename T>
class ClientInterface
{
public:
	ClientInterface()
	{
	}

	virtual ~ClientInterface()
	{
		// If the client is destroyed, always try and disconnect from server
		Disconnect();
	}

	// ������������ � �������, ������ ��� �����/ip-����� � ����
	bool Connect(const std::string& host, const uint16_t port)
	{
		try
		{
			connection = std::make_unique<Connection<T>>();

			// ������������� ��� �����/ip-����� � �������� ���������� �����
			asio::ip::tcp::resolver resolver(context);
			asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
			//asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

			// ������� ������� �����������, ��� �� ������ ������������ � �������
			connection->ConnectToServer(endpoints);

			//// �������� �����������
			//connection = std::make_unique<connection<T>>(connection<T>::owner::client, context, asio::ip::tcp::socket(context), qMessagesIn);

			// ��������� ����������� �����
			thrContext = std::thread([this]() { context.run(); });
		}
		catch (std::exception& e)
		{
			std::cerr << "Client Exception: " << e.what() << "\n";
			return false;
		}

		return false;
	}

	// ����������� �� �������
	void Disconnect()
	{
		// ���� ���������� ����������, � ����� ��� ����������...
		if (IsConnected())
		{
			// ...��������� ����������� �� �������
			connection->Disconnect();
		}

		// � ����� ������, �� ����� ��������� � ���������� asio...				
		context.stop();
		// ...� ��� ����
		if (thrContext.joinable())
			thrContext.join();

		// ���������� ������ �����������
		connection.release();
	}

	// �������� ����������� � �������
	bool IsConnected()
	{
		if (connection)
			return connection->IsConnected();
		else
			return false;
	}

	// �������� ������� ��������� � �������
	tsqueue<ownedMessage<T>>& Incoming()
	{
		return qMessagesIn;
	}

protected:
	// �������� asio ������������ �������� ������...
	asio::io_context context;
	// ...�� ��������� � ����������� ������ ��� ���������� ����� ������� ������
	std::thread thrContext;
	// � ������� ���� ������������ ��������� ������� "connection", ������� ������������ �������� ������
	std::unique_ptr<connection<T>> connection;

private:
	// ���������������� ������� �������� ��������� � �������
	tsqueue<ownedMessage<T>> qMessagesIn;
};

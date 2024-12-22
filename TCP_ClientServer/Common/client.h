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
			// ������������� ��� �����/ip-����� � �������� ���������� �����
			asio::ip::tcp::resolver resolver(context);
			asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
			
			//// �������� �����������
			m_connection = std::make_unique<connection<T>>(
				connection<T>::owner::client, 
				context, asio::ip::tcp::socket(context), 
				qMessagesIn);

			// ������� ������� �����������, ��� �� ������ ������������ � �������
			m_connection->ConnectionToServer(endpoints);

			// ��������� ����������� �����
			thrContext = std::thread([this]() { context.run(); });
		}
		catch (const std::exception& e)
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
			m_connection->Disconnect();
		}

		// � ����� ������, �� ����� ��������� � ���������� asio...				
		context.stop();
		// ...� ��� ����
		if (thrContext.joinable())
			thrContext.join();

		// ���������� ������ �����������
		m_connection.release();
	}

	// �������� ����������� � �������
	bool IsConnected()
	{
		if (m_connection)
			return m_connection->IsConnected();
		else
			return false;
	}

	// �������� ������� ��������� � �������
	tsqueue<ownedMessage<T>>& Incoming()
	{
		return qMessagesIn;
	}

	// ��������� ��������� �� ������
	void Send(const messageBody<T>& msg)
	{
		if (IsConnected())
			m_connection->Send(msg);
	}

protected:
	// �������� asio ������������ �������� ������...
	asio::io_context context;
	// ...�� ��������� � ����������� ������ ��� ���������� ����� ������� ������
	std::thread thrContext;
	// � ������� ���� ������������ ��������� ������� "connection", ������� ������������ �������� ������
	std::unique_ptr<�onnection<T>> m_connection;

private:
	// ���������������� ������� �������� ��������� � �������
	tsqueue<ownedMessage<T>> qMessagesIn;
};

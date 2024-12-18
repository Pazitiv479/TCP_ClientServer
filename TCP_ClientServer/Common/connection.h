#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"

template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>
{
public:
	// ���������� "�����������" ���� �������, ���� �������, � ���
	// ��������� � ��� ������� ����������.
	enum class owner
	{
		server,
		client
	};

	// �����������: ������� ���������, ������������ � ���������, ��������� �����
	//				������� ������ �� ������� �������� ���������
	connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<ownedMessage<T>>& qIn)
		: asioContext(asioContext), socket(std::move(socket)), qMessagesIn(qIn)
	{
		nOwnerType = parent;
	}

	virtual ~connection()
	{
	}

	// ���� ������������� ������������ �� ���� ������� - ��� ������� ����� �������� ������ ��������
			// ���������� �� ���� �������.
	uint32_t GetID() const
	{
		return id;
	}

	void ConnectToClient(uint32_t uid = 0)
	{
		if (nOwnerType == owner::server)
		{
			if (socket.is_open())
			{
				id = uid;
				ReadHeader();
			}
		}
	}

	void ConnectionToServer(const asio::ip::tcp::resolver::results_type& endpoints)
	{
		//������ ������� ����� ������������ � ��������
		if (nOwnerType == owner::client)
		{
			// ��������� ������� asio ������������ � �������� �����
			asio::async_connect(socket, endpoints,
				[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
				{
					if (!ec)
					{
						ReadHeader();
					}
				});
		}
	}
	
	void Disconnect()
	{
		if (IsConnected())
			asio::post(asioContext, [this]() { socket.close(); });
	}

	bool IsConnected() const 
	{
		return socket.is_open();
	}

	// ASYNC - �������� ���������, ���������� ����������� ���� � ������, ������� ��������� ���� �� �����,
	// ��� ������� ����� �������� ������, � ��������
	void Send(const messageBody<T>& msg)
	{
		asio::post(asioContext,
			[this, msg]()
			{
				// ���� � ������� ���� ���������, �� �� ������ 
				// ������������, ��� ��� ��������� � �������� ����������� ������.
				// � ����� ������ �������� ��������� � ������� ��� ������. ���� ��� ��������� 
				// ��������� ��� ������, ��������� ������� ������
				// ��������� � ������ �������.
				bool bWritingMessage = !qMessagesOut.empty();
				qMessagesOut.push_back(msg);
				if (!bWritingMessage)
				{
					WriteHeader();
				}
			});
	}

private:
	// ASYNC - �������� ��������, ������� � ������ ��������� ���������
	void ReadHeader()
	{
		// ���� ��� ������� �������, �� �������, ��� asio ����� �����, ���� �� �������
		// ����������� ���������� ���� ��� ������������ ��������� ���������. �� �����, ��� ��������� ����� ������������� ������,
		// ������� �������� ����� ��������, ���������� ������� ��� ��� ��������. ����������,
		// �� �������� ��������� �� "���������" ������� messageBdy, ��������� � ��� 
		// ������ ��������.
		asio::async_read(socket, asio::buffer(&msgTemporaryIn.header, sizeof(messageHeader<T>)),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// ��������� ��������� �������� ���������, ���������, ���� �� � ����� ��������� �����, �� ������� ����� ���������.
					if (msgTemporaryIn.header.size > 0)
					{
						// ...��� ���, ������� �������� ���������� ����� � ���� ���������
						// vector � ��������� asio ������� �� ������ ������.
						msgTemporaryIn.body.resize(msgTemporaryIn.header.size);
						ReadBody();
					}
					else
					{
						// ��� �� ���, ������� �������� ��� ��������� ��� ������ �
						// ������� �������� ���������
						AddToIncomingMessageQueue();
					}
				}
				else
				{
					// ��� ���������� ������ � ������� ��������� ������, ������ �����, ��������� ����������
					// �������� ����� � ��������� ������� �������� ��� � ������� �����.
					std::cout << "[" << id << "] Read Header Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - Prime context ready to read a message body
	void ReadBody()
	{
		// ���� ��� ������� �������, ��������� ��� ��������, � ���� ���������
		// �����������, ����� �� ��������� �����, ������������ ��� ����� ������ ��� ��������
		// �� ��������� ������� ���������, ������� ������ ��������� ����������� ������...
		asio::async_read(socket, asio::buffer(msgTemporaryIn.body.data(), msgTemporaryIn.body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// ...� ��� ��� �������! ������ ��������� ������, ������� ��������
					// ��� ��������� ������� �� �������� �������
					AddToIncomingMessageQueue();
				}
				else
				{
					// ��� ������� ����!
					std::cout << "[" << id << "] Read Body Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - �������� �������� ��� ��������� ��������� ���������
	void WriteHeader()
	{
		// ���� ��� ������� �������, �� �����, ��� � ������� ��������� ��������� ������ ���� 
		// �� ������� ���� ���� ��������� ��� ��������. ������� �������� ����� �������� ��� ��������
		// ��������� � ��������� ������� work - asio, �������� ��� �����
		asio::async_write(socket, asio::buffer(&qMessagesOut.front().header, sizeof(messageHeader<T>)),
			[this](std::error_code ec, std::size_t length)
			{
				// asio ��� �������� ����� - ���� �� �������� ��������
				// ���� �� �������� ��������� �� ������...
				if (!ec)
				{
					// ... ������ ���, ������� ���������, ���� �� � ��������� ������ ��� ������������� ��������� ����� ���������.
					if (qMessagesOut.front().body.size() > 0)
					{
						// ...��� ���, ������� ������� ������ ��� ������ ������ ����
						WriteBody();
					}
					else
					{
						// ...����� �� ���������, ������� �� ��������� � ���� ����������. ������� ��� �� 
						// ������� ��������� ���������
						qMessagesOut.pop_front();

						// ���� ������� �� �����, ����� ��������� ��� ��������� ���������, �������
						// �������� ���, ����� ��� ���������, ����� ������� �� �������� ���������� ���������.
						if (!qMessagesOut.empty())
						{
							WriteHeader();
						}
					}
				}
				else
				{
					// ...asio �� ������� ��������� ���������, �� ����� �� ���������������� �������, �� 
					// ���� ������ �����������, ��� ���������� ���������� ��-�� �������� ������. 
					// ����� � ������� ������� ������ �� ���� ������ ���������� �������� ��-��
					// ��������� ������, ��� ����� ����������.
					std::cout << "[" << id << "] Write Header Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - �������� �������� ��� ��������� ������ ���������
	void WriteBody()
	{
		// ���� ��� ������� �������, ������ ��� ��� ��������� ���������, � ���� ���������
		// ��������� �� ��, ��� ��� ����� ��������� ���������� �������� �����. ��������� ����� ��������
		// ������� �������� ����� � ��������� ���!
		asio::async_write(socket, asio::buffer(qMessagesOut.front().body.data(), qMessagesOut.front().body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// �������� ������ �������, ������� �� ��������� ������ � ����������
					// � ������� ��� �� �������
					qMessagesOut.pop_front();

					// ���� � ������� ��� ��� ���� ���������, �� ��������� ������� �� 
					// �������� ��������� ��������� ���������.
					if (!qMessagesOut.empty())
					{
						WriteHeader();
					}
				}
				else
				{
					// ������ ��������, �������� �������� � ����������� ������� WriteHeader()
					std::cout << "[" << id << "] Write Body Fail.\n";
					socket.close();
				}
			});
	}

	// ��� ������ ����� �������� ������ ���������, �������� ��� �� �������� �������
	void AddToIncomingMessageQueue()
	{
		// �������� ��� � �������, ���������� � "����������� ���������", �������������
		// � ������� ������ ��������� �� ����� ������� connection
		if (nOwnerType == owner::server)
			qMessagesIn.push_back({ this->shared_from_this(), msgTemporaryIn });
		else
			qMessagesIn.push_back({ nullptr, msgTemporaryIn });

		// ������ �� ������ ��������� �������� asio ��� ��������� ���������� ���������. ��� 
		// ����� �����, ���� �������� �����, � ����� ����� ������������ ���������
		// ������� �����������.
		ReadHeader();
	}

protected:
	// ������ ���������� ����� ���������� ����� ��� ����������� � ���������� ����������. 
	asio::ip::tcp::socket socket;

	// ���� �������� �������� ����� ��� ����� ���������� asio
	asio::io_context& asioContext;

	// � ���� ������� �������� ��� ���������, ������� ������ ���� ���������� ��������� �������
	// ����� ����������
	tsqueue<messageBody<T>> qMessagesOut;

	// C�������� �� �������� ������� ������������� �������
	tsqueue<ownedMessage<T>>& qMessagesIn;

	// �������� ��������� ��������� ����������, ������� �� �����
	// ��������� �������� ��������� ��������� �����, ���� ��� �� ����� ������
	messageBody<T> msgTemporaryIn;

	// "��������" ������, ��� ����� ����� ���� ��������� ����������
	owner nOwnerType = owner::server;
	uint32_t id = 0;
};

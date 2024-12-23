#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"
#include "connection.h"

namespace net
{
	template<typename T>
	class ServerInterface
	{
	public:

		// ������ ������, ������� � ������������� �� ���������� �����
		ServerInterface(uint16_t port)
			: asioAcceptor(asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{

		}

		virtual ~ServerInterface()
		{
			Stop();
		}

		//������ �������
		bool Start()
		{
			try
			{
				// ��������� ������ ��������� asio - ��� �����
				//, ��� ��� � ���� ������ �������� ����� �������� ������ "work" �
				// �� �� ������ ���������� �����������. ��������� ��� ������,  
				// �� ������ ���� ����� � ������ � ���������, �����������
				// ������������.
				WaitForClientConnection();

				// ��������� �������� asio � ����� ����������� ������
				threadContext = std::thread([this]() { asioContext.run(); });
			}
			catch (std::exception& e)
			{
				// ���-�� �������� ������� ������������
				std::cerr << "[SERVER] Exception: " << e.what() << "\n";
				return false;
			}

			std::cout << "[SERVER] Started!\n";
			return true;
		}

		//��������� �������
		void Stop()
		{
			// ��������� �������� ���������
			asioContext.stop();

			// ��������� � ������� ����������� �����
			if (threadContext.joinable())
				threadContext.join();

			std::cout << "[SERVER] Stopped!\n";
		}

		// ASYNC - ���� ������� asio ������� �����������. ������������� ��� ��������� ASIO
		// ASYNC ����� ��� ���������� �����, �����������, ASIO �� �����, ��������������, ����������
		void WaitForClientConnection()
		{
			asioAcceptor.async_accept(
				[this](std::error_code ec, asio::ip::tcp::socket socket)
				{
					// ����������� �� ��������� ������� �� �����������
					if (!ec)
					{
						// ���������� ��������� ��������(?) ����������
						std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

						// �������� ����� ���������� ��� ������ � ���� ��������
						std::shared_ptr<connection<T>> newconn =
							std::make_shared<connection<T>>(connection<T>::owner::server,
								asioContext, std::move(socket), qMessagesIn);

						// ����� ������� ������������ ����������� ��������� �����������
						if (OnClientConnect(newconn))
						{
							// ����������� ���������, ������� ���������� � ��������� ����� �����������
							deqConnections.push_back(std::move(newconn));

							// � ��� ����� �����! ������� ������ ��������� ����������
							// asio ������ � ����� ����������� ������!
							deqConnections.back()->ConnectToClient(nIDCounter++);

							std::cout << "[" << deqConnections.back()->GetID() << "] Connection Approved\n";
						}
						else
						{
							std::cout << "[-----] Connection Denied\n";

							// ���������� ������ �� ������� ������� ��������� ��� �����-���� ��������� ���������� �����, �������
							// ����� ������������� ������� ��-�� �������� ���������������� ����������
						}
					}
					else
					{
						// �� ����� ������ ��������� ������
						std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
					}

					// ��������� �������� asio �������������� ������� - ����� ������ ���������
					// ������� �����������...
					WaitForClientConnection();
				});
		}

		// ��������� ��������� ����������� �������
		void MessageClient(std::shared_ptr<connection<T>> client, const messageBody<T>& msg)
		{
			// ���������, �������� �� ������ ��������...
			if (client && client->IsConnected())
			{
				// ...� ��������� ��������� ����� ����������
				client->Send(msg);
			}
			else
			{
				// ���� �� �� ������ ��������� � ��������, �� �� ����� ������� ��������� 
				// ������ ������� - ������� �� ���� �������, �� �����
				// �����-�� ������� ��������� ���
				OnClientDisconnect(client);

				client.reset();

				// �������� �� ������  ������������ ��������
				deqConnections.erase(
					std::remove(deqConnections.begin(), deqConnections.end(), client), deqConnections.end());
			}
		}

		// ��������� ��������� ���� ��������
		void MessageAllClients(const messageBody<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
		{
			bool bInvalidClientExists = false;

			// ��������� �������� �� ���� �������� � ����������
			for (auto& client : deqConnections)
			{
				// ���������, ��������� �� ������
				if (client && client->IsConnected())
				{
					if (client != pIgnoreClient)
						client->Send(msg);
				}
				else
				{
					// ��������� � �������� �� �������, ������� �����������, ��� ��
					// ��������.
					OnClientDisconnect(client);
					client.reset();

					// ���������� ���� ����, ����� ����� ������� ������� �������� �� ����������
					bInvalidClientExists = true;
				}
			}

			// ������� ������� �������� �� ���� ��� - ����� �������, �� �� ������ ���������������� ���������
			// ��� ��������� ���������.
			if (bInvalidClientExists)
				deqConnections.erase(
					std::remove(deqConnections.begin(), deqConnections.end(), nullptr), deqConnections.end());
		}

		// ��������� ������ �������� �� �������� ���������
		void Update(size_t nMaxMessages = -1, bool bWait = false)
		{
			if (bWait) qMessagesIn.wait();

			// ����������� ������� ���������, ������� �������, ������ �� ���������� ��������
			size_t nMessageCount = 0;
			while (nMessageCount < nMaxMessages && !qMessagesIn.empty())
			{
				// ��������� �������� ���������
				auto msg = qMessagesIn.pop_front();

				// �������� ����������� ���������
				OnMessage(msg.remote, msg.msg);

				nMessageCount++;
			}
		}

	protected:
		// ���� ��������� ����� ������ �������������� ��� ������� ��� ����������
		// ������������� ����������������

		// ���������� ��� ����������� �������, �� ������ �������� ���� �� �����������, ������ �������� false
		virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
		{
			return false;
		}

		// ���������� ����� ������ ����������
		virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
		{

		}

		// ���������� ��� ����������� ��������� �� ����������� �������
		virtual void OnMessage(std::shared_ptr<connection<T>> client, messageBody<T>& msg)
		{

		}

		// ���������������� ������� ��� �������� ������� ���������
		tsqueue<ownedMessage<T>> qMessagesIn;

		// ��������� �������� ����������� ����������
		std::deque<std::shared_ptr<connection<T>>> deqConnections;

		// ������� ���������� ����� - ��� ����� ������� �������������
		asio::io_context asioContext;
		std::thread threadContext;

		// ��� ����� ����� ����� �������� asio
		asio::ip::tcp::acceptor asioAcceptor; // ������������ ����� ������� ��������� �����������...

		// ������� ����� ���������������� � "����� ������� �������" � ������� ��������������
		uint32_t nIDCounter = 10000;
	};
}
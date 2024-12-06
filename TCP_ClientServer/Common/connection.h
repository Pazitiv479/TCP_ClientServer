#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"

template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>
{
public:
	connectiob()
	{
	}

	virtual ~connection()
	{
	}

	bool ConnectionToServer();
	bool Disconnectrd();
	bool IsConnected() const;

	bool Send(const messageBody<T>& msg);

protected:
	// ������ ���������� ����� ���������� ����� ��� ����������� � ���������� ����������. 
	asio::ip::tcp::socket socket;

	// ���� �������� �������� ����� ��� ����� ���������� asio
	asio::io_context& asioContext;

	// � ���� ������� �������� ��� ���������, ������� ������ ���� ���������� ��������� �������
	// ����� ����������
	tsqueue<message<T>> qMessagesOut;

	// C�������� �� �������� ������� ������������� �������
	tsqueue<owned_message<T>>& qMessagesIn;
};

#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"

template <typename T>
class Connection : public std::enable_shared_from_this<connection<T>>
{
public:
	Connection()
	{
	}

	virtual ~Connection()
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
	tsqueue<messageBody<T>> qMessagesOut;

	// C�������� �� �������� ������� ������������� �������
	tsqueue<ownedMessage<T>>& qMessagesIn;
};

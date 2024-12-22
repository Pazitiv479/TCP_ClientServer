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

	// Подключиться к серверу, указав имя хоста/ip-адрес и порт
	bool Connect(const std::string& host, const uint16_t port)
	{
		try
		{
			// Преобразовать имя хоста/ip-адрес в реальный физический адрес
			asio::ip::tcp::resolver resolver(context);
			asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
			
			//// Создание подключения
			m_connection = std::make_unique<connection<T>>(
				connection<T>::owner::client, 
				context, asio::ip::tcp::socket(context), 
				qMessagesIn);

			// Укажите объекту подключения, что он должен подключиться к серверу
			m_connection->ConnectionToServer(endpoints);

			// Запустить контекстный поток
			thrContext = std::thread([this]() { context.run(); });
		}
		catch (const std::exception& e)
		{
			std::cerr << "Client Exception: " << e.what() << "\n";
			return false;
		}
		
		return false;
	}

	// Отключиться от сервера
	void Disconnect()
	{
		// Если соединение существует, и тогда оно подключено...
		if (IsConnected())
		{
			// ...корректно отключиться от сервера
			m_connection->Disconnect();
		}

		// В любом случае, мы также закончили с контекстом asio...				
		context.stop();
		// ...и его нить
		if (thrContext.joinable())
			thrContext.join();

		// Уничтожить объект подключения
		m_connection.release();
	}

	// Проверка подключения к серверу
	bool IsConnected()
	{
		if (m_connection)
			return m_connection->IsConnected();
		else
			return false;
	}

	// Получить очередь сообщений с сервера
	tsqueue<ownedMessage<T>>& Incoming()
	{
		return qMessagesIn;
	}

	// Отправить сообщение на сервер
	void Send(const messageBody<T>& msg)
	{
		if (IsConnected())
			m_connection->Send(msg);
	}

protected:
	// контекст asio обрабатывает передачу данных...
	asio::io_context context;
	// ...но нуждается в собственном потоке для выполнения своих рабочих команд
	std::thread thrContext;
	// У клиента есть единственный экземпляр объекта "connection", который обрабатывает передачу данных
	std::unique_ptr<сonnection<T>> m_connection;

private:
	// Потокобезопасная очередь входящих сообщений с сервера
	tsqueue<ownedMessage<T>> qMessagesIn;
};

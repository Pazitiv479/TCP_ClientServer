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

		// Создаёт сервер, готовый к прослушиванию по указанному порту
		ServerInterface(uint16_t port)
			: asioAcceptor(asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{

		}

		virtual ~ServerInterface()
		{
			Stop();
		}

		//Запуск сервера
		bool Start()
		{
			try
			{
				// Передайте задачу контексту asio - это важно
				//, так как в этом случае контекст будет заполнен словом "work" и
				// он не сможет немедленно завершиться. Поскольку это сервер,  
				// он должен быть готов к работе с клиентами, пытающимися
				// подключиться.
				WaitForClientConnection();

				// Запускает контекст asio в своем собственном потоке
				threadContext = std::thread([this]() { asioContext.run(); });
			}
			catch (std::exception& e)
			{
				// Что-то помешало серверу прослушивать
				std::cerr << "[SERVER] Exception: " << e.what() << "\n";
				return false;
			}

			std::cout << "[SERVER] Started!\n";
			return true;
		}

		//Остановка сервера
		void Stop()
		{
			// Запросить закрытие контекста
			asioContext.stop();

			// Приведите в порядок контекстный поток
			if (threadContext.joinable())
				threadContext.join();

			std::cout << "[SERVER] Stopped!\n";
		}

		// ASYNC - дает команду asio ожидать подключения. Предназначена для контекста ASIO
		// ASYNC нужна для разделения задач, выполняемых, ASIO от задач, предназначеных, фреймворку
		void WaitForClientConnection()
		{
			asioAcceptor.async_accept(
				[this](std::error_code ec, asio::ip::tcp::socket socket)
				{
					// Запускается по входящему запросу на подключение
					if (!ec)
					{
						// Отображать некоторую полезную(?) информацию
						std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

						// Создайте новое соединение для работы с этим клиентом
						std::shared_ptr<connection<T>> newconn =
							std::make_shared<connection<T>>(connection<T>::owner::server,
								asioContext, std::move(socket), qMessagesIn);

						// Дайте серверу пользователя возможность запретить подключение
						if (OnClientConnect(newconn))
						{
							// Подключение разрешено, поэтому добавляйте в контейнер новые подключения
							deqConnections.push_back(std::move(newconn));

							// И это очень важно! Задайте задачу контексту соединения
							// asio сидеть и ждать поступления байтов!
							deqConnections.back()->ConnectToClient(nIDCounter++);

							std::cout << "[" << deqConnections.back()->GetID() << "] Connection Approved\n";
						}
						else
						{
							std::cout << "[-----] Connection Denied\n";

							// Соединение выйдет за пределы области видимости без каких-либо ожидающих выполнения задач, поэтому
							// будет автоматически удалено из-за чудесных интеллектуальных указателей
						}
					}
					else
					{
						// Во время приема произошла ошибка
						std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
					}

					// Загрузите контекст asio дополнительной работой - снова просто дождитесь
					// другого подключения...
					WaitForClientConnection();
				});
		}

		// Отправить сообщение конкретному клиенту
		void MessageClient(std::shared_ptr<connection<T>> client, const messageBody<T>& msg)
		{
			// Проверьте, является ли клиент законным...
			if (client && client->IsConnected())
			{
				// ...и отправить сообщение через соединение
				client->Send(msg);
			}
			else
			{
				// Если мы не сможем связаться с клиентом, то мы можем сделать следующее 
				// удалим клиента - сообщим об этом серверу, он может
				// каким-то образом отследить это
				OnClientDisconnect(client);

				client.reset();

				// Удаление из списка  подключенных клиентов
				deqConnections.erase(
					std::remove(deqConnections.begin(), deqConnections.end(), client), deqConnections.end());
			}
		}

		// Отправить сообщение всем клиентам
		void MessageAllClients(const messageBody<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
		{
			bool bInvalidClientExists = false;

			// Выполнить итерацию по всем клиентам в контейнере
			for (auto& client : deqConnections)
			{
				// Проверьте, подключен ли клиент
				if (client && client->IsConnected())
				{
					if (client != pIgnoreClient)
						client->Send(msg);
				}
				else
				{
					// Связаться с клиентом не удалось, поэтому предположим, что он
					// отключен.
					OnClientDisconnect(client);
					client.reset();

					// Установите этот флаг, чтобы затем удалить мертвых клиентов из контейнера
					bInvalidClientExists = true;
				}
			}

			// Удаляем мертвых клиентов за один раз - таким образом, мы не делаем недействительным контейнер
			// при повторном просмотре.
			if (bInvalidClientExists)
				deqConnections.erase(
					std::remove(deqConnections.begin(), deqConnections.end(), nullptr), deqConnections.end());
		}

		// Заставить сервер отвечать на входящие сообщения
		void Update(size_t nMaxMessages = -1, bool bWait = false)
		{
			if (bWait) qMessagesIn.wait();

			// Обработайте столько сообщений, сколько сможете, вплоть до указанного значения
			size_t nMessageCount = 0;
			while (nMessageCount < nMaxMessages && !qMessagesIn.empty())
			{
				// Захватите переднее сообщение
				auto msg = qMessagesIn.pop_front();

				// Передать обработчику сообщения
				OnMessage(msg.remote, msg.msg);

				nMessageCount++;
			}
		}

	protected:
		// Этот серверный класс должен переопределить эти функции для реализации
		// настраиваемой функциональности

		// Вызывается при подключении клиента, вы можете наложить вето на подключение, вернув значение false
		virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
		{
			return false;
		}

		// Вызывается когда клиент отключился
		virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
		{

		}

		// Вызывается при поступлении сообщения от конкретного клиента
		virtual void OnMessage(std::shared_ptr<connection<T>> client, messageBody<T>& msg)
		{

		}

		// Потокобезопасная очередь для входящих пакетов сообщений
		tsqueue<ownedMessage<T>> qMessagesIn;

		// Контейнер активных проверенных соединений
		std::deque<std::shared_ptr<connection<T>>> deqConnections;

		// Порядок объявления важен - это также порядок инициализации
		asio::io_context asioContext;
		std::thread threadContext;

		// Для таких вещей нужен контекст asio
		asio::ip::tcp::acceptor asioAcceptor; // Обрабатывает новые попытки входящего подключения...

		// Клиенты будут идентифицированы в "более широкой системе" с помощью идентификатора
		uint32_t nIDCounter = 10000;
	};
}
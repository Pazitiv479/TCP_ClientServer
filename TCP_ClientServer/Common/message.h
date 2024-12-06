#pragma once

#include "common.h"


//—труктура заголовка сообщени€
template <typename T>
struct messageHeader
{
	T id{};
	uint32_t size = 0;
};

//—труктура тела сообщени€
template <typename T>
struct messageBody
{
	messageHeader<T> header;
	std::vector<uint8_t> body; //uint8_t - дл€ работы с байтами, std::vector хранит необработанные байты данных

	// ¬озвращает размер всего пакета сообщени€ в байтах
	size_t size() const
	{
		return sizeof(messageHeader<T>) + body.size();
	}

	// ѕереопределение дл€ совместимости с std::cout - выводит удобочитаемое описание сообщени€
	friend std::ostream& operator << (std::ostream& os, const messageBody<T>& msg)
	{
		os << "ID: " << int(msg.header.id) << " Size: " << msg.header.size;
	}

	// ƒобавл€ет любые простые старые (привычные) типы данных данные в буфер сообщени€.
	// POD (Plain Old Data) - ѕростой старый (или привычный) тип данных
	template<typename DataType>
	friend messageBody<T>& operator << (messageBody<T>& msg, const DataType& data)
	{
		// ѕровер€ет, что тип добавл€емых данных тривиально копируемый.
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

		// —охран€ет текущий размер вектора, так как это будет точка, в которую мы вставим данные.
		size_t i = msg.body.size();

		// »змен€ет размер вектора на размер добавл€емых данных.
		msg.body.resize(msg.body.size() + sizeof(DataType));

		// ‘изически скопирует данные в недавно выделенное пространство вектора.
		// + i место с которого добавл€ютс€ данные
		std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

		// ќбновление переменной, объ€вленной в зщаголовке сообщени€
		msg.header.size = msg.size();

		return msg;
	}

	// »звлекает любые простые старые (привычные) типы данных данные в буфер сообщени€.
	// POD (Plain Old Data) - ѕростой старый (или привычный) тип данных
	template<typename DataType>
	friend messageBody<T>& operator >> (messageBody<T>& msg, DataType& data)
	{
		// ѕроверка, что тип извлекаемых данных тривиально копируемый
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

		// ¬ычитание размера извлекаемых данных с конца вектора
		size_t i = msg.body.size() - sizeof(DataType);

		// ‘изическое копирвание данных из вектора в переменную пользовател€
		std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

		// ”меньшение размера вектора, чтобы удалить прочитанные байты, и сброс конечной позиции
		msg.body.resize(i);

		// ќбновление переменной, объ€вленной в зщаголовке сообщени€
		msg.header.size = msg.size();

		return msg;
	}
};

// ѕредварительн ое объ€вление класса соединени€
template <typename T>
class connection;


// —труктура, котра€ покажет серверу от какого клиента пришло сообщение
template <typename T> 
struct ownedMessage
{
	std::shared_ptr<connection<T>> remote = nullptr;
	messageBody<T> msg;

	// ‘ункци€ дл€ вывода в поток при  помощи cout
	friend std::ostream& operator << (std::ostream& os, const ownedMessage<T>& msg)
	{
		os << msg.msg;
		return os;
	}
};



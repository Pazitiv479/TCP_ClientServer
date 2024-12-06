#include <iostream>
#include <net.h>

enum class CustomMsgTypes : uint32_t
{
	FireBullet,
	MovePlayer
};

int main()
{
	messageBody<CustomMsgTypes> msg;
	msg.header.id = CustomMsgTypes::FireBullet;
}
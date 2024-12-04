#include <iostream>
#include <net.h>

enum class CustomMsgTypes : uint32_t
{
	FireBullet,
	MovePlayer
};

int main()
{
	message<CustomMsgTypes> msg;
}
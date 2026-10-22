#ifndef HOT_DISCORD_HOST_H
#define HOT_DISCORD_HOST_H

// Finding the Discord SDK library and the Discord client on this OS.
namespace HoT::Discord::Host {

// The DiscordCreate export of the SDK library, nullptr while it cannot be loaded.
void* LoadCreateFunction();

// Core::Create with default flags relaunches the game through Discord when it is not running, so look for it first.
bool IsDiscordRunning();

}

#endif

#ifndef SERVER_HPP
#define SERVER_HPP
#include "network/Network.hpp"
#include "Player.hpp"
#include <memory>

using std::unique_ptr;

namespace Diggler {

class Game;

class Server {
private:
	Game &G;

	void handleCommand(Player *, const std::string &command, const std::vector<std::string> &args);

	void handlePlayerJoin(Net::InMessage&, Net::Peer&);
	void handlePlayerQuit(Net::Peer&, Net::QuitReason reason = Net::QuitReason::Quit);
	void handleDisconnect(Net::Peer&);

	void handleEvent(Net::InMessage&, Net::Peer&);
	void handleChat(Net::InMessage&, Net::Peer&);
	void handlePlayerUpdate(Net::InMessage&, Net::Peer&);
		void handlePlayerDeath(Net::InMessage&, Player&);
	void handlePlayerMapUpdate(Net::InMessage&, Net::Peer&);

public:
	Net::Host H;

	Server(Game &G, uint16 port);
	~Server();
	void setupInternals();
	void setup();

	void run();
	bool isPlayerOnline(const std::string &playername) const;
	bool isIPOnline(const std::string &ip) const;
	Player* getPlayerById(uint32 id);
	Player* getPlayerByPeer(const Net::Peer &peer);
	Player* getPlayerByName(const std::string &name);
	void kick(Player &p, Net::QuitReason r = Net::QuitReason::Kicked, const std::string& message = "");
};

}

#endif
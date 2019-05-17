/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http:
*/
/** @file Peer.h
 * @author Alex Leverington <nessence@gmail.com>
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include "Common.h"

namespace dev
{

namespace p2p
{

/**
 * @brief Representation of connectivity state and all other pertinent Peer metadata.
 * A Peer represents connectivity between two nodes, which in this case, are the host
 * and remote nodes.
 *
 * State information necessary for loading network topology is maintained by NodeTable.
 *
 * @todo Implement 'bool required'
 * @todo reputation: Move score, rating to capability-specific map (&& remove friend class)
 * @todo reputation: implement via origin-tagged events
 * @todo Populate metadata upon construction; save when destroyed.
 * @todo Metadata for peers needs to be handled via a storage backend. 
 * Specifically, peers can be utilized in a variety of
 * many-to-many relationships while also needing to modify shared instances of
 * those peers. Modifying these properties via a storage backend alleviates
 * Host of the responsibility. (&& remove save/restoreNetwork)
 * @todo reimplement recording of historical session information on per-transport basis
 * @todo move attributes into protected
 */
class Peer: public Node
{
	friend class Session;		
	friend class Host;		

	friend class RLPXHandshake;

public:
	
	Peer(Node const& _node): Node(_node) {}
	
	bool isOffline() const { return !m_session.lock(); }

	virtual bool operator<(Peer const& _p) const;
	
	
	int rating() const { return m_rating; }
	
	
	bool shouldReconnect() const;
	
	
	int failedAttempts() const { return m_failedAttempts; }

	
	DisconnectReason lastDisconnect() const { return m_lastDisconnect; }
	
	
	void noteSessionGood() { m_failedAttempts = 0; }
	
protected:
	
	unsigned fallbackSeconds() const;

	int m_score = 0;									
	int m_rating = 0;									
	
	
	
	std::chrono::system_clock::time_point m_lastConnected;
	std::chrono::system_clock::time_point m_lastAttempted;
	unsigned m_failedAttempts = 0;
	DisconnectReason m_lastDisconnect = NoDisconnect;	

	
	std::weak_ptr<Session> m_session;
};
using Peers = std::vector<Peer>;

}
}

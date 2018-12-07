// (c) 2018 Pttn (https://github.com/Pttn/rieMiner)

#ifndef HEADER_GBTClient_hpp
#define HEADER_GBTClient_hpp

#include "Client.hpp"
#include "tools.hpp"

struct GetBlockTemplateData {
	BlockHeader bh;
	std::string transactions, default_witness_commitment; // In hex format
	std::vector<std::array<uint8_t, 32>> txHashes;
	uint64_t coinbasevalue;
	uint32_t height, primes;
	std::vector<uint8_t> coinbase, // Store Coinbase Transaction here
	                     scriptPubKey; // Calculated from custom payout address for Coinbase Transaction
	std::vector<std::string> rules; // From GetBlockTemplate response
	
	GetBlockTemplateData() {
		bh = BlockHeader();
		transactions = std::string();
		txHashes = std::vector<std::array<uint8_t, 32>>();
		default_witness_commitment = std::string();
		coinbasevalue = 0;
		height = 0;
		primes = 6;
		coinbase = std::vector<uint8_t>();
		scriptPubKey = std::vector<uint8_t>(20, 0);
		rules = std::vector<std::string>();
	}
	
	void coinBaseGen(const std::string& = "");
	std::array<uint8_t, 32> coinBaseHash() const {
		return v8ToA8(sha256sha256(coinbase.data(), coinbase.size()));
	}
	void merkleRootGen() {
		const std::array<uint8_t, 32> mr(calculateMerkleRoot(txHashes));
		memcpy(bh.merkleRoot, mr.data(), 32);
	}
	bool isActive(const std::string &rule) const {
		for (uint32_t i(0) ; i < rules.size() ; i++) {
			if (rules[i] == rule) return true;
		}
		return false;
	}
};

class GBTClient : public RPCClient {
	GetBlockTemplateData _gbtd;
	
	public:
	using RPCClient::RPCClient;
	bool connect();
	bool getWork();
	void sendWork(const WorkData&) const;
	WorkData workData() const;
};

#endif

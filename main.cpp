// (c) 2017-2018 Pttn and contributors (https://github.com/Pttn/rieMiner)

#include "main.hpp"
#include "Client.hpp"
#include "Miner.hpp"
#include "WorkManager.hpp"
#include "tools.hpp"
#include <iomanip>
#include <unistd.h>
#ifndef _WIN32
	#include <signal.h>
	#include <arpa/inet.h>
	#include <netdb.h>
#else
	#include <winsock2.h>
#endif

int DEBUG(0);

std::shared_ptr<WorkManager> manager;
static std::string confPath("rieMiner.conf");

void Options::parseLine(std::string line, std::string& key, std::string& value) const {
	for (uint16_t i(0) ; i < line.size() ; i++) { // Delete spaces
		if (line[i] == ' ' || line[i] == '\t') {
			line.erase (i, 1);
			i--;
		}
	}
	
	const auto pos(line.find('='));
	if (pos != std::string::npos) {
		key   = line.substr(0, pos);
		value = line.substr(pos + 1, line.size() - pos - 1);
	}
	else {
		std::cerr << "Ill formed configuration line: " << line << std::endl;
		key   = "Error";
		value = "Ill formed configuration line";
	}
}

void Options::askConf() {
	std::string value;
	std::ofstream file(confPath);
	if (file) {
		std::cout << "Solo mining (solo), pooled mining (pool), or benchmarking (benchmark)? ";
		std::cin >> value;
		if (value == "solo") {
			file << "Protocol = GetBlockTemplate" << std::endl;
			_protocol = "GetBlockTemplate";
		}
		else if (value == "pool") {
			file << "Protocol = Stratum" << std::endl;
			_protocol = "Stratum";
		}
		else if (value == "benchmark") {
			file << "Protocol = Benchmark" << std::endl;
			_protocol = "Benchmark";
		}
		else {
			std::cerr << "Invalid choice! Please answer solo, pool, or benchmark." << std::endl;
			std::remove(confPath.c_str());
			exit(0);
		}
		
		if (_protocol != "Benchmark") {
			if (_protocol == "GetBlockTemplate") {
				std::cout << "Riecoin Core (wallet) IP: ";
				std::cin >> value;
#ifndef _WIN32
				struct sockaddr_in sa;
				if (inet_pton(AF_INET, value.c_str(), &(sa.sin_addr)) != 1) {
					std::cerr << "Invalid IP address!" << std::endl;
					std::remove(confPath.c_str());
					exit(0);
				}
				else {
					file << "Host = " << value << std::endl;
					_host = value;
				}
#else
				file << "Host = " << value << std::endl;
				_host = value;
#endif
				std::cout << "RPC port: ";
			}
			else {
				std::cout << "Current pools " << std::endl;
				std::cout << "     XPoolX: mining.xpoolx.com:5000" << std::endl;
				std::cout << "    riePool: riepool.ovh:8000" << std::endl;
				std::cout << "  uBlock.it: mine.ublock.it:5000" << std::endl;
				std::cout << "Pool address (without port): ";
				std::cin >> value;
				file << "Host = " << value << std::endl;
				_host = value;
				std::cout << "Pool port (example: 5000): ";
			}
			
			std::cin >> value;
			try {
				_port = std::stoi(value);
				file << "Port = " << value << std::endl;
			}
			catch (...) {
				std::cerr << "Invalid port !" << std::endl;
				std::remove(confPath.c_str());
				exit(0);
			}
			
			if (_protocol == "GetBlockTemplate") std::cout << "RPC username: ";
			else std::cout << "Pool username.worker: ";
			
			std::cin >> value;
			file << "User = " << value << std::endl;
			_user = value;
			
			if (_protocol == "GetBlockTemplate") std::cout << "RPC password: ";
			else std::cout << "Worker password: ";
			
			std::cin >> value;
			file << "Pass = " << value << std::endl;
			_pass = value;
			
			if (_protocol == "GetBlockTemplate") {
				std::vector<uint8_t> spk;
				std::cout << "Payout address: ";
				std::cin >> value;
				if (!addrToScriptPubKey(value, spk)) {
					std::cerr << "Invalid payout address!" << std::endl;
					std::remove(confPath.c_str());
					exit(0);
				}
				else {
					file << "Address = " << value << std::endl;
					_address = value;
				}
			}
		}
		else std::cout << "Standard Benchmark values loaded. Edit the configuration file if needed." << std::endl;
		
		std::cout << "Number of threads: ";
		std::cin >> value;
		try {
			_threads = std::stoi(value);
			file << "Threads = " << value << std::endl;
		}
		catch (...) {
			std::cerr << "Invalid thread number!" << std::endl;
			std::remove(confPath.c_str());
			exit(0);
		}
		
		if (_protocol != "Benchmark") std::cout << "Thank you, happy mining :D !" << std::endl;
		else std::cout << "Thank you :D !" << std::endl;
		std::cout << "-----------------------------------------------------------" << std::endl;
		file.close();
	}
	else std::cerr << "Unable to create " << confPath << " :|, values for standard benchmark loaded." << std::endl;
}

void Options::loadConf() {
	std::ifstream file(confPath, std::ios::in);
	std::cout << "Opening " << confPath << "..." << std::endl;
	if (file) {
		std::string line, key, value;
		while (std::getline(file, line)) {
			if (line.size() != 0) {
				parseLine(line, key, value);
				if (key == "Debug") {
					try {_debug = std::stoi(value);}
					catch (...) {_debug = 0;}
				}
				else if (key == "Host") _host = value;
				else if (key == "Port") {
					try {_port = std::stoi(value);}
					catch (...) {_port = 28332;}
				}
				else if (key == "User") _user = value;
				else if (key == "Pass") _pass = value;
				else if (key == "Threads") {
					try {_threads = std::stoi(value);}
					catch (...) {_threads = 8;}
				}
				else if (key == "SieveWorkers") {
					try {_sieveWorkers = std::stoi(value);}
					catch (...) {_sieveWorkers = 0;}
				}
				else if (key == "Sieve") {
					try {_sieve = std::stoll(value);}
					catch (...) {_sieve = 2147483648;}
					if (_sieve < 65536) _sieve = 65536;
				}
				else if (key == "Tuples") {
					try {_tuples = std::stoi(value);}
					catch (...) {_tuples = 6;}
				}
				else if (key == "Refresh") {
					try {_refresh = std::stoi(value);}
					catch (...) {_refresh = 10;}
				}
				else if (key == "Protocol") {
					if (value == "GetBlockTemplate" || value == "Stratum" || value == "Benchmark") {
						_protocol = value;
					}
					else std::cout << "Invalid Protocol" << std::endl;
				}
				else if (key == "Address") _address = value;
				else if (key == "CBMsg")   _cbMsg = value;
				else if (key == "TestDiff") {
					try {_testDiff = std::stoll(value);}
					catch (...) {_testDiff = 304;}
					if (_testDiff < 265) _testDiff = 265;
					else if (_testDiff > 32767) _testDiff = 32767;
				}
				else if (key == "TestTime") {
					try {_testTime = std::stoll(value);}
					catch (...) {_testTime = 0;}
				}
				else if (key == "Test2t") {
					try {_test2t = std::stoll(value);}
					catch (...) {_test2t = 50000;}
				}
				else if (key == "TCFile") _tcFile = value;
				else if (key == "PN") {
					try {_pn = std::stoll(value);}
					catch (...) {_pn = 40;}
				}
				else if (key == "POff") {
					for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
					std::stringstream offsets(value);
					std::vector<uint64_t> primorialOffset;
					uint64_t tmp;
					while (offsets >> tmp) primorialOffset.push_back(tmp);
					if (primorialOffset.size() < 1)
						std::cout << "Too short or invalid primorial offsets, ignoring." << std::endl;
					else _pOff = primorialOffset;
				}
				else if (key == "SieveBits") {
					try {_sieveBits = std::stoi(value);}
					catch (...) {_sieveBits = 25;}
				}
				else if (key == "ConsType") {
					for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
					std::stringstream offsets(value);
					std::vector<uint64_t> primeTupleOffset;
					uint64_t tmp;
					while (offsets >> tmp) primeTupleOffset.push_back(tmp);
					if (primeTupleOffset.size() < 2)
						std::cout << "Too short or invalid tuple offsets, ignoring." << std::endl;
					else _consType = primeTupleOffset;
				}
				else if (key == "Rules") {
					for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
					std::stringstream offsets(value);
					_rules = std::vector<std::string>();
					std::string tmp;
					while (offsets >> tmp) _rules.push_back(tmp);
				}
				else if (key == "Error") std::cout << "Ignoring invalid line" << std::endl;
				else std::cout << "Ignoring line with unused key " << key << std::endl;
			}
		}
		
		if (_tuples < 2 || _tuples > _consType.size()) _tuples = _consType.size();
		file.close();
	}
	else {
		std::cout << confPath << " not found or unreadable, please configure rieMiner now." << std::endl;
		askConf();
	}
	
	DEBUG = _debug;
	DBG(std::cout << "Debug Mode enabled" << std::endl;);
	DBG_VERIFY(std::cout << "Debug Verification enabled" << std::endl;);
	if (_protocol == "Benchmark") {
		std::cout << "Benchmark Mode at difficulty " << _testDiff << std::endl;
		if (_testTime != 0) std::cout << "Will stop after " << _testTime << " s" << std::endl;
		if (_test2t   != 0) std::cout << "Will stop after finding " << _test2t << " 2-tuples" << std::endl;
		if (_testDiff == 1600 && _sieve == 2147483648 && _test2t >= 50000 && _testTime == 0)
			std::cout << "VALID parameters for Standard Benchmark" << std::endl;
	}
	else {
		std::cout << "Host = " << _host << std::endl;
		std::cout << "Port = " << _port << std::endl;
		if (_protocol != "Stratum")
			std::cout << "User = " << _user << std::endl;
		else std::cout << "User.worker = " << _user << std::endl;
		std::cout << "Pass = ..." << std::endl;
		std::cout << "Protocol = " << _protocol << std::endl;
		if (_rules.size() > 0 && _protocol == "GetBlockTemplate") {
			std::cout << "Consensus Rules = ";
			for (std::vector<std::string>::size_type i(0) ; i < _rules.size() ; i++) {
				std::cout << _rules[i];
				if (i != _rules.size() - 1) std::cout << ", ";
			}
			std::cout << std::endl;
		}
	}
	if (_protocol == "GetBlockTemplate")
		std::cout << "Payout address = " << _address << std::endl;
	if (_threads < 2) {
		std::cout << "At least 2 threads are needed, overriding." << std::endl;
		_threads = 2;
	}
	std::cout << "Threads = " << _threads << std::endl;
	std::cout << "Sieve max = " << _sieve << std::endl;
	if (_protocol == "Benchmark")
		std::cout << "Will show tuples of at least length " << _tuples << std::endl;
	else if (_protocol != "Stratum")
		std::cout << "Will submit tuples of at least length " << _tuples << std::endl;
	else std::cout << "Will submit 4-shares" << std::endl;
	std::cout << "Stats refresh rate = " << _refresh << " s" << std::endl;
	if (_tcFile != "None") std::cout << "Tuple Count File = " << _tcFile << std::endl;
	std::cout << "Primorial Number  = " << _pn << std::endl;
	std::cout << "Primorial Offsets = " << "(";
	for (std::vector<uint64_t>::size_type i(0) ; i < _pOff.size() ; i++) {
		std::cout << _pOff[i];
		if (i != _pOff.size() - 1) std::cout << ", ";
	}
	std::cout << ")" << std::endl;
	std::cout << "Constellation Type = " << "(";
	uint64_t offsetTemp(0);
	for (std::vector<uint64_t>::size_type i(0) ; i < _consType.size() ; i++) {
		offsetTemp += _consType[i];
		if (offsetTemp == 0) std::cout << "n";
		else std::cout << "n + " << offsetTemp;
		if (i != _consType.size() - 1) std::cout << ", ";
	}
	std::cout << "), length " << _consType.size() << std::endl;
	std::cout << "Sieve bits = " <<  _sieveBits << std::endl;
}

void signalHandler(int signum) {
	std::cout << std::endl << "Signal " << signum << " received, terminating rieMiner." << std::endl;
	manager->printTuplesStats();
	manager->saveTuplesCounts();
	_exit(0);
}

int main(int argc, char** argv) {
#ifndef _WIN32
	struct sigaction SIGINTHandler;
	SIGINTHandler.sa_handler = signalHandler;
	sigemptyset(&SIGINTHandler.sa_mask);
	SIGINTHandler.sa_flags = 0;
	sigaction(SIGINT, &SIGINTHandler, NULL);
#endif
	
	std::cout << versionString << ", Riecoin miner by Pttn and contributors" << std::endl;
	std::cout << "Project page: https://github.com/Pttn/rieMiner" << std::endl;
	std::cout << "Go to project page or open README.md for usage information" << std::endl;
	std::cout << "-----------------------------------------------------------" << std::endl;
	std::cout << "GMP " << __GNU_MP_VERSION << "." << __GNU_MP_VERSION_MINOR << "." << __GNU_MP_VERSION_PATCHLEVEL << std::endl;
	std::cout << "LibCurl " << LIBCURL_VERSION << std::endl;
	std::cout << "Jansson " << JANSSON_VERSION << std::endl;
	std::cout << "-----------------------------------------------------------" << std::endl;
	
	if (argc >= 2) {
		confPath = argv[1];
		std::cout << "Using custom configuration file path " << confPath << std::endl;
	}
	
	manager = std::shared_ptr<WorkManager>(new WorkManager);
	manager->init();
	manager->manage();
	
	return 0;
}

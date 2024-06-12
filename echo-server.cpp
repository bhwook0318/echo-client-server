#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <vector> // std::vector
#include <algorithm> // std::algorithm::find
#include <mutex> // mutex

std::vector<int> clients; // for broadcast
static std::mutex m; // iter, insert, remove

void usage() {
	printf("syntax: echo-server <port> [-e[-b]]\n");
	printf("  -e : echo\n");
	printf("  -b : broadcast\n");
	printf("sample: echo-server 1234 -e -b\n");
}

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				continue;
			}
			if (strcmp(argv[i], "-b") == 0) { // is it broadcast?
				broadcast = true;
				continue;
			}
			port = atoi(argv[i]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];

	while (true) {
		// Receive data from Client
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			perror(" ");
			break;
		}
		buf[res] = '\0'; 	// last
		printf("%s", buf); 	// print!!
		fflush(stdout); 	// empty
		if (param.echo) {
			if (param.broadcast) {
				m.lock();
				for (int sd : clients) {	// for broadcast
					// Send data to Client
					res = ::send(sd, buf, res, 0);
					if (res == 0 || res == -1) {
						fprintf(stderr, "send return %ld", res);
						perror(" ");	
						// no break  : because of next msg...
					}
				}
				m.unlock();
			}
			else {
				// Send data to Client
				res = ::send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %ld", res);
					perror(" ");
					break;
				}
			}
		}
	}
	printf("disconnected\n");
	auto it = find(clients.begin(), clients.end(), sd);
	m.lock();
	clients.erase(it);	// erase iterator
	clients.clear();	// clear vector
	m.unlock();
	::close(sd); // close client socket
}

int main(int argc, char* argv[]) {
	// if the input format does not match, it is returned
	if (!param.parse(argc, argv)) { 
		usage();
		return -1;
	}

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int res;
	int optval = 1;
	res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	// Server IP
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);
	
	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		// Accept the connection when the request comes from the client
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);

		int cli_sd = ::accept(sd, (struct sockaddr *)&cli_addr, &len);
		if (cli_sd == -1) {
			perror("accept");
			break;
		}
		m.lock();
		clients.push_back(cli_sd);	// for broadcast
		m.unlock();
		std::thread* t = new std::thread(recvThread, cli_sd);
		t->detach();
	}
	::close(sd); // close server socket
}

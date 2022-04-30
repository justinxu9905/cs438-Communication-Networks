/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <string>
#include <iostream>
#include <fstream>

using namespace std;
// https://stackoverflow.com/questions/7146719/identifier-string-undefined

#define MAXDATASIZE 1000 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

    ofstream outfile;

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	string url, protocol, host, port, path;

	// parse url
    url = argv[1];
    int index = url.find_first_of("://");
    protocol = url.substr(0, index);
    string remain = url.substr(index+3);
    index = remain.find('/');
	host = remain.substr(0, index);
    path = remain.substr(index);
	port = "80";  // default port
	index = host.find(':');
    if (index != host.npos) {
        port = host.substr(index+1);
        host = host.substr(0, index);
    }
    cout << "host: " << host << endl; 
	cout << "port: " << port << endl; 
	cout << "path: " << path << endl << endl; 

	if ((rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// http get wget
	string request = "GET " + path + " HTTP/1.1\r\n"
                   + "User-Agent: Wget/1.12(linux-gnu)\r\n"
                   + "Host: " + host + ":" + port + "\r\n"
                   + "Connection: Keep-Alive\r\n\r\n";
    // send http request
    send(sockfd, request.c_str(), request.size(), 0);
	printf("client: sending request:\n%s\n", request.c_str());
    // receive http response
	outfile.open("output", ios::binary);
    int head = 1;
    while(1) {
        memset(buf, '\0', MAXDATASIZE);
		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
        // numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
        if (numbytes > 0){
			buf[numbytes] = '\0';
			printf("client: received:\n%s\n",buf);
            if (head) {
                char* data_begin = strstr(buf, "\r\n\r\n");
				// whether buf ends with "\r\n\r\n"; incase the head is very long
                if (data_begin) {
                    // outfile.write(data_begin + 4 * sizeof(char), strlen(data_begin));
					outfile.write(data_begin + 4, strlen(data_begin+4));
                    head = 0;
                }
                continue;
            }
            outfile.write(buf, sizeof(char) * numbytes);
        } else {
            outfile.close();
            break;
        }
    }
	close(sockfd);

	return 0;
}


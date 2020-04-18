//  Created by Janet Fang on 4/17/20.
//  Copyright © 2020 Janet Fang. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>     
#include <netdb.h>     
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> 
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

typedef int socket_t;

#define IP_VERISON_ANY 0
#define IP_V4 4
#define IP_V6 6

#define MIN_IP_HEADER_SIZE 20
#define MAX_IP_HEADER_SIZE 60
#define MAX_IP6_PSEUDO_HEADER_SIZE 40

#ifndef ICMP_ECHO
    #define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHO6
    #define ICMP6_ECHO 128
#endif
#ifndef ICMP_ECHO_REPLY
    #define ICMP_ECHO_REPLY 0
#endif
#ifndef ICMP_ECHO_REPLY6
    #define ICMP6_ECHO_REPLY 129
#endif


#define REQUEST_INTERVAL 1000000


#pragma pack(push, 1)

int REQUEST_TIMEOUT = 1000000;

struct ip6_pseudo_hdr {
    struct in6_addr ip6_src;
    struct in6_addr ip6_dst;
    uint32_t ip6_plen;
    uint8_t ip6_zero[3];
    uint8_t ip6_nxt;
};

#pragma pack(pop)

static uint16_t compute_checksum(const char *buf, size_t size) {
    size_t i;
    uint64_t sum = 0;
    for (i = 0; i < size; i += 2) {
        sum += *(uint16_t *)buf;
        buf += 2;
    }
    if (size - i > 0) {
        sum += *(uint8_t *)buf;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}
static void fprint_net_error(FILE *stream, const char *callee) {
    fprintf(stream, "%s: %s\n", callee, strerror(errno));
}

static uint64_t get_time(void) {
    struct timeval now;
    return gettimeofday(&now, NULL) != 0
                        ? 0
                        : now.tv_sec * 1000000 + now.tv_usec;
}

int main(int argc, char **argv) {
    char *target_host = NULL;
    int ip_version = IP_VERISON_ANY;
    int i;
    int gai_error;
    socket_t sockfd = -1;
    struct addrinfo addrinfo_hints;
    struct addrinfo *addrinfo_head = NULL;
    struct addrinfo *addrinfo = NULL;
    void *addr;
    char addrstr[INET6_ADDRSTRLEN] = "<unknown>";
    uint16_t id = (uint16_t)getpid();
    uint16_t seq;
    //loop through the arguments and check for optional arguments and host
    for (i = 1; i < argc; i++) {
        //if first char of argument is -
        if (argv[i][0] == '-') {
            //check if ipv4 is passed or not
            if (strcmp(argv[i], "-ip4") == 0) {
                //set the ip version
                ip_version = IP_V4;
            }
            //check if ipv6 is passed or not 
            else if (strcmp(argv[i], "-ip6") == 0) {
                //set the ip version
                ip_version = IP_V6;
            }
            //check if ipv6 is passed or not 
            else if (strcmp(argv[i], "-ttl") == 0) {
                //set the ip version
                REQUEST_TIMEOUT = atoi(argv[i+1]);
                i++;
            }
        } else {
            //set the target host
            target_host = argv[i];
        }
    }
    //if no target is provided, then display error and exit
    if (target_host == NULL) {
        fprintf(stderr, "Usage: ping [-ip4] [-ip6] [-ttl value] <target_host>\n");
        goto error_exit;
    }
    //init address info according to the ip version
    if (ip_version == IP_V4 || ip_version == IP_VERISON_ANY) {
        memset(&addrinfo_hints, 0, sizeof(addrinfo_hints));
        addrinfo_hints.ai_family = AF_INET;
        addrinfo_hints.ai_socktype = SOCK_RAW;
        addrinfo_hints.ai_protocol = IPPROTO_ICMP;
        gai_error = getaddrinfo(target_host,
                                NULL,
                                &addrinfo_hints,
                                &addrinfo_head);
    }
    if (ip_version == IP_V6
        || (ip_version == IP_VERISON_ANY && gai_error != 0)) {
        memset(&addrinfo_hints, 0, sizeof(addrinfo_hints));
        addrinfo_hints.ai_family = AF_INET6;
        addrinfo_hints.ai_socktype = SOCK_RAW;
        addrinfo_hints.ai_protocol = IPPROTO_ICMPV6;
        gai_error = getaddrinfo(target_host,
                                NULL,
                                &addrinfo_hints,
                                &addrinfo_head);
    }
    //if error occured in initializing the address info then display error and exit
    if (gai_error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_error));
        goto error_exit;
    }
    //initialize the socket
    for (addrinfo = addrinfo_head;
         addrinfo != NULL;
         addrinfo = addrinfo->ai_next) {
        sockfd = socket(addrinfo->ai_family,
                        addrinfo->ai_socktype,
                        addrinfo->ai_protocol);
        //if socket initalization is successful, break
        if (sockfd >= 0) {
            break;
        }
    }
    //if no socket was initializeed then show the error and end
    if (sockfd < 0) {
        fprint_net_error(stderr, "socket");
        goto error_exit;
    }
    //check for address family and initialize the addr struct
    //with the address provided 
    switch (addrinfo->ai_family) {
        case AF_INET:
            addr = &((struct sockaddr_in *)addrinfo->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            addr = &((struct sockaddr_in6 *)addrinfo->ai_addr)->sin6_addr;
            break;
    }

    inet_ntop(addrinfo->ai_family,
              addr,
              addrstr,
              sizeof(addrstr));
    //sets the file descriptor
    //if fail show error and end
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
        fprint_net_error(stderr, "fcntl");
        goto error_exit;
    }
    //loop to keep sending ping message
    for (seq = 0; ; seq++) {
        struct icmp icmp_request = {0};
        int send_result;
        char recv_buf[MAX_IP_HEADER_SIZE + sizeof(struct icmp)];
        int recv_size;
        int recv_result;
        socklen_t addrlen;
        uint8_t ip_vhl;
        uint8_t ip_header_size;
        struct icmp *icmp_response;
        uint64_t start_time;
        uint64_t delay;
        uint16_t checksum;
        uint16_t expected_checksum;
        //wait for some delay after first message
        if (seq > 0) {
            usleep(REQUEST_INTERVAL);
        }

        icmp_request.icmp_type =
            addrinfo->ai_family == AF_INET6 ? ICMP6_ECHO : ICMP_ECHO;
        icmp_request.icmp_code = 0;
        icmp_request.icmp_cksum = 0;
        icmp_request.icmp_id = htons(id);
        icmp_request.icmp_seq = htons(seq);

        switch (addrinfo->ai_family) {
            case AF_INET:
                //compute checksum for ipv4
                icmp_request.icmp_cksum =
                    compute_checksum((const char *)&icmp_request,
                                     sizeof(icmp_request));
                break;
            case AF_INET6: 
            //compute checksum for ipv6
            {
                //parse headers
                struct {
                    struct ip6_pseudo_hdr ip6_hdr;
                    struct icmp icmp;
                } data = {0};

                data.ip6_hdr.ip6_src.s6_addr[15] = 1;
                data.ip6_hdr.ip6_dst =
                    ((struct sockaddr_in6 *)&addrinfo->ai_addr)->sin6_addr;
                data.ip6_hdr.ip6_plen = htonl((uint32_t)sizeof(struct icmp));
                data.ip6_hdr.ip6_nxt = IPPROTO_ICMPV6;
                data.icmp = icmp_request;
                //compute checksum for ipv6

                icmp_request.icmp_cksum =
                    compute_checksum((const char *)&data, sizeof(data));
                break;
            }
        }
        //send the message to the socket file descriptor
        send_result = sendto(sockfd,
                             (const char *)&icmp_request,
                             sizeof(icmp_request),
                             0,
                             addrinfo->ai_addr,
                             (int)addrinfo->ai_addrlen);
        //if send failed, then display error and exit
        if (send_result < 0) {
            fprint_net_error(stderr, "sendto");
            goto error_exit;
        }
        //print the sent successfull message
        printf("Sent ICMP echo request to %s\n", addrstr);
        //receive reply size
        //depending on the ip version
        switch (addrinfo->ai_family) {
            case AF_INET:
                recv_size = (int)(MAX_IP_HEADER_SIZE + sizeof(struct icmp));
                break;
            case AF_INET6:
                recv_size = (int)sizeof(struct icmp);
                break;
        }
        //get the time at which reply recived
        start_time = get_time();

        for (;;) {
            delay = get_time() - start_time;

            addrlen = (int)addrinfo->ai_addrlen;
            //receive from sockfd file descriptor
            recv_result = recvfrom(sockfd,
                                   recv_buf,
                                   recv_size,
                                   0,
                                   addrinfo->ai_addr,
                                   &addrlen);
            //if nothing recieved 
            //then connection must be closed
            if (recv_result == 0) {
                printf("Connection closed\n");
                break;
            }
            //negative number means some error occured
            if (recv_result < 0) {
                if (errno == EAGAIN) {
                    if (delay > REQUEST_TIMEOUT) {
                        printf("Request timed out\n");
                        break;
                    } else {
                        continue;
                    }
                } else {
                    fprint_net_error(stderr, "recvfrom");
                    break;
                }
            }
            //get the header size as epr ip versiob
            switch (addrinfo->ai_family) {
                case AF_INET:
                    ip_vhl = *(uint8_t *)recv_buf;
                    ip_header_size = (ip_vhl & 0x0F) * 4;
                    break;
                case AF_INET6:
                    ip_header_size = 0;
                    break;
            }   
            //parse header of received reply
            icmp_response = (struct icmp *)(recv_buf + ip_header_size);
            icmp_response->icmp_cksum = ntohs(icmp_response->icmp_cksum);
            icmp_response->icmp_id = ntohs(icmp_response->icmp_id);
            icmp_response->icmp_seq = ntohs(icmp_response->icmp_seq);

            if (icmp_response->icmp_id == id
                && ((addrinfo->ai_family == AF_INET
                        && icmp_response->icmp_type == ICMP_ECHO_REPLY)
                    ||
                    (addrinfo->ai_family == AF_INET6
                        && (icmp_response->icmp_type != ICMP6_ECHO
                            || icmp_response->icmp_type != ICMP6_ECHO_REPLY))
                )
            ) {
                break;
            }
        }

        if (recv_result <= 0) {
            continue;
        }

        checksum = icmp_response->icmp_cksum;
        icmp_response->icmp_cksum = 0;
        //actually compute checksum
        switch (addrinfo->ai_family) {
            case AF_INET:
                expected_checksum =
                    compute_checksum((const char *)icmp_response,
                                     sizeof(*icmp_response));
                break;
            case AF_INET6: {
                struct {
                    struct ip6_pseudo_hdr ip6_hdr;
                    struct icmp icmp;
                } data = {0};
                data.ip6_hdr.ip6_plen = htonl((uint32_t)sizeof(struct icmp));
                data.ip6_hdr.ip6_nxt = IPPROTO_ICMPV6;
                data.icmp = *icmp_response;

                expected_checksum =
                    compute_checksum((const char *)&data, sizeof(data));
                break;
            }
        }
        //print recieve message
        printf("Received ICMP echo reply from %s: seq=%d, time=%.3f ms",
               addrstr,
               icmp_response->icmp_seq,
               delay / 1000.0);
        //on checksum fail print error and end
        if (checksum != expected_checksum) {
            printf(" (incorrect checksum: %x != %x)\n",
                    checksum,
                    expected_checksum);
        } else {
            printf("\n");
        }
    }

    return EXIT_SUCCESS;


error_exit:
    if (addrinfo_head != NULL) {
        freeaddrinfo(addrinfo_head);
    }
    return EXIT_FAILURE;
}

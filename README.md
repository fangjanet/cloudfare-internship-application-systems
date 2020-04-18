# Cloudflare Internship Application: Systems
Ping CLI application for MacOS or Linux

## Compile and run in terminal:
gcc -o ping ping.c \
sudo ./ping ping [-ip4] [-ip6] [-ttl value] <target_host>

## Description
The CLI app accepts a hostname or an IP address as its argument and send ICMP "echo requests" in a loop to the target while receiving "echo reply" messages. It reports loss and RTT times for each sent message.

## Features
- The CLI app accepts a hostname or an IPV4/IPV6 IP address as its argument and sends ICMP "echo requests" in a loop to the target while receiving "echo reply" messages
- Reports loss and RTT times for each sent message
- Added support for IPv4 and IPv6
- Allows user to set TTL as an optional second argument and reports the corresponding "time exceeded” ICMP message
- Error checking: checks for valid hostname, valid IPV4/IPV6 IP address, and valid ttl value

## Output
![](https://github.com/fangjanet/cloudfare-internship-application-systems/blob/master/output.png)

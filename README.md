# Cloudflare Internship Application: Systems
Ping CLI application for MacOS or Linux

## To compile and run in Linux terminal:
gcc -o myPing ping.c \
sudo ./myping \<hostname or IPV4/IPV6 IP address>

## Description
The CLI app accepts a hostname or an IP address as its argument and send ICMP "echo requests" in a loop to the target while receiving "echo reply" messages. It reports loss and RTT times for each sent message.

## Features
- The CLI app accepts a hostname or an IPV4/IPV6 IP address as its argument and sends ICMP "echo requests" in a loop to the target while receiving "echo reply" messages
- Reports loss and RTT times for each sent message
- Added support for IPv4 and IPv6
- Error checking: checks for valid hostname and valid IPV4/IPV6 IP address

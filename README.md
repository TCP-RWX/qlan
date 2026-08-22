
# Quick LAN

Quick LAN (QLAN) is a lightweight, command-line utility for easy and convenient file transfer between two computers on the same local network. This utility does 
not require you to know the IP address of or manually configure any devices involved in the data transfer. 

This program uses a UDP broadcast to discover the sender and then establishes a TCP connection to transfer data reliably.

## Demonstration video

[![qLAN Demonstration video](https://i9.ytimg.com/vi/6aB6jM5mpU0/mqdefault.jpg?sqp=CKyz8tMG-oaymwEmCMACELQB8quKqQMa8AEB-AH-CYAC0AWKAgwIABABGEYgICh_MA8=&rs=AOn4CLAvSAll4npN5nol93_a0iTv4xm8Uw)](https://www.youtube.com/watch?v=6aB6jM5mpU0)

# How it works
The device sending the data is called the sender, while the device receiving the data is the receiver.

The sender generates a 4-digit code, this code will be used later to pair the two devices.
From there, the sender will then wait for the broadcast packet sent from the receiver.

The receiver takes the 4-digit code from the user, and then uses this code to craft the broadcast message.
The Receiver then sends the UDP broadcast packet on the local network.
After sending the UDP broadcast, the receiver will wait for a connection from the sender

The sender receives this broadcast packet and then evaluates if the packet contains the right transfer code.

If the received transfer code is right, the sender will connect to the receiver over TCP and stream everything from stdin to the receiver's stdout.

# Dependencies and compilation

Dependencies:
* GCC

Steps for compilation:
1. Clone the repo
```
git clone https://github.com/TCP-RWX/qlan
```
2. Compile
```
make
```
3. Install (optional)
```
make install
```

# Usage

## Display help:
```
qlan -h
```
Example output:
```
Usage: qlan [OPTION]
Conveniently transfer infomation over LAN
        -s              Send mode
        -r [code]       Receive mode

```


## Sender mode:
```
qlan -s
```

Example output:
```
transfer code: 3141
```

## Receiver mode:
```
qlan -r 3141
```

# Examples
## Send a file
Sender:
```
cat file.txt | qlan -s
```

Receiver:
```
qlan -r 1414 > file.txt
```

## Pipe command output
Sender:
```
journalctl | qlan -s
```

Receiver:
```
qlan -r 2718
```

## Manually type data into stdin
Sender:
```
qlan -s
transfer code: 1414
Hello World!
This text is being sent over LAN!
```

Receiver:
```
qlan -r 1414
Hello World!
This text is being sent over LAN!
```

> [!CAUTION]
> Do not use this tool on networks you don't trust!
Everything transferred using this program is unencrypted and all data sent be easily be seen using a 
network packet analyzer tool such as wireshark.

# Future improvements
- [ ] IPv6 support
- [ ] TLS support
- [ ]  Multiple simultaneous transfers
- [ ] Dedicated `-f` switch for transferring files
- [ ] Better error handling
- [ ] Time sensitive transfer codes

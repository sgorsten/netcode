// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include <netcode.h>
#include <GLFW\glfw3.h>
#include <WinSock2.h>
#include <cmath>
#include <tuple>
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <iostream>

inline bool operator == (const SOCKADDR_IN & a, const SOCKADDR_IN & b) { return std::tie(a.sin_family, a.sin_addr.s_addr, a.sin_port) == std::tie(b.sin_family, b.sin_addr.s_addr, b.sin_port); }
inline bool operator != (const SOCKADDR_IN & a, const SOCKADDR_IN & b) { return std::tie(a.sin_family, a.sin_addr.s_addr, a.sin_port) != std::tie(b.sin_family, b.sin_addr.s_addr, b.sin_port); }
inline bool operator < (const SOCKADDR_IN & a, const SOCKADDR_IN & b) { return std::tie(a.sin_family, a.sin_addr.s_addr, a.sin_port) < std::tie(b.sin_family, b.sin_addr.s_addr, b.sin_port); }

struct Packet { SOCKADDR_IN addr; std::vector<char> bytes; };
std::vector<Packet> ReceiveAll(SOCKET sock);
void SendPeerMessage(NCpeer * peer, SOCKET sock, const SOCKADDR_IN & addr);

struct Protocol
{
    NCprotocol * protocol;
    NCclass * characterCl, * inputCl;
    NCint * characterPosX, * characterPosY;
    NCint * inputTargetX, * inputTargetY;

    Protocol()
    {
        protocol = ncCreateProtocol(30);

        characterCl = ncCreateClass(protocol,0);
        characterPosX = ncCreateInt(characterCl);
        characterPosY = ncCreateInt(characterCl);

        inputCl = ncCreateClass(protocol,0);
        inputTargetX = ncCreateInt(inputCl);
        inputTargetY = ncCreateInt(inputCl);    
    }
};

class Server
{
    struct Character
    {
        NCobject * object;
        float posX,posY,targetX,targetY;
    };

    struct Peer
    {
        NCpeer * peer;
        Character * player;
    };

    const Protocol & protocol;
    NCauthority * auth;
    std::list<Character> chars;
    std::map<SOCKADDR_IN, Peer> peers;
    SOCKET sock;
public:
    Server(const Protocol & protocol);

    void Update(float timestep);
};

class Client
{
    const Protocol & protocol;
    NCauthority * auth;
    NCobject * input;
    NCpeer * peer;

    SOCKET sock;
    SOCKADDR_IN addr;
    GLFWwindow * win;
public:
    Client(const Protocol & protocol, GLFWwindow * win);

    void Draw() const;
    void Update(float timestep);
};

int main() try
{
    WSADATA wsad;
    if(WSAStartup(MAKEWORD(2,0), &wsad) != 0) throw std::runtime_error("WSAStartup(...) failed.");

    const Protocol protocol;
    std::unique_ptr<Server> server;
    std::cout << "(h)ost, (j)oin, or (q)uit? ";
    switch(getchar())
    {
    case 'h': server.reset(new Server(protocol));
    case 'j': break;
    default: return EXIT_SUCCESS;
    }

    if(glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit(...) failed.");
    auto win = glfwCreateWindow(640, 480, "Simple Input", nullptr, nullptr);
    Client client(protocol, win);

    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        double t1 = glfwGetTime();
        if(server) server->Update(static_cast<float>(t1 - t0));
        client.Update(static_cast<float>(t1 - t0));
        t0 = t1;        

        glfwMakeContextCurrent(win);
        glClear(GL_COLOR_BUFFER_BIT);
        client.Draw();
        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    WSACleanup();
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << "Caught exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

////////////
// Client //
////////////

Client::Client(const Protocol & protocol, GLFWwindow * win) : protocol(protocol), win(win)
{
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == INVALID_SOCKET) throw std::runtime_error("socket(...) failed.");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(12345);

    auth = ncCreateAuthority(protocol.protocol);
    input = ncCreateObject(auth, protocol.inputCl);
    ncSetObjectInt(input, protocol.inputTargetX, 320);
    ncSetObjectInt(input, protocol.inputTargetY, 240);
    
    peer = ncCreatePeer(auth);
    ncSetVisibility(peer, input, 1);
}

void Client::Draw() const
{
    // Render client-side views of server-side objects
    glPushMatrix();
    glOrtho(0, 640, 480, 0, -1, +1);
    for(int i=0, n=ncGetViewCount(peer); i<n; ++i)
    {
        auto view = ncGetView(peer, i);
        if(ncGetViewClass(view) == protocol.characterCl)
        {
            int x = ncGetViewInt(view, protocol.characterPosX);
            int y = ncGetViewInt(view, protocol.characterPosY);
            glBegin(GL_TRIANGLE_FAN);
            for(int j=0; j<12; ++j)
            {
                float a = 6.28f * j / 12;
                glVertex2f(x + cosf(a)*10, y + sinf(a)*10);
            }
            glEnd();
        }
    }
    glPopMatrix();     
}

void Client::Update(float timestep)
{
    // Receive messages from the server
    for(auto packet : ReceiveAll(sock))
    {
        if(packet.addr != addr) continue;
        ncConsumeMessage(peer, packet.bytes.data(), packet.bytes.size());
    }
    
    // Retrieve input and publish it in client-side objects
    glfwPollEvents();
    if(glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        ncSetObjectInt(input, protocol.inputTargetX, static_cast<int>(x));
        ncSetObjectInt(input, protocol.inputTargetY, static_cast<int>(y));
    }
    ncPublishFrame(auth);

    // Send a message to the server
    SendPeerMessage(peer, sock, addr);
}

////////////
// Server //
////////////

Server::Server(const Protocol & protocol) : protocol(protocol)
{
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == INVALID_SOCKET) throw std::runtime_error("socket(...) failed.");

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(12345);
    if(bind(sock, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR) throw std::runtime_error("bind(...) failed.");

    auth = ncCreateAuthority(protocol.protocol);
}

void Server::Update(float timestep)
{
    for(auto packet : ReceiveAll(sock))
    {
        if(peers.find(packet.addr) == end(peers))
        {
            auto & newPeer = peers[packet.addr];
            newPeer.peer = ncCreatePeer(auth);

            chars.push_back({});
            auto & newChar = chars.back();
            newChar.object = ncCreateObject(auth, protocol.characterCl);
            newChar.posX = newChar.targetX = 320;
            newChar.posY = newChar.targetY = 240;
            newPeer.player = &newChar;
        }

        ncConsumeMessage(peers[packet.addr].peer, packet.bytes.data(), packet.bytes.size());
    }

    // Handle user input
    for(auto & p : peers)
    {
        auto & peer = p.second;
        for(int i=0, n=ncGetViewCount(peer.peer); i<n; ++i)
        {
            auto view = ncGetView(peer.peer, i);
            if(ncGetViewClass(view) == protocol.inputCl)
            {
                peer.player->targetX = static_cast<float>(ncGetViewInt(view, protocol.inputTargetX));
                peer.player->targetY = static_cast<float>(ncGetViewInt(view, protocol.inputTargetY));
            }
        }
    }

    // Update game state
    if(timestep > 0)
    {
        for(auto & c : chars)
        {
            float maxDist = timestep * 200;
            float dx = c.targetX - c.posX, dy = c.targetY - c.posY;
            float len = sqrtf(dx*dx + dy*dy);
            if(len < maxDist)
            {
                c.posX = c.targetX;
                c.posY = c.targetY;
            }
            else
            {
                c.posX += dx*maxDist/len;
                c.posY += dy*maxDist/len;
            }        

            ncSetObjectInt(c.object, protocol.characterPosX, static_cast<int>(c.posX));
            ncSetObjectInt(c.object, protocol.characterPosY, static_cast<int>(c.posY));

            for(auto & p : peers) ncSetVisibility(p.second.peer, c.object, 1);
        }
        ncPublishFrame(auth);
    }

    // Send a message to the clients
    for(auto & p : peers)
    {
        SendPeerMessage(p.second.peer, sock, p.first);
    }
}

///////////////
// Utilities //
///////////////

std::vector<Packet> ReceiveAll(SOCKET sock)
{
    std::vector<Packet> packets;
    while(1)
    {
        struct timeval tv = {0,0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        if(select(1, &fds, NULL, NULL, &tv) == SOCKET_ERROR) throw std::runtime_error("select(...) failed.");
        if(!FD_ISSET(sock, &fds)) return packets;

        char buffer[2000];
        SOCKADDR_IN remoteAddr;
        int remoteLen = sizeof(remoteAddr);
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
        if(len == SOCKET_ERROR) throw std::runtime_error("recvfrom(...) failed.");
        packets.push_back({remoteAddr, {buffer,buffer+len}});
    }
}

void SendPeerMessage(NCpeer * peer, SOCKET sock, const SOCKADDR_IN & addr)
{
    auto message = ncProduceMessage(peer);
    sendto(sock, (const char *)ncGetBlobData(message), ncGetBlobSize(message), 0, (const SOCKADDR *)&addr, sizeof(addr));
    ncFreeBlob(message);
}
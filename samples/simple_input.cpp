#include <netcode.h>
#include <GLFW\glfw3.h>
#include <WinSock2.h>
#include <cmath>
#include <iostream>

struct Protocol
{
    NCprotocol * protocol;
    NCclass * characterCl, * inputCl;
    NCint * characterPosX, * characterPosY;
    NCint * inputTargetX, * inputTargetY;

    Protocol()
    {
        protocol = ncCreateProtocol(30);

        characterCl = ncCreateClass(protocol);
        characterPosX = ncCreateInt(characterCl);
        characterPosY = ncCreateInt(characterCl);

        inputCl = ncCreateClass(protocol);
        inputTargetX = ncCreateInt(inputCl);
        inputTargetY = ncCreateInt(inputCl);    
    }
};

struct Server
{
    const Protocol & protocol;
    NCauthority * auth;
    NCobject * player;
    NCpeer * peer;
    
    SOCKET sock;
    SOCKADDR_IN clientAddr;

    float posX,posY, targetX,targetY;

    Server(const Protocol & protocol) : protocol(protocol)
    {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(sock == INVALID_SOCKET) throw std::runtime_error("socket(...) failed.");

        SOCKADDR_IN addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(12345);
        if(bind(sock, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR) throw std::runtime_error("bind(...) failed.");

        posX=targetX=320;
        posY=targetY=240;

        auth = ncCreateAuthority(protocol.protocol);
        player = ncCreateObject(auth, protocol.characterCl);
    
        peer = ncCreatePeer(auth);
        ncSetVisibility(peer, player, 1);

        clientAddr = {};
    }

    void Update(float timestep)
    {
        // Consume any incoming messages
        while(1)
        {
            struct timeval tv = {0,0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            if(select(1, &fds, NULL, NULL, &tv) == SOCKET_ERROR) throw std::runtime_error("select(...) failed.");
            if(!FD_ISSET(sock, &fds)) break;

            char buffer[2000];
            SOCKADDR_IN remoteAddr;
            int remoteLen = sizeof(remoteAddr);
            int len = recvfrom(sock, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
            if(len == SOCKET_ERROR) throw std::runtime_error("recvfrom(...) failed.");
            clientAddr = remoteAddr;
            ncConsumeMessage(peer, buffer, len);
        }

        // Handle user input
        for(int i=0, n=ncGetViewCount(peer); i<n; ++i)
        {
            auto view = ncGetView(peer, i);
            if(ncGetViewClass(view) == protocol.inputCl)
            {
                targetX = static_cast<float>(ncGetViewInt(view, protocol.inputTargetX));
                targetY = static_cast<float>(ncGetViewInt(view, protocol.inputTargetY));
            }
        }

        // Update game state
        if(timestep > 0)
        {
            float maxDist = timestep * 200;
            float dx = targetX - posX, dy = targetY - posY;
            float len = sqrtf(dx*dx + dy*dy);
            if(len < maxDist)
            {
                posX = targetX;
                posY = targetY;
            }
            else
            {
                posX += dx*maxDist/len;
                posY += dy*maxDist/len;
            }        

            ncSetObjectInt(player, protocol.characterPosX, static_cast<int>(posX));
            ncSetObjectInt(player, protocol.characterPosY, static_cast<int>(posY));
            ncPublishFrame(auth);

            // Send a message to the client
            if(clientAddr.sin_addr.s_addr != 0)
            {
                auto message = ncProduceMessage(peer);
                sendto(sock, (const char *)ncGetBlobData(message), ncGetBlobSize(message), 0, (const SOCKADDR *)&clientAddr, sizeof(clientAddr));
                ncFreeBlob(message);
            }
        }
    }
};

struct Client
{
    const Protocol & protocol;
    NCauthority * auth;
    NCobject * input;
    NCpeer * peer;

    SOCKET sock;
    SOCKADDR_IN addr;

    Client(const Protocol & protocol) : protocol(protocol)
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

    void Update(GLFWwindow * win)
    {
        // Consume any incoming messages
        while(1)
        {
            struct timeval tv = {0,0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            if(select(1, &fds, NULL, NULL, &tv) == SOCKET_ERROR) throw std::runtime_error("select(...) failed.");
            if(!FD_ISSET(sock, &fds)) break;

            char buffer[2000];
            SOCKADDR_IN remoteAddr;
            int remoteLen = sizeof(remoteAddr);
            int len = recvfrom(sock, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
            if(len == SOCKET_ERROR) throw std::runtime_error("recvfrom(...) failed.");
            ncConsumeMessage(peer, buffer, len);
        }

        // Render client-side views of server-side objects
        glClear(GL_COLOR_BUFFER_BIT);
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
        glfwSwapBuffers(win);    
    
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
        auto message = ncProduceMessage(peer);
        sendto(sock, (const char *)ncGetBlobData(message), ncGetBlobSize(message), 0, (const SOCKADDR *)&addr, sizeof(addr));
        ncFreeBlob(message);
    }
};

int main() try
{
    WSADATA wsad;
    if(WSAStartup(MAKEWORD(2,0), &wsad) != 0) throw std::runtime_error("WSAStartup(...) failed.");
    if(glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit(...) failed.");
    auto win = glfwCreateWindow(640, 480, "Simple Input", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    const Protocol protocol;
    Server server(protocol);
    Client client(protocol);

    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        double t1 = glfwGetTime();
        server.Update(static_cast<float>(t1 - t0));
        t0 = t1;

        client.Update(win);
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
#include <netcode.h>
#include <GLFW/glfw3.h>
#include <WinSock2.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MAX_PEERS 16

/* Protocol data */
struct NCclass * nunit;

/* Server state */
struct Unit
{
    int team, hp;
    float x, y;
    struct NCobject * nobj;
} units[20];
struct NCserver * nserver;

SOCKET serverSocket;
SOCKADDR_IN peerAddrs[MAX_PEERS];
struct NCpeer * npeers[MAX_PEERS];

/* Client state */
SOCKET clientSocket;
struct NCclient * nclient;
GLFWwindow * win;

void error(const char * message)
{
    fprintf(stderr, message);
    exit(EXIT_FAILURE);
}

void spawn_unit(int i)
{
    units[i].team = i < 10 ? 0 : 1;
    units[i].hp = 100;
    units[i].x = rand() % 320 + units[i].team * 960;
    units[i].y = rand() % 720;
    units[i].nobj = ncCreateObject(nserver, nunit);
}

int main(int argc, char * argv[])
{
    int i, j, n, x, y, h; double a, t0, t1, timestep; 
    struct NCblob * nupdate, * nresponse; struct NCview * nview;
    WSADATA wsad;

    WSAStartup(MAKEWORD(2,0), &wsad);

    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(serverSocket == INVALID_SOCKET) error("socket(...) failed.");

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12345);
    if(bind(serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) error("bind(...) failed.");

    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(clientSocket == INVALID_SOCKET) error("socket(...) failed.");

    SOCKADDR_IN clientServerAddr;
    clientServerAddr.sin_family = AF_INET;
    clientServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientServerAddr.sin_port = htons(12345);

    char buffer[] = "Hello server!";
    sendto(clientSocket, buffer, sizeof(buffer), 0, (const SOCKADDR *)&clientServerAddr, sizeof(clientServerAddr));

    char recvbuf[2000];
    SOCKADDR_IN serverClientAddr;
    int len = sizeof(serverClientAddr);
    int recvLen = recvfrom(serverSocket, recvbuf, sizeof(recvbuf), 0, (SOCKADDR *)&serverClientAddr, &len);
    recvbuf[recvLen] = 0;
    printf("Received %s\n", recvbuf);

    nunit = ncCreateClass(4); /* team, hp, x, y */

    /* init server */
    nserver = ncCreateServer(&nunit, 1, 30);
    for(i=0; i<20; ++i) spawn_unit(i);
    npeers[0] = ncCreatePeer(nserver);
    
    /* init client */
    nclient = ncCreateClient(&nunit, 1, 30);    
    if (glfwInit() != GL_TRUE) error("glfwInit() failed.");
	win = glfwCreateWindow(1280, 720, "Simple Game", NULL, NULL);
	if (!win) error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

    /* main loop */
	t0 = glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
		glfwPollEvents();
		t1 = glfwGetTime(), timestep = t1 - t0;
		if (timestep < 1.0 / 60) continue;
		t0 = t1;

        /* for each game unit on server */
        for(i=0; i<20; ++i)
        {
            /* select a target */
            int target=0; float dx, dy, dist, best=HUGE_VALF;
            for(j=0; j<20; ++j)
            {
                if(units[i].team == units[j].team) continue;
                dx = units[j].x - units[i].x;
                dy = units[j].y - units[i].y;
                dist = dx*dx + dy*dy;
                if(dist < best)
                {
                    target = j;
                    best = dist;
                }
            }

            /* pursure target */
            dx = units[target].x - units[i].x;
            dy = units[target].y - units[i].y;
            dist = sqrtf(dx*dx + dy*dy);
            if(dist > 50) /* if far, move towards target */
            {
                units[i].x += dx * 50 * timestep / dist;
                units[i].y += dy * 50 * timestep / dist;
            }
            else /* if near, attack target */
            {
                --units[target].hp;
                if(units[target].hp < 1) /* if target is dead, destroy and respawn */
                {
                    ncDestroyObject(units[target].nobj);
                    spawn_unit(target);
                }
            }
            
            /* update corresponding netcode object */
            ncSetObjectInt(units[i].nobj, 0, units[i].team);
            ncSetObjectInt(units[i].nobj, 1, units[i].hp);
            ncSetObjectInt(units[i].nobj, 2, (int)units[i].x);
            ncSetObjectInt(units[i].nobj, 3, (int)units[i].y);
            ncSetVisibility(npeers[0], units[i].nobj, 1); /* for now, all units are always visible, but we could implement a "fog of war" by manipulating this */
        }
        ncPublishFrame(nserver);

        /* check for incoming data on any sockets */
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serverSocket, &fds);
        FD_SET(clientSocket, &fds);
        if(select(2, &fds, NULL, NULL, &tv) == SOCKET_ERROR) error("select(...) failed.");
        if(FD_ISSET(serverSocket, &fds))
        {
            char buffer[2000];
            SOCKADDR_IN remoteAddr;
            int remoteLen = sizeof(remoteAddr);
            int len = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
            if(recvfrom == SOCKET_ERROR) error("recvfrom(...) failed.");
            peerAddrs[0] = remoteAddr;
            ncConsumeResponse(npeers[0], buffer, len);
        }
        if(FD_ISSET(clientSocket, &fds))
        {
            char buffer[2000];
            SOCKADDR_IN remoteAddr;
            int remoteLen = sizeof(remoteAddr);
            int len = recvfrom(clientSocket, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
            if(recvfrom == SOCKET_ERROR) error("recvfrom(...) failed.");
            ncConsumeUpdate(nclient, buffer, len);
        }

        if(peerAddrs[0].sin_port)
        {
            nupdate = ncProduceUpdate(npeers[0]);
            sendto(serverSocket, (const char *)ncGetBlobData(nupdate), ncGetBlobSize(nupdate), 0, (const SOCKADDR *)&peerAddrs[0], sizeof(peerAddrs[0]));
            ncFreeBlob(nupdate);
        }

        nresponse = ncProduceResponse(nclient);
        sendto(clientSocket, (const char *)ncGetBlobData(nresponse), ncGetBlobSize(nresponse), 0, (const SOCKADDR *)&clientServerAddr, sizeof(clientServerAddr));
        ncConsumeResponse(npeers[0], ncGetBlobData(nresponse), ncGetBlobSize(nresponse));

        /* redraw client */        
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
        for(i=0, n=ncGetViewCount(nclient); i<n; ++i)
        {
            nview = ncGetView(nclient, i);
            if(ncGetViewClass(nview) == nunit)
            {
                /* draw colored circle to represent unit */
                x = ncGetViewInt(nview, 2);
                y = ncGetViewInt(nview, 3);
                glBegin(GL_TRIANGLE_FAN);
                switch(ncGetViewInt(nview, 0))
                {
                case 0: glColor3f(0,1,1); break;
                case 1: glColor3f(1,0,0); break;
                }
                for(j=0; j<12; ++j)
                {
                    a = 6.28 * j / 12;
                    glVertex2d(x + cos(a)*10, y + sin(a)*10);
                }
                glEnd();
                
                /* draw unit health bar */
                h = ncGetViewInt(nview, 1);
                glBegin(GL_QUADS);
                glColor3f(0,1,0);
                glVertex2i(x-10, y-13);
                glVertex2i(x-10+(h*20/100), y-13);
                glVertex2i(x-10+(h*20/100), y-11);
                glVertex2i(x-10, y-11);
                glColor3f(0.5f,0.2f,0);
                glVertex2i(x-10+(h*20/100), y-13);
                glVertex2i(x+10, y-13);
                glVertex2i(x+10, y-11);
                glVertex2i(x-10+(h*20/100), y-11);
                glEnd();
            }
        }
        glPopMatrix();
		glfwSwapBuffers(win);
	}

	glfwDestroyWindow(win);
	glfwTerminate();
    WSACleanup();
    return EXIT_SUCCESS;
}

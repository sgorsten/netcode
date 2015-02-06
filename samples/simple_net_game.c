#include <netcode.h>
#include <GLFW/glfw3.h>
#include <WinSock2.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

void error(const char * message);

struct Server * CreateServer(int port);
void UpdateServer(struct Server * s, float timestep);
void DestroyServer(struct Server * s);

struct Client * CreateClient(const char * ip, int port);
void UpdateClient(struct Client * c);
int IsClientFinished(struct Client * c);
void DestroyClient(struct Client * c);

struct NCclass * nteam;
struct NCclass * nunit;

int main(int argc, char * argv[])
{
    double t0, t1, timestep; WSADATA wsad;
    struct Server * server=0; struct Client * client=0;

    if(WSAStartup(MAKEWORD(2,0), &wsad) != 0) error("WSAStartup(...) failed.");
    if(glfwInit() != GL_TRUE) error("glfwInit() failed.");

    nteam = ncCreateClass(1); /* team */
    nunit = ncCreateClass(4); /* team, hp, x, y */
    printf("(h)ost, (j)oin, or (q)uit?\n");
    switch(getchar())
    {
    case 'h': server = CreateServer(12345);
    case 'j': client = CreateClient("127.0.0.1", 12345); break;
    default: return EXIT_SUCCESS;
    }

	t0 = glfwGetTime();
	while(!IsClientFinished(client))
	{
		glfwPollEvents();
		t1 = glfwGetTime(), timestep = t1 - t0;
		if(timestep < 1.0 / 60) continue;
		t0 = t1;

        if(server) UpdateServer(server, (float)timestep);
        UpdateClient(client);        
	}

    DestroyClient(client);
    if(server) DestroyServer(server);

	glfwTerminate();
    WSACleanup();
    return EXIT_SUCCESS;
}

void error(const char * message)
{
    fprintf(stderr, message);
    exit(EXIT_FAILURE);
}

/**********
 * Server *
 **********/

#define MAX_PEERS 16

struct Unit
{
    int team, hp;
    float x, y;
    struct NCobject * nobj;
};

struct Peer
{
    SOCKADDR_IN addr;
    struct NCpeer * npeer;
};

struct Server
{
    struct Unit units[20];
    struct NCserver * nserver;
    struct NCobject * nteams[2];

    SOCKET serverSocket;
    struct Peer peers[MAX_PEERS];
    int numPeers;
};

void SpawnUnit(struct Server * s, int i)
{
    s->units[i].team = i < 10 ? 0 : 1;
    s->units[i].hp = 100;
    s->units[i].x = rand() % (WINDOW_WIDTH/4) + s->units[i].team * (WINDOW_WIDTH*3/4);
    s->units[i].y = rand() % WINDOW_HEIGHT;
    s->units[i].nobj = ncCreateObject(s->nserver, nunit);
}

struct Server * CreateServer(int port)
{
    int i; struct Server * s;

    s = malloc(sizeof(struct Server));
    memset(s, 0, sizeof(struct Server));

    s->serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s->serverSocket == INVALID_SOCKET) error("socket(...) failed.");

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    if(bind(s->serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) error("bind(...) failed.");

    struct NCclass * classes[] = {nteam, nunit};
    s->nserver = ncCreateServer(classes, 2, 30);

    for(i=0; i<2; ++i)
    {
        s->nteams[i] = ncCreateObject(s->nserver, nteam);
        ncSetObjectInt(s->nteams[i], 0, i);
    }

    for(i=0; i<20; ++i) SpawnUnit(s, i);

    return s;
}

void DestroyServer(struct Server * s)
{
    closesocket(s->serverSocket);
    free(s);
}

void UpdateServer(struct Server * s, float timestep)
{
    int i, j; struct timeval tv; fd_set fds;
    
    /* read any incoming messages on our socket */
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    FD_ZERO(&fds);
    FD_SET(s->serverSocket, &fds);
    if(select(1, &fds, NULL, NULL, &tv) == SOCKET_ERROR) error("select(...) failed.");
    if(FD_ISSET(s->serverSocket, &fds))
    {
        char buffer[2000];
        SOCKADDR_IN remoteAddr;
        int remoteLen = sizeof(remoteAddr);
        int len = recvfrom(s->serverSocket, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
        if(len == SOCKET_ERROR) error("recvfrom(...) failed.");

        for(i=0; i<s->numPeers; ++i)
        {
            if(remoteAddr.sin_addr.S_un.S_addr == s->peers[i].addr.sin_addr.S_un.S_addr && remoteAddr.sin_port == s->peers[i].addr.sin_port)
            {
                ncConsumeResponse(s->peers[i].npeer, buffer, len);
                break;
            }
        }
        if(i == s->numPeers && i < MAX_PEERS)
        {
            s->peers[i].npeer = ncCreatePeer(s->nserver);
            ncSetVisibility(s->peers[i].npeer, s->nteams[i%2], 1);
            ncConsumeResponse(s->peers[i].npeer, buffer, len);
            s->peers[i].addr = remoteAddr;
            ++s->numPeers;
        }
    }

    /* for each game unit on server */
    for(i=0; i<20; ++i)
    {
        /* select a target */
        int target=0; float dx, dy, dist, best=HUGE_VALF;
        for(j=0; j<20; ++j)
        {
            if(s->units[i].team == s->units[j].team) continue;
            dx = s->units[j].x - s->units[i].x;
            dy = s->units[j].y - s->units[i].y;
            dist = dx*dx + dy*dy;
            if(dist < best)
            {
                target = j;
                best = dist;
            }
        }

        /* pursue target */
        dx = s->units[target].x - s->units[i].x;
        dy = s->units[target].y - s->units[i].y;
        dist = sqrtf(dx*dx + dy*dy);
        if(dist > 50) /* if far, move towards target */
        {
            s->units[i].x += dx * 50 * timestep / dist;
            s->units[i].y += dy * 50 * timestep / dist;
        }
        else /* if near, attack target */
        {
            --s->units[target].hp;
            if(s->units[target].hp < 1) /* if target is dead, destroy and respawn */
            {
                ncDestroyObject(s->units[target].nobj);
                SpawnUnit(s, target);
            }
        }
    }

    /* publish next frame */
    for(i=0; i<20; ++i)
    {
        /* set state for each object */
        ncSetObjectInt(s->units[i].nobj, 0, s->units[i].team);
        ncSetObjectInt(s->units[i].nobj, 1, s->units[i].hp);
        ncSetObjectInt(s->units[i].nobj, 2, (int)s->units[i].x);
        ncSetObjectInt(s->units[i].nobj, 3, (int)s->units[i].y);

        /* determine if unit is visible to other team */
        int isVisible = 0;
        for(j=0; j<20; ++j)
        {
            float dx, dy;
            if(s->units[i].team == s->units[j].team) continue;
            dx = s->units[j].x - s->units[i].x;
            dy = s->units[j].y - s->units[i].y;
            if(dx*dx + dy*dy < 120*120)
            {
                isVisible = 1;
                break;
            }
        }

        /* units are visible to their own team, and within 120 units of the other team's units */
        for(j=0; j<s->numPeers; ++j)
        {
            ncSetVisibility(s->peers[j].npeer, s->units[i].nobj, s->units[i].team == j%2 || isVisible);
        }
    }
    ncPublishFrame(s->nserver);

    /* send updates to peers */
    for(j=0; j<s->numPeers; ++j) 
    {
        struct NCblob * nupdate = ncProduceUpdate(s->peers[j].npeer);
        sendto(s->serverSocket, (const char *)ncGetBlobData(nupdate), ncGetBlobSize(nupdate), 0, (const SOCKADDR *)&s->peers[j].addr, sizeof(s->peers[j].addr));
        ncFreeBlob(nupdate);
    }
}

/**********
 * Client *
 **********/

struct Client
{
    SOCKET clientSocket;
    SOCKADDR_IN serverAddr;
    struct NCclient * nclient;
    GLFWwindow * win;
    int team;
};

struct Client * CreateClient(const char * ip, int port)
{
    struct Client * c;

    c = malloc(sizeof(struct Client));
    memset(c, 0, sizeof(struct Client));

    c->clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(c->clientSocket == INVALID_SOCKET) error("socket(...) failed.");

    c->serverAddr.sin_family = AF_INET;
    c->serverAddr.sin_addr.s_addr = inet_addr(ip);
    c->serverAddr.sin_port = htons(port);

    struct NCclass * classes[] = {nteam, nunit};
    c->nclient = ncCreateClient(classes, 2, 30);    
	c->win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Simple Game", NULL, NULL);
	if (!c->win) error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(c->win);

    return c;
}

void DestroyClient(struct Client * s)
{
    closesocket(s->clientSocket);
    free(s);
}

int IsClientFinished(struct Client * c)
{
    return glfwWindowShouldClose(c->win);
}

void DrawCircle(int x, int y, int radius)
{
    int j; float a;
    glBegin(GL_TRIANGLE_FAN);
    for(j=0; j<24; ++j)
    {
        a = 6.28f * j / 24;
        glVertex2f(x + cosf(a)*radius, y + sinf(a)*radius);
    }
    glEnd();
}

void UpdateClient(struct Client * c)
{
    int i, j, n, x, y, h; float a;

    /* check for incoming data on any sockets */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(c->clientSocket, &fds);
    if(select(1, &fds, NULL, NULL, &tv) == SOCKET_ERROR) error("select(...) failed.");
    if(FD_ISSET(c->clientSocket, &fds))
    {
        char buffer[2000];
        SOCKADDR_IN remoteAddr;
        int remoteLen = sizeof(remoteAddr);
        int len = recvfrom(c->clientSocket, buffer, sizeof(buffer), 0, (SOCKADDR *)&remoteAddr, &remoteLen);
        if(len == SOCKET_ERROR) error("recvfrom(...) failed.");
        ncConsumeUpdate(c->nclient, buffer, len);
    }

    struct NCblob * nresponse = ncProduceResponse(c->nclient);
    sendto(c->clientSocket, (const char *)ncGetBlobData(nresponse), ncGetBlobSize(nresponse), 0, (const SOCKADDR *)&c->serverAddr, sizeof(c->serverAddr));

    /* determine team */
    for(i=0, n=ncGetViewCount(c->nclient); i<n; ++i)
    {
        struct NCview * nview = ncGetView(c->nclient, i);
        if(ncGetViewClass(nview) == nteam) c->team = ncGetViewInt(nview, 0);
    }

    /* redraw client */        
	glClear(GL_COLOR_BUFFER_BIT);
	glPushMatrix();
	glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, +1);

    /* draw unit visibility */
    glColor3f(0.5f,0.3f,0.1f);
    for(i=0, n=ncGetViewCount(c->nclient); i<n; ++i)
    {
        struct NCview * nview = ncGetView(c->nclient, i);
        if(ncGetViewClass(nview) == nunit && ncGetViewInt(nview, 0) == c->team)
        {
            DrawCircle(ncGetViewInt(nview, 2), ncGetViewInt(nview, 3), 120);
        }
    }

    /* draw units */
    for(i=0, n=ncGetViewCount(c->nclient); i<n; ++i)
    {
        struct NCview * nview = ncGetView(c->nclient, i);
        if(ncGetViewClass(nview) == nunit)
        {
            /* draw colored circle to represent unit */
            x = ncGetViewInt(nview, 2);
            y = ncGetViewInt(nview, 3);
            switch(ncGetViewInt(nview, 0))
            {
            case 0: glColor3f(0,1,1); break;
            case 1: glColor3f(1,0,0); break;
            }
            DrawCircle(x, y, 10);
                
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
	glfwSwapBuffers(c->win);
}

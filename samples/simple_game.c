#include <netcode.h>
#include <netcodex.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Protocol data */
struct NCprotocol * protocol;
struct NCclass * unitClass;
struct NCint * teamField, * hpField, * xField, * yField;

/* Server state */
struct Unit
{
    int team, hp;
    float x, y;
    struct NCobject * object;
} units[20];
struct NCserver * server;
struct NCpeer * peer;

/* Client state */
struct NCclient * client;
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
    units[i].object = ncCreateObject(server, unitClass);
}

int main(int argc, char * argv[])
{
    int i, j, n, x, y, h; double a, t0, t1, timestep; 
    struct NCblob * updateBlob, * responseBlob; struct NCview * view;

    /* init protocol */
    protocol = ncCreateProtocol(30);
    unitClass = ncCreateClass(protocol);
    teamField = ncCreateInt(unitClass);
    hpField = ncCreateInt(unitClass);
    xField = ncCreateInt(unitClass);
    yField = ncCreateInt(unitClass);

    /* init server */
    server = ncCreateServer(protocol);
    for(i=0; i<20; ++i) spawn_unit(i);
    peer = ncCreatePeer(server);
    
    /* init client */
    client = ncCreateClient(protocol);
    if (glfwInit() != GL_TRUE) error("glfwInit() failed.");
	win = glfwCreateWindow(1280, 720, "Simple Game", NULL, NULL);
	if (!win) error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

    /* main loop */
	t0 = glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
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
                    ncDestroyObject(units[target].object);
                    spawn_unit(target);
                }
            }
            
            /* update corresponding netcode object */
            ncSetObjectInt(units[i].object, teamField, units[i].team);
            ncSetObjectInt(units[i].object, hpField, units[i].hp);
            ncSetObjectInt(units[i].object, xField, (int)units[i].x);
            ncSetObjectInt(units[i].object, yField, (int)units[i].y);
            ncSetVisibility(peer, units[i].object, 1); /* for now, all units are always visible, but we could implement a "fog of war" by manipulating this */
        }
        ncPublishFrame(server);

        /* simulate network traffic */
        updateBlob = ncProduceUpdate(peer);
        if(rand() % 100 > 20) /* simulate 20% packet loss, in a real app, blob would be transmitted from server to client via UDP */
        {
            ncConsumeUpdate(client, ncGetBlobData(updateBlob), ncGetBlobSize(updateBlob));

            responseBlob = ncProduceResponse(client);
            if(rand() % 100 > 20) /* simulate 20% packet loss, in a real app, blob would be transmitted from client to server via UDP */
            {
                ncConsumeResponse(peer, ncGetBlobData(responseBlob), ncGetBlobSize(responseBlob));
            }
            ncFreeBlob(responseBlob);
        }
        printf("Update size: %d B\n", ncGetBlobSize(updateBlob));
        ncFreeBlob(updateBlob);

        /* redraw client */        
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
        for(i=0, n=ncGetViewCount(client); i<n; ++i)
        {
            view = ncGetView(client, i);
            if(ncGetViewClass(view) == unitClass)
            {
                /* draw colored circle to represent unit */
                x = ncGetViewInt(view, xField);
                y = ncGetViewInt(view, yField);
                glBegin(GL_TRIANGLE_FAN);
                switch(ncGetViewInt(view, teamField))
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
                h = ncGetViewInt(view, hpField);
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

        glfwPollEvents();
	}

    ncxPrintClientCodeEfficiency(client);

	glfwDestroyWindow(win);
	glfwTerminate();
    return EXIT_SUCCESS;
}